#include "moksha/Ownership/ARCInserter.h"
#include "moksha/HIR/HIRType.h"
#include "moksha/MIR/MIRArgument.h"
#include "moksha/MIR/MIRBlock.h"
#include "moksha/MIR/MIRFunction.h"
#include "moksha/MIR/MIRGlobal.h"
#include "moksha/MIR/MIRInst.h"
#include "moksha/MIR/MIRModule.h"
#include "moksha/Support/Diagnostics.h"
#include "llvm/Support/Casting.h"
#include <unordered_set>
#include <vector>

namespace moksha {
namespace mir {

namespace {

class ARCInserter {
public:
  ARCInserter(MIRModule *module, DiagnosticEngine &diags)
      : module(module), diags(diags) {}

  bool run() {
    bool modified = false;
    for (auto &func : module->getFunctions()) {
      modified |= runOnFunction(func);
    }

    // Global Teardown (Module Destroy)
    MIRFunction *destroyFunc = module->getFunction("__moksha_module_destroy");

    // Synthesize the teardown function if the frontend didn't generate one
    if (!destroyFunc) {
      auto fn = std::make_unique<MIRFunction>(
          nullptr, "__moksha_module_destroy", Linkage::External);
      destroyFunc = fn.get();
      module->addFunction(std::move(fn));

      auto block = std::make_unique<MIRBlock>("entry", destroyFunc);
      block->getInstructionsMut().push_back(
          std::make_unique<ReturnInst>(nullptr, SourceLocation()));
      destroyFunc->addBlock(std::move(block));
    }

    if (!destroyFunc->getBlocks().empty()) {
      MIRBlock *entry = destroyFunc->getEntryBlock();
      auto retIt = entry->getInstructionsMut().end();
      for (auto it = entry->getInstructionsMut().begin();
           it != entry->getInstructionsMut().end(); ++it) {
        if (llvm::isa<ReturnInst>(it->get())) {
          retIt = it;
          break;
        }
      }

      for (auto &globalPtr : module->getGlobalsMut()) {
        MIRGlobal *g = globalPtr.get();

        if (g->isConstant()) {
          continue;
        }

        // Check if the global's underlying type requires ARC
        const hir::HIRType *valTy = nullptr;
        if (auto *ptrTy =
                llvm::dyn_cast_or_null<hir::PointerType>(g->getType())) {
          valTy = ptrTy->getPointee();
        } else {
          valTy = g->getType();
        }

        if (valTy && isRefCounted(valTy)) {
          // 1. Load the global
          auto loadInst = std::make_unique<LoadInst>(
              g, g->getName() + ".cleanup", SourceLocation());
          loadInst->setParent(entry);
          MIRValue *loadedVal = loadInst.get();

          // 2. Release it
          auto releaseInst = std::make_unique<ARCInst>(
              Opcode::Release, loadedVal, getDropFunc(loadedVal),
              SourceLocation());
          releaseInst->setParent(entry);
          retIt =
              entry->getInstructionsMut().insert(retIt, std::move(loadInst));
          ++retIt;
          retIt =
              entry->getInstructionsMut().insert(retIt, std::move(releaseInst));
          modified = true;
        }
      }
    }
    return modified;
  }

private:
  MIRModule *module;
  DiagnosticEngine &diags;

  MIRFunction *getDropFunc(MIRValue *val) {
    if (!val)
      return nullptr;
    const hir::HIRType *valTy = val->getType();
    if (auto *nullableTy =
            llvm::dyn_cast_or_null<hir::HIRNullableType>(valTy)) {
      valTy = nullableTy->getInner();
    }
    if (auto *pTy = llvm::dyn_cast_or_null<hir::PointerType>(valTy)) {
      valTy = pTy->getPointee();
    }
    if (valTy) {
      std::string typeName = valTy->toString();

      auto removePrefix = [&](const std::string &prefix) {
        if (typeName.find(prefix) == 0)
          typeName = typeName.substr(prefix.length());
      };

      while (!typeName.empty() && (typeName[0] == '&' || typeName[0] == '*' ||
                                   typeName[0] == ' ' || typeName[0] == '?')) {
        typeName = typeName.substr(1);
      }

      removePrefix("shared ");
      removePrefix("owned ");
      removePrefix("weak ");
      removePrefix("mut ");
      removePrefix("view ");
      removePrefix("lock ");
      removePrefix("struct ");
      removePrefix("class ");

      size_t arcPos = typeName.find("Arc<");
      size_t boxPos = typeName.find("Box<");
      size_t startPos =
          (arcPos != std::string::npos)
              ? arcPos
              : ((boxPos != std::string::npos) ? boxPos : std::string::npos);
      if (startPos != std::string::npos) {
        typeName = typeName.substr(startPos + 4);
        size_t endPos = typeName.rfind(">");
        if (endPos != std::string::npos)
          typeName = typeName.substr(0, endPos);
      }

      std::string dropName = typeName + ".destructor_ret_void";
      return module->getFunction(dropName);
    }
    return nullptr;
  }

  bool isRefCounted(const hir::HIRType *type) {
    if (!type)
      return false;

    if (auto *nullableTy =
            llvm::dyn_cast_or_null<const hir::HIRNullableType>(type)) {
      type = nullableTy->getInner();
    }

    if (auto *ptrType = llvm::dyn_cast_or_null<const hir::PointerType>(type)) {
      if (ptrType->getOwnership() == hir::Ownership::Borrowed ||
          ptrType->getOwnership() == hir::Ownership::None) {
        return false;
      }

      if (ptrType->getOwnership() == hir::Ownership::Shared ||
          ptrType->getOwnership() == hir::Ownership::Owned) {
        return true;
      }
      if (ptrType->getPointee() &&
          ptrType->getPointee()->getKind() == hir::TypeKind::Struct) {
        return true;
      }
      return false;
    }

    auto kind = type->getKind();
    if (kind == hir::TypeKind::Any || kind == hir::TypeKind::Slice ||
        kind == hir::TypeKind::String || kind == hir::TypeKind::Map ||
        kind == hir::TypeKind::Closure || kind == hir::TypeKind::Function ||
        kind == hir::TypeKind::Promise) {
      return true;
    }

    return false;
  }

  // Analyzes the actual memory location rather than the loaded SSA instance.
  MIRValue *getUnderlyingObject(MIRValue *val) {
    while (auto *inst = llvm::dyn_cast_or_null<MIRInst>(val)) {
      if (inst->getOpcode() == Opcode::BitCast ||
          inst->getOpcode() == Opcode::AnyCast) {
        val = static_cast<CastInst *>(inst)->getValue();
        continue;
      } else if (inst->getOpcode() == Opcode::ExtractValue) {
        auto *ext = static_cast<ExtractValueInst *>(inst);
        if (ext->getIndex() == 0) {
          val = ext->getAggregate();
          continue;
        }
        break;
      } else if (auto *load = llvm::dyn_cast_or_null<LoadInst>(inst)) {
        val = load->getPointer();
        continue;
      }
      break;
    }
    return val;
  }

  bool runOnFunction(MIRFunction *func) {
    if (func->isDeclaration())
      return false;

    MIRBlock *entryBlock = func->getEntryBlock();
    bool functionModified = false;

    // 1. Identify which parameters actually need ARC
    std::vector<MIRArgument *> refCountedParams;
    for (auto *arg : func->getRawArguments()) {
      if (arg->getName() == "this") {
        continue;
      }
      if (isRefCounted(arg->getType())) {
        refCountedParams.push_back(arg);
      }
    }

    std::unordered_set<MIRValue *> initializedPointers;

    for (auto &blockPtr : func->getBlocks()) {
      MIRBlock *block = blockPtr.get();
      auto &instructions = block->getInstructionsMut();
      std::vector<std::unique_ptr<MIRInst>> newInstructions;

      bool blockModified = false;

      MIRValue *exitVal = nullptr;
      MIRValue *baseExitVal = nullptr;
      if (!instructions.empty()) {
        if (auto *retInst =
                llvm::dyn_cast_or_null<ReturnInst>(instructions.back().get())) {
          exitVal = retInst->getReturnValue();
          if (exitVal && exitVal->getType() &&
              isRefCounted(exitVal->getType())) {
            baseExitVal = getUnderlyingObject(exitVal);
          }
        }
      }

      bool elidedFrontendRelease = false;

      for (auto &inst : instructions) {
        auto op = inst->getOpcode();
        if (op == Opcode::Release && baseExitVal) {
          auto *arcInst = static_cast<ARCInst *>(inst.get());
          if (getUnderlyingObject(arcInst->getObject()) == baseExitVal) {
            elidedFrontendRelease = true;
            blockModified = true;
            continue;
          }
        }

        if (op == Opcode::Return && baseExitVal) {
          bool isParam = false;
          for (auto *arg : refCountedParams) {
            if (arg == baseExitVal)
              isParam = true;
          }

          bool isFreshAlloc = false;
          MIRValue *traceVal = exitVal;
          while (traceVal) {
            if (auto *cast = llvm::dyn_cast_or_null<CastInst>(traceVal)) {
              traceVal = cast->getValue();
            } else if (auto *load =
                           llvm::dyn_cast_or_null<LoadInst>(traceVal)) {
              traceVal = load->getPointer();
            } else if (auto *alloca =
                           llvm::dyn_cast_or_null<AllocaInst>(traceVal)) {
              isFreshAlloc = true;
              break;
            } else {
              break;
            }
          }

          if (!elidedFrontendRelease && !isParam && !isFreshAlloc) {
            newInstructions.push_back(std::make_unique<ARCInst>(
                Opcode::Retain, exitVal, nullptr, inst->getLoc()));
            blockModified = true;
          }
        }
        newInstructions.push_back(std::move(inst));
      }

      for (auto &newInst : newInstructions) {
        newInst->setParent(block);
      }
      instructions = std::move(newInstructions);

      if (blockModified) {
        functionModified = true;
      }
    }

    return functionModified;
  }
};

} // namespace

bool runARCInsertion(MIRModule *module, DiagnosticEngine &diags) {
  ARCInserter inserter(module, diags);
  return inserter.run();
}

} // namespace mir
} // namespace moksha

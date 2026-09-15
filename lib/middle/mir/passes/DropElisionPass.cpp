#include "moksha/MIR/Passes/DropElisionPass.h"
#include "moksha/HIR/HIRType.h"
#include "moksha/MIR/MIRBlock.h"
#include "moksha/MIR/MIRFunction.h"
#include "moksha/MIR/MIRInst.h"
#include "moksha/MIR/MIRModule.h"
#include "llvm/Support/Casting.h"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace moksha {
namespace mir {

namespace {

std::unordered_map<MIRBlock *, std::vector<MIRBlock *>>
buildPredecessorMap(MIRFunction *func) {
  std::unordered_map<MIRBlock *, std::vector<MIRBlock *>> preds;
  for (auto &blockPtr : func->getBlocks()) {
    MIRBlock *block = blockPtr.get();
    for (auto *succ : block->getSuccessors()) {
      preds[succ].push_back(block);
    }
  }
  return preds;
}

MIRValue *getBaseAlloca(MIRValue *val) {
  while (val) {
    if (llvm::isa<AllocaInst>(val))
      return val;
    if (auto *load = llvm::dyn_cast_or_null<LoadInst>(val))
      val = load->getPointer();
    else if (auto *gep = llvm::dyn_cast_or_null<GetElementPtrInst>(val))
      val = gep->getPointer();
    else if (auto *cast = llvm::dyn_cast_or_null<CastInst>(val))
      val = cast->getValue();
    else if (auto *ext = llvm::dyn_cast_or_null<ExtractValueInst>(val))
      val = ext->getAggregate();
    else
      break;
  }
  return nullptr;
}

/* @brief Checks if the given type is ARC material (shared or owned). */
bool isARCMaterial(const hir::HIRType *ty) {
  if (!ty)
    return false;
  const hir::HIRType *coreTy = ty;

  while (auto *ptrTy = llvm::dyn_cast_or_null<hir::PointerType>(coreTy)) {
    if (ptrTy->getOwnership() == hir::Ownership::Shared ||
        ptrTy->getOwnership() == hir::Ownership::Owned) {
      return true;
    }
    coreTy = ptrTy->getPointee();
  }
  if (auto *refTy = llvm::dyn_cast_or_null<hir::ReferenceType>(coreTy)) {
    coreTy = refTy->getInner();
  }
  if (auto *nullTy = llvm::dyn_cast_or_null<hir::HIRNullableType>(coreTy)) {
    coreTy = nullTy->getInner();
  }

  if (!coreTy)
    return false;

  auto kind = coreTy->getKind();
  if (kind == hir::TypeKind::Slice || kind == hir::TypeKind::Array ||
      kind == hir::TypeKind::String || kind == hir::TypeKind::Map ||
      kind == hir::TypeKind::Any || kind == hir::TypeKind::Closure ||
      kind == hir::TypeKind::Function || kind == hir::TypeKind::Promise) {
    return true;
  }

  if (auto *stTy = llvm::dyn_cast_or_null<hir::StructType>(coreTy)) {
    if (stTy->isRefClass() ||
        coreTy->toString().find("class.") != std::string::npos) {
      return true;
    }
  }

  if (auto *stTy = llvm::dyn_cast_or_null<hir::StructType>(coreTy)) {
    return true;
  }

  std::string name = coreTy->toString();
  if (name.find("shared ") != std::string::npos ||
      name.find("Closure.") != std::string::npos ||
      name.find("closure") != std::string::npos ||
      name.find("Arc<") != std::string::npos ||
      name.find("Box<") != std::string::npos) {
    return true;
  }

  return false;
}

} // namespace

bool DropElisionPass::runOnModule(MIRModule &M) {
  bool changed = false;
  for (auto &func : M.getFunctions()) {
    changed |= runOnFunction(func);
  }
  return changed;
}

bool DropElisionPass::runOnFunction(MIRFunction *F) {
  if (F->isDeclaration())
    return false;

  bool functionChanged = false;

  // 1. Dataflow Analysis: Track which Allocas are fully moved
  auto preds = buildPredecessorMap(F);
  std::unordered_map<MIRBlock *, std::unordered_set<MIRValue *>> blockMovedIn;
  std::unordered_map<MIRBlock *, std::unordered_set<MIRValue *>> blockMovedOut;
  bool changed = true;

  while (changed) {
    changed = false;
    for (auto &blockPtr : F->getBlocks()) {
      MIRBlock *block = blockPtr.get();

      std::unordered_set<MIRValue *> inSet;
      bool firstPred = true;
      for (auto *pred : preds[block]) {
        if (firstPred) {
          inSet = blockMovedOut[pred];
          firstPred = false;
        } else {
          // Intersection: Only keep values moved in ALL incoming paths
          for (auto it = inSet.begin(); it != inSet.end();) {
            if (blockMovedOut[pred].find(*it) == blockMovedOut[pred].end()) {
              it = inSet.erase(it);
            } else {
              ++it;
            }
          }
        }
      }
      blockMovedIn[block] = inSet;

      std::unordered_set<MIRValue *> currentOut = inSet;

      for (auto &instPtr : block->getInstructions()) {
        MIRInst *inst = instPtr.get();

        if (auto *store = llvm::dyn_cast_or_null<StoreInst>(inst)) {
          bool isDestSpill = false;
          if (MIRValue *destAlloca = getBaseAlloca(store->getPointer())) {
            currentOut.erase(destAlloca);
            if (destAlloca->getName().find(".spill") != std::string::npos) {
              isDestSpill = true;
            }
          }

          if (!isDestSpill) {
            if (auto *sourceLoad =
                    llvm::dyn_cast_or_null<LoadInst>(store->getValue())) {
              std::string loadName = sourceLoad->getName();
              if (loadName.find("cleanup") == std::string::npos &&
                  loadName.find("old") == std::string::npos) {
                if (sourceLoad->getBorrowKind() != BorrowKind::View) {
                  if (MIRValue *sourceAlloca =
                          getBaseAlloca(sourceLoad->getPointer())) {
                    // Check the true underlying type instead of the loaded cast
                    bool isEnv = sourceAlloca->getName().find("Env.lambda") !=
                                     std::string::npos ||
                                 (sourceAlloca->getType() &&
                                  sourceAlloca->getType()->toString().find(
                                      "Env.lambda") != std::string::npos);
                    bool isTemp =
                        !isEnv && (sourceAlloca->getName().find(".stack") !=
                                       std::string::npos ||
                                   sourceAlloca->getName().find(".temp") !=
                                       std::string::npos ||
                                   sourceAlloca->getName().find("temp.") !=
                                       std::string::npos ||
                                   sourceAlloca->getName().find(
                                       "new.obj.stack") != std::string::npos);
                    if (!isARCMaterial(sourceAlloca->getType()) || isTemp) {
                      currentOut.insert(sourceAlloca);
                    }
                  }
                }
              }
            }
          }
        } else if (auto *call = llvm::dyn_cast_or_null<CallInst>(inst)) {
          for (auto *arg : call->getArgs()) {
            if (auto *argLoad = llvm::dyn_cast_or_null<LoadInst>(arg)) {
              std::string argName = argLoad->getName();
              if (argName.find("cleanup") == std::string::npos &&
                  argName.find("old") == std::string::npos) {
                if (argLoad->getBorrowKind() != BorrowKind::View) {
                  if (MIRValue *sourceAlloca =
                          getBaseAlloca(argLoad->getPointer())) {
                    bool isEnv = sourceAlloca->getName().find("Env.lambda") !=
                                     std::string::npos ||
                                 (sourceAlloca->getType() &&
                                  sourceAlloca->getType()->toString().find(
                                      "Env.lambda") != std::string::npos);
                    bool isTemp =
                        !isEnv && (sourceAlloca->getName().find(".stack") !=
                                       std::string::npos ||
                                   sourceAlloca->getName().find(".temp") !=
                                       std::string::npos ||
                                   sourceAlloca->getName().find("temp.") !=
                                       std::string::npos ||
                                   sourceAlloca->getName().find(
                                       "new.obj.stack") != std::string::npos);
                    if (!isARCMaterial(sourceAlloca->getType()) || isTemp) {
                      currentOut.insert(sourceAlloca);
                    }
                  }
                }
              }
            }
          }
        } else if (auto *invoke = llvm::dyn_cast_or_null<InvokeInst>(inst)) {
          for (auto *arg : invoke->getArgs()) {
            if (auto *argLoad = llvm::dyn_cast_or_null<LoadInst>(arg)) {
              if (argLoad->getName() != "cleanup_val" &&
                  argLoad->getBorrowKind() != BorrowKind::View) {
                if (MIRValue *sourceAlloca =
                        getBaseAlloca(argLoad->getPointer())) {
                  bool isEnv = sourceAlloca->getName().find("Env.lambda") !=
                                   std::string::npos ||
                               (sourceAlloca->getType() &&
                                sourceAlloca->getType()->toString().find(
                                    "Env.lambda") != std::string::npos);
                  bool isTemp =
                      !isEnv && (sourceAlloca->getName().find(".stack") !=
                                     std::string::npos ||
                                 sourceAlloca->getName().find(".temp") !=
                                     std::string::npos ||
                                 sourceAlloca->getName().find("temp.") !=
                                     std::string::npos ||
                                 sourceAlloca->getName().find(
                                     "new.obj.stack") != std::string::npos);
                  if (!isARCMaterial(sourceAlloca->getType()) || isTemp) {
                    currentOut.insert(sourceAlloca);
                  }
                }
              }
            }
          }
        }
      }

      if (currentOut != blockMovedOut[block]) {
        blockMovedOut[block] = currentOut;
        changed = true;
      }
    }
  }

  // 2. Elision Sweep: Remove Drops for Moved Variables
  for (auto &blockPtr : F->getBlocks()) {
    MIRBlock *block = blockPtr.get();
    std::unordered_set<MIRValue *> movedAllocas = blockMovedIn[block];

    auto &insts = block->getInstructionsMut();
    auto it = insts.begin();

    while (it != insts.end()) {
      MIRInst *inst = it->get();
      bool elideInstruction = false;

      if (auto *store = llvm::dyn_cast_or_null<StoreInst>(inst)) {
        bool isDestSpill = false;
        if (MIRValue *destAlloca = getBaseAlloca(store->getPointer())) {
          movedAllocas.erase(destAlloca);
          if (destAlloca->getName().find(".spill") != std::string::npos) {
            isDestSpill = true;
          }
        }

        if (!isDestSpill) {
          if (auto *sourceLoad =
                  llvm::dyn_cast_or_null<LoadInst>(store->getValue())) {
            if (sourceLoad->getName() != "cleanup_val" &&
                sourceLoad->getName() != "old_val") {
              if (sourceLoad->getBorrowKind() != BorrowKind::View) {
                if (MIRValue *sourceAlloca =
                        getBaseAlloca(sourceLoad->getPointer())) {
                  bool isEnv = sourceAlloca->getName().find("Env.lambda") !=
                                   std::string::npos ||
                               (sourceAlloca->getType() &&
                                sourceAlloca->getType()->toString().find(
                                    "Env.lambda") != std::string::npos);
                  bool isTemp =
                      !isEnv && (sourceAlloca->getName().find(".stack") !=
                                     std::string::npos ||
                                 sourceAlloca->getName().find(".temp") !=
                                     std::string::npos ||
                                 sourceAlloca->getName().find("temp.") !=
                                     std::string::npos ||
                                 sourceAlloca->getName().find(
                                     "new.obj.stack") != std::string::npos);
                  if (!isARCMaterial(sourceAlloca->getType()) || isTemp) {
                    movedAllocas.insert(sourceAlloca);
                  }
                }
              }
            }
          }
        }
      } else if (auto *call = llvm::dyn_cast_or_null<CallInst>(inst)) {
        for (auto *arg : call->getArgs()) {
          if (auto *argLoad = llvm::dyn_cast_or_null<LoadInst>(arg)) {
            if (argLoad->getName() != "cleanup_val") {
              if (argLoad->getBorrowKind() != BorrowKind::View) {
                if (MIRValue *sourceAlloca =
                        getBaseAlloca(argLoad->getPointer())) {
                  bool isEnv = sourceAlloca->getName().find("Env.lambda") !=
                                   std::string::npos ||
                               (sourceAlloca->getType() &&
                                sourceAlloca->getType()->toString().find(
                                    "Env.lambda") != std::string::npos);
                  bool isTemp =
                      !isEnv && (sourceAlloca->getName().find(".stack") !=
                                     std::string::npos ||
                                 sourceAlloca->getName().find(".temp") !=
                                     std::string::npos ||
                                 sourceAlloca->getName().find("temp.") !=
                                     std::string::npos ||
                                 sourceAlloca->getName().find(
                                     "new.obj.stack") != std::string::npos);
                  if (!isARCMaterial(sourceAlloca->getType()) || isTemp) {
                    movedAllocas.insert(sourceAlloca);
                  }
                }
              }
            }
          }
        }
      } else if (auto *invoke = llvm::dyn_cast_or_null<InvokeInst>(inst)) {
        if (invoke->getCallee()) {
          std::string calleeName = invoke->getCallee()->getName();
          if (calleeName == "__moksha_free" ||
              calleeName == "moksha_rt_map_free_internal" ||
              calleeName == "moksha_rt_release_closure_env" ||
              calleeName.find(".destructor_ret_void") != std::string::npos ||
              calleeName.find(".drop_ret_void") != std::string::npos) {
            if (invoke->getArgs().size() > 0) {
              if (MIRValue *base = getBaseAlloca(invoke->getArgs()[0])) {
                bool isEnv =
                    base->getName().find("Env.lambda") != std::string::npos ||
                    (base->getType() && base->getType()->toString().find(
                                            "Env.lambda") != std::string::npos);
                bool isTemp =
                    !isEnv &&
                    (base->getName().find(".stack") != std::string::npos ||
                     base->getName().find(".temp") != std::string::npos ||
                     base->getName().find("temp.") != std::string::npos ||
                     base->getName().find("new.obj.stack") !=
                         std::string::npos);
                if (!isARCMaterial(base->getType()) || isTemp) {
                  if (movedAllocas.count(base)) {
                    elideInstruction = true;
                  }
                }
              }
            }
          }
        }
      }

      // Check for Elision targets
      if (auto *arc = llvm::dyn_cast_or_null<ARCInst>(inst)) {
        if (arc->getOpcode() == Opcode::Release) {
          if (MIRValue *base = getBaseAlloca(arc->getObject())) {
            // Respect the ARC material check before stripping drops!
            bool isEnv =
                base->getName().find("Env.lambda") != std::string::npos ||
                (base->getType() && base->getType()->toString().find(
                                        "Env.lambda") != std::string::npos);
            bool isTemp =
                !isEnv &&
                (base->getName().find(".stack") != std::string::npos ||
                 base->getName().find(".temp") != std::string::npos ||
                 base->getName().find("temp.") != std::string::npos ||
                 base->getName().find("new.obj.stack") != std::string::npos);
            if ((!isARCMaterial(base->getType()) || isTemp) &&
                movedAllocas.count(base)) {
              elideInstruction = true;
            }
          }
        }
      } else if (auto *call = llvm::dyn_cast_or_null<CallInst>(inst)) {
        if (call->getCallee()) {
          std::string calleeName = call->getCallee()->getName();
          if (calleeName == "__moksha_free" ||
              calleeName == "moksha_rt_map_free_internal" ||
              calleeName == "moksha_rt_release_closure_env" ||
              calleeName.find(".destructor_ret_void") != std::string::npos ||
              calleeName.find(".drop_ret_void") != std::string::npos) {
            if (call->getArgs().size() > 0) {
              if (MIRValue *base = getBaseAlloca(call->getArgs()[0])) {
                bool isEnv =
                    base->getName().find("Env.lambda") != std::string::npos ||
                    (base->getType() && base->getType()->toString().find(
                                            "Env.lambda") != std::string::npos);
                bool isTemp =
                    !isEnv &&
                    (base->getName().find(".stack") != std::string::npos ||
                     base->getName().find(".temp") != std::string::npos ||
                     base->getName().find("temp.") != std::string::npos ||
                     base->getName().find("new.obj.stack") !=
                         std::string::npos);
                if ((!isARCMaterial(base->getType()) || isTemp) &&
                    movedAllocas.count(base)) {
                  elideInstruction = true;
                }
              }
            }
          }
        }
      }

      if (elideInstruction) {
        it = insts.erase(it);
        functionChanged = true;
      } else {
        ++it;
      }
    }
  }

  return functionChanged;
}

} // namespace mir
} // namespace moksha

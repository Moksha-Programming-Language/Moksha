#include "moksha/MIR/Passes/EscapeAnalysisPass.h"
#include "moksha/HIR/HIRType.h"
#include "moksha/MIR/MIRArgument.h"
#include "moksha/MIR/MIRBlock.h"
#include "moksha/MIR/MIRFunction.h"
#include "moksha/MIR/MIRInst.h"
#include "moksha/MIR/MIRModule.h"
#include "llvm/Support/Casting.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace moksha {
namespace mir {

static void replaceAllUsesInFunction(MIRFunction *F, MIRValue *oldVal,
                                     MIRValue *newVal) {
  for (auto &blockPtr : F->getBlocks()) {
    for (auto &instPtr : blockPtr->getInstructionsMut()) {
      instPtr->replaceOperand(oldVal, newVal);
    }
  }
}

bool EscapeAnalysisPass::runOnModule(MIRModule &M) {
  bool changed = false;
  for (auto &func : M.getFunctions()) {
    changed |= runOnFunction(func, M);
  }
  return changed;
}

bool EscapeAnalysisPass::runOnFunction(MIRFunction *F, MIRModule &M) {
  if (F->isDeclaration())
    return false;

  bool changed = false;
  std::vector<CallInst *> allocations;

  // 1. Build a Def-Use map for the function
  std::unordered_map<MIRValue *, std::vector<MIRInst *>> defUse;
  for (auto &block : F->getBlocks()) {
    for (auto &instPtr : block->getInstructions()) {
      MIRInst *inst = instPtr.get();

      if (auto *store = llvm::dyn_cast_or_null<StoreInst>(inst)) {
        defUse[store->getValue()].push_back(inst);
        defUse[store->getPointer()].push_back(inst);
      } else if (auto *load = llvm::dyn_cast_or_null<LoadInst>(inst)) {
        defUse[load->getPointer()].push_back(inst);
      } else if (auto *gep = llvm::dyn_cast_or_null<GetElementPtrInst>(inst)) {
        defUse[gep->getPointer()].push_back(inst);
      } else if (auto *cast = llvm::dyn_cast_or_null<CastInst>(inst)) {
        defUse[cast->getValue()].push_back(inst);
      } else if (auto *ins = llvm::dyn_cast_or_null<InsertValueInst>(inst)) {
        defUse[ins->getAggregate()].push_back(inst);
        defUse[ins->getValue()].push_back(inst);
      } else if (auto *ext = llvm::dyn_cast_or_null<ExtractValueInst>(inst)) {
        defUse[ext->getAggregate()].push_back(inst);
      } else if (auto *call = llvm::dyn_cast_or_null<CallInst>(inst)) {
        for (auto *arg : call->getArgs())
          defUse[arg].push_back(inst);

        if (call->getCallee() &&
            call->getCallee()->getName() == "__moksha_alloc") {
          bool isArray = false;
          if (call->getArgs().size() > 1) {
            if (auto *typeId =
                    llvm::dyn_cast_or_null<ConstantInt>(call->getArgs()[1])) {
              if (typeId->getValue() == 3) {
                isArray = true;
              }
            }
          }
          if (!isArray) {
            allocations.push_back(call);
          }
        }
      } else if (auto *invoke = llvm::dyn_cast_or_null<InvokeInst>(inst)) {
        for (auto *arg : invoke->getArgs())
          defUse[arg].push_back(inst);
      } else if (auto *thr = llvm::dyn_cast_or_null<ThrowInst>(inst)) {
        if (thr->getException())
          defUse[thr->getException()].push_back(inst);
      } else if (auto *res = llvm::dyn_cast_or_null<ResumeInst>(inst)) {
        if (res->getException())
          defUse[res->getException()].push_back(inst);
      } else if (auto *asmInst = llvm::dyn_cast_or_null<InlineAsmInst>(inst)) {
        for (auto *arg : asmInst->getArgs())
          defUse[arg].push_back(inst);
      } else if (auto *storeWk = llvm::dyn_cast_or_null<StoreWeakInst>(inst)) {
        defUse[storeWk->getValue()].push_back(inst);
        defUse[storeWk->getPointer()].push_back(inst);
      } else if (auto *loadWk = llvm::dyn_cast_or_null<LoadWeakInst>(inst)) {
        defUse[loadWk->getPointer()].push_back(inst);
      } else if (auto *arc = llvm::dyn_cast_or_null<ARCInst>(inst)) {
        defUse[arc->getObject()].push_back(inst);
      } else if (auto *ret = llvm::dyn_cast_or_null<ReturnInst>(inst)) {
        if (ret->getReturnValue())
          defUse[ret->getReturnValue()].push_back(inst);
      } else if (auto *makeClosure =
                     llvm::dyn_cast_or_null<MakeClosureInst>(inst)) {
        defUse[makeClosure->getFunctionPointer()].push_back(inst);
        for (auto *cap : makeClosure->getCaptures())
          defUse[cap].push_back(inst);
      } else if (auto *spawn = llvm::dyn_cast_or_null<SpawnInst>(inst)) {
        if (spawn->getClosure())
          defUse[spawn->getClosure()].push_back(inst);
      }
    }
  }

  // 2. Analyze each allocation to see if it escapes
  for (CallInst *alloc : allocations) {
    bool isEnv = false;
    const hir::HIRType *envPtrTy = nullptr;

    std::vector<CastInst *> bitcasts;
    if (defUse.count(alloc)) {
      for (auto *user : defUse[alloc]) {
        if (auto *cast = llvm::dyn_cast_or_null<CastInst>(user)) {
          bitcasts.push_back(cast);
          if (cast->getType() && cast->getType()->toString().find(
                                     "Env.lambda") != std::string::npos) {
            isEnv = true;
            envPtrTy = cast->getType();
          }
        }
      }
    }

    if (!doesEscape(alloc, defUse)) {
      if (bitcasts.empty())
        continue;

      const hir::HIRType *actualType = bitcasts[0]->getType();
      const hir::HIRType *pointeeType = nullptr;
      bool isShared = false;

      if (auto *pTy = llvm::dyn_cast_or_null<hir::PointerType>(actualType)) {
        pointeeType = pTy->getPointee();
        if (pTy->getOwnership() == hir::Ownership::Shared) {
          isShared = true;
        }
      } else {
        pointeeType = actualType;
        if (actualType &&
            actualType->toString().find("shared ") != std::string::npos) {
          isShared = true;
        }
      }

      if (isShared) {
        continue;
      }

      // 3. Promote to Stack: Create an AllocaInst
      auto alloca = std::make_unique<AllocaInst>(actualType, pointeeType,
                                                 alloc->getName() + ".stack",
                                                 alloc->getLoc(), 8);
      alloca->setBorrowKind(BorrowKind::Mut);
      MIRBlock *entryBlock = F->getEntryBlock();
      alloca->setParent(entryBlock);

      MIRValue *newAllocaPtr = alloca.get();

      entryBlock->getInstructionsMut().insert(
          entryBlock->getInstructionsMut().begin(), std::move(alloca));

      for (auto *bc : bitcasts) {
        replaceAllUsesInFunction(F, bc, newAllocaPtr);
      }

      replaceAllUsesInFunction(F, alloc, newAllocaPtr);

      for (auto *bitcast : bitcasts) {
        replaceAllUsesInFunction(F, bitcast, newAllocaPtr);
        auto &castInsts = bitcast->getParent()->getInstructionsMut();
        castInsts.erase(std::remove_if(castInsts.begin(), castInsts.end(),
                                       [&](const std::unique_ptr<MIRInst> &i) {
                                         return i.get() == bitcast;
                                       }),
                        castInsts.end());
      }

      std::vector<MIRInst *> freeCalls;
      std::vector<MIRInst *> closureReleaseCalls; // ADD THIS

      for (auto *bc : bitcasts) {
        if (defUse.count(bc)) {
          for (auto *user : defUse[bc]) {
            if (auto *call = llvm::dyn_cast_or_null<CallInst>(user)) {
              if (call->getCallee() &&
                  call->getCallee()->getName() == "__moksha_free") {
                freeCalls.push_back(call);
              } else if (call->getCallee() &&
                         call->getCallee()->getName() ==
                             "moksha_rt_release_closure_env") {
                closureReleaseCalls.push_back(call); // Catch closure releases
              }
            } else if (auto *invoke =
                           llvm::dyn_cast_or_null<InvokeInst>(user)) {
              if (invoke->getCallee() &&
                  invoke->getCallee()->getName() == "__moksha_free") {
                freeCalls.push_back(invoke);
              } else if (invoke->getCallee() &&
                         invoke->getCallee()->getName() ==
                             "moksha_rt_release_closure_env") {
                closureReleaseCalls.push_back(invoke); // Catch closure invokes
              }
            }
          }
        }
      }

      if (defUse.count(alloc)) {
        for (auto *user : defUse[alloc]) {
          if (auto *call = llvm::dyn_cast_or_null<CallInst>(user)) {
            if (call->getCallee() &&
                call->getCallee()->getName() == "__moksha_free") {
              freeCalls.push_back(call);
            } else if (call->getCallee() &&
                       call->getCallee()->getName() ==
                           "moksha_rt_release_closure_env") {
              closureReleaseCalls.push_back(call);
            }
          } else if (auto *invoke = llvm::dyn_cast_or_null<InvokeInst>(user)) {
            if (invoke->getCallee() &&
                invoke->getCallee()->getName() == "__moksha_free") {
              freeCalls.push_back(invoke);
            } else if (invoke->getCallee() &&
                       invoke->getCallee()->getName() ==
                           "moksha_rt_release_closure_env") {
              closureReleaseCalls.push_back(invoke);
            }
          }
        }
      }

      for (MIRInst *freeCall : freeCalls) {
        auto &freeInsts = freeCall->getParent()->getInstructionsMut();
        freeInsts.erase(std::remove_if(freeInsts.begin(), freeInsts.end(),
                                       [&](const std::unique_ptr<MIRInst> &i) {
                                         return i.get() == freeCall;
                                       }),
                        freeInsts.end());
      }

      if (!closureReleaseCalls.empty()) {
        std::string dtorOnlyName = "moksha_rt_execute_closure_dtor_only";
        MIRFunction *dtorOnlyFunc = M.getFunction(dtorOnlyName);
        if (!dtorOnlyFunc) {
          MIRFunction *existingRel =
              M.getFunction("moksha_rt_release_closure_env");
          auto fn = std::make_unique<MIRFunction>(
              existingRel->getType(), dtorOnlyName, Linkage::External);
          fn->addArgument(std::make_unique<MIRArgument>(
              fn.get(), existingRel->getRawArguments()[0]->getType(), 0));
          dtorOnlyFunc = fn.get();
          M.addFunction(std::move(fn));
        }
        for (MIRInst *relCall : closureReleaseCalls) {
          if (auto *c = llvm::dyn_cast_or_null<CallInst>(relCall)) {
            c->replaceOperand(c->getCallee(), dtorOnlyFunc);
          } else if (auto *i = llvm::dyn_cast_or_null<InvokeInst>(relCall)) {
            i->replaceOperand(i->getCallee(), dtorOnlyFunc);
          }
        }
      }

      auto &allocInsts = alloc->getParent()->getInstructionsMut();
      allocInsts.erase(std::remove_if(allocInsts.begin(), allocInsts.end(),
                                      [&](const std::unique_ptr<MIRInst> &i) {
                                        return i.get() == alloc;
                                      }),
                       allocInsts.end());

      changed = true;
    } else if (isEnv) {
      if (alloc->getArgs().size() > 1) {
        auto *typeArg = alloc->getArgs()[1];
        MIRValue *newTypeId =
            M.getOrInsertConstant<ConstantInt>(21, typeArg->getType());
        auto &mutableArgs =
            const_cast<std::vector<MIRValue *> &>(alloc->getArgs());
        mutableArgs[1] = newTypeId;
        changed = true;
      }
    }
  }

  return changed;
}

bool EscapeAnalysisPass::doesEscape(
    MIRValue *val,
    const std::unordered_map<MIRValue *, std::vector<MIRInst *>> &defUse) {
  std::vector<MIRValue *> worklist = {val};
  std::unordered_set<MIRValue *> visited;

  while (!worklist.empty()) {
    MIRValue *curr = worklist.back();
    worklist.pop_back();

    if (!visited.insert(curr).second)
      continue;

    auto it = defUse.find(curr);
    if (it == defUse.end())
      continue;

    for (MIRInst *user : it->second) {
      if (auto *storeWk = llvm::dyn_cast_or_null<StoreWeakInst>(user)) {
        if (storeWk->getValue() == curr) {
          if (auto *alloca =
                  llvm::dyn_cast_or_null<AllocaInst>(storeWk->getPointer())) {
            worklist.push_back(alloca);
          } else {
            return true;
          }
        }
      } else if (auto *store = llvm::dyn_cast_or_null<StoreInst>(user)) {
        if (store->getValue() == curr) {
          if (auto *alloca =
                  llvm::dyn_cast_or_null<AllocaInst>(store->getPointer())) {
            worklist.push_back(alloca);
          } else {
            return true;
          }
        }
      } else if (auto *call = llvm::dyn_cast_or_null<CallInst>(user)) {
        if (call->getCallee() &&
            (call->getCallee()->getName() == "__moksha_alloc" ||
             call->getCallee()->getName() == "__moksha_free" ||
             call->getCallee()->getName().find("lambda.") == 0 ||
             call->getCallee()->getName().find("Closure.lambda.") == 0 ||
             call->getCallee()->getName().find(".destructor_ret_void") !=
                 std::string::npos)) {
        } else {
          return true;
        }
      } else if (auto *invoke = llvm::dyn_cast_or_null<InvokeInst>(user)) {
        if (invoke->getCallee() &&
            (invoke->getCallee()->getName() == "__moksha_alloc" ||
             invoke->getCallee()->getName() == "__moksha_free" ||
             invoke->getCallee()->getName().find("lambda.") == 0 ||
             invoke->getCallee()->getName().find("Closure.lambda.") == 0 ||
             invoke->getCallee()->getName().find(".destructor_ret_void") !=
                 std::string::npos)) {
        } else {
          return true;
        }
      } else if (auto *load = llvm::dyn_cast_or_null<LoadInst>(user)) {
        worklist.push_back(load);
      } else if (auto *loadWk = llvm::dyn_cast_or_null<LoadWeakInst>(user)) {
        worklist.push_back(loadWk);
      } else if (llvm::isa<ARCInst>(user)) {
      } else if (auto *gep = llvm::dyn_cast_or_null<GetElementPtrInst>(user)) {
        worklist.push_back(gep);
      } else if (auto *cast = llvm::dyn_cast_or_null<CastInst>(user)) {
        worklist.push_back(cast);
      } else if (auto *ext = llvm::dyn_cast_or_null<ExtractValueInst>(user)) {
        if (ext->getIndex() == 0)
          worklist.push_back(ext);
      } else if (auto *ins = llvm::dyn_cast_or_null<InsertValueInst>(user)) {
        worklist.push_back(ins);
      } else if (auto *makeClosure =
                     llvm::dyn_cast_or_null<MakeClosureInst>(user)) {
        worklist.push_back(makeClosure);
      } else if (llvm::isa<ReturnInst>(user) ||
                 llvm::isa<MakeSharedInst>(user) ||
                 llvm::isa<SpawnInst>(user)) {
        return true;
      } else {
        return true;
      }
    }
  }
  return false;
}

} // namespace mir
} // namespace moksha

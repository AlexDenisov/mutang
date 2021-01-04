#include "mull/Parallelization/Tasks/MutantPreparationTasks.h"

#include "LLVMCompatibility.h"
#include "mull/Bitcode.h"
#include "mull/MutationPoint.h"
#include "mull/Parallelization/Progress.h"
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Transforms/Utils/Cloning.h>

using namespace mull;

void CloneMutatedFunctionsTask::cloneFunctions(Bitcode &bitcode) {
  for (auto &pair : bitcode.getMutationPointsMap()) {
    llvm::Function *original = pair.first;
    for (MutationPoint *point : pair.second) {
      llvm::ValueToValueMapTy map;
      llvm::Function *mutatedFunction = llvm::CloneFunction(original, map);
      mutatedFunction->setLinkage(llvm::GlobalValue::InternalLinkage);
      point->setMutatedFunction(mutatedFunction);
    }
  }
}

void DeleteOriginalFunctionsTask::deleteFunctions(Bitcode &bitcode) {
  for (auto pair : bitcode.getMutationPointsMap()) {
    auto original = pair.first;
    auto anyPoint = pair.second.front();

    llvm::ValueToValueMapTy map;
    auto originalCopy = CloneFunction(original, map);
    originalCopy->setName(anyPoint->getOriginalFunctionName());
    original->dropAllReferences();
  }
}

void InsertMutationTrampolinesTask::insertTrampolines(Bitcode &bitcode) {
  llvm::Module *module = bitcode.getModule();
  llvm::LLVMContext &context = module->getContext();

  llvm::Type *charPtr = llvm::Type::getInt8Ty(context)->getPointerTo();
  llvm::FunctionType *getEnvType = llvm::FunctionType::get(charPtr, { charPtr }, false);
  llvm::Value *getenv = llvm_compat::getOrInsertFunction(module, "getenv", getEnvType);

  for (auto pair : bitcode.getMutationPointsMap()) {
    llvm::Function *original = pair.first;

    llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "entry", original);
    llvm::BasicBlock *originalBlock = llvm::BasicBlock::Create(context, "original", original);
    llvm::BasicBlock *trampolineCall =
        llvm::BasicBlock::Create(context, "trampoline_call", original);

    auto anyPoint = pair.second.front();
    llvm::Type *trampolineType = original->getFunctionType()->getPointerTo();
    auto trampoline = new llvm::AllocaInst(trampolineType, 0, "trampoline", entry);
    new llvm::StoreInst(bitcode.getModule()->getFunction(anyPoint->getOriginalFunctionName()),
                        trampoline,
                        originalBlock);
    llvm::BranchInst::Create(trampolineCall, originalBlock);

    llvm::BasicBlock *head = originalBlock;

    for (auto &point : pair.second) {
      auto name = llvm::ConstantDataArray::getString(context, point->getUserIdentifier());
      auto *global = new llvm::GlobalVariable(
          *module, name->getType(), true, llvm::GlobalValue::PrivateLinkage, name);

      llvm::BasicBlock *mutationCheckBlock =
          llvm::BasicBlock::Create(context, point->getUserIdentifier() + "_check", original);
      llvm::Value *nullPtr = llvm::Constant::getNullValue(charPtr);
      llvm::CmpInst *predicate = llvm::CmpInst::Create(llvm::Instruction::ICmp,
                                                       llvm::ICmpInst::ICMP_NE,
                                                       nullPtr,
                                                       nullPtr,
                                                       "is_enabled",
                                                       mutationCheckBlock);
      llvm::Value *zero = llvm::Constant::getNullValue(llvm::Type::getInt64Ty(context));
      auto mutantName =
          llvm::ConstantExpr::getInBoundsGetElementPtr(name->getType(), global, { zero, zero });
      auto getEnvCall =
          llvm::CallInst::Create(getEnvType, getenv, { mutantName }, "check_mutation", predicate);
      predicate->setOperand(0, getEnvCall);

      llvm::BasicBlock *mutationBlock =
          llvm::BasicBlock::Create(context, point->getUserIdentifier(), original);
      new llvm::StoreInst(point->getMutatedFunction(), trampoline, mutationBlock);

      llvm::BranchInst::Create(mutationBlock, head, predicate, mutationCheckBlock);
      llvm::BranchInst::Create(trampolineCall, mutationBlock);
      head = mutationCheckBlock;
    }

    llvm::BranchInst::Create(head, entry);

    std::vector<llvm::Value *> args;
    for (auto &arg : original->args()) {
      args.push_back(&arg);
    }

    auto retType = original->getFunctionType()->getReturnType();
    llvm::Constant *dummy = nullptr;
    if (!retType->isVoidTy()) {
      dummy = llvm::Constant::getNullValue(retType);
    }

    auto retVal = llvm::ReturnInst::Create(context, dummy, trampolineCall);
    auto loadValue = new llvm::LoadInst(trampoline->getType()->getPointerElementType(),
                                        trampoline,
                                        "trampoline_pointer",
                                        false,
                                        retVal);
    auto callInst =
        llvm::CallInst::Create(original->getFunctionType(), loadValue, args, "", retVal);
    if (!retType->isVoidTy()) {
      retVal->setOperand(0, callInst);
    }
  }
}

#include "mull/Mutators/OrAndReplacementMutator.h"

#include "mull/Logger.h"
#include "mull/MutationPoint.h"
#include "mull/ReachableFunction.h"
#include "mull/SourceLocation.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/DebugLoc.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>

#include <fstream>
#include <iterator>

using namespace llvm;
using namespace mull;

const std::string OrAndReplacementMutator::ID = "or_to_and_replacement_mutator";
const std::string OrAndReplacementMutator::description =
    "Replaces || with &&";

static std::string getMutationTypeToString(OR_AND_MutationType mutationType) {
  switch (mutationType) {
  case OR_AND_MutationType_OR_to_AND_Pattern1:
  case OR_AND_MutationType_OR_to_AND_Pattern2:
  case OR_AND_MutationType_OR_to_AND_Pattern3:
    return std::string("&&");
  default:
    assert(0 && "should not reach here");
  }
}

OR_AND_MutationType OrAndReplacementMutator::findPossibleMutation(Value &V) {
  BranchInst *branchInst = dyn_cast<BranchInst>(&V);

  if (branchInst == nullptr) {
    return OR_AND_MutationType_None;
  }

  if (branchInst->isConditional() == false) {
    return OR_AND_MutationType_None;
  }

  /// TODO: Discuss how to filter out irrelevant branch instructions.
  SourceLocation location = SourceLocation::locationFromInstruction(branchInst);
  if (!location.isNull()) {
    if (location.filePath.find("include/c++/v1") != std::string::npos) {
      return OR_AND_MutationType_None;
    }
  }

  BranchInst *secondBranch = nullptr;
  OR_AND_MutationType possibleMutationType =
      findPossibleMutationInBranch(branchInst, &secondBranch);

  return possibleMutationType;
}

void OrAndReplacementMutator::applyMutation(llvm::Function *function,
                                            const MutationPointAddress &address,
                                            irm::IRMutation *lowLevelMutation) {
  llvm::Instruction &I = address.findInstruction(function);

  BranchInst *branchInst = dyn_cast<BranchInst>(&I);
  assert(branchInst != nullptr);
  assert(branchInst->isConditional());

  BranchInst *secondBranch = nullptr;
  OR_AND_MutationType possibleMutationType =
      findPossibleMutationInBranch(branchInst, &secondBranch);

  if (possibleMutationType == OR_AND_MutationType_OR_to_AND_Pattern1) {
    return applyMutationORToAND_Pattern1(branchInst, secondBranch);
  }

  if (possibleMutationType == OR_AND_MutationType_OR_to_AND_Pattern2) {
    return applyMutationORToAND_Pattern2(branchInst, secondBranch);
  }

  if (possibleMutationType == OR_AND_MutationType_OR_to_AND_Pattern3) {
    return applyMutationORToAND_Pattern3(branchInst, secondBranch);
  }
}

#pragma mark - Private: Apply mutations: OR -> AND

void OrAndReplacementMutator::applyMutationORToAND_Pattern1(
    BranchInst *firstBranch, BranchInst *secondBranch) {

  assert(firstBranch != nullptr);
  assert(firstBranch->isConditional());

  assert(secondBranch != nullptr);
  assert(secondBranch->isConditional());

  /// Operand #0 is a comparison instruction or simply a scalar value.
  Value *sourceValue = dyn_cast<Value>(firstBranch->getOperand(0));
  assert(sourceValue);

  /// Left branch value is somehow operand #2, right is #1.
  BasicBlock *firstBranchRightBB =
      dyn_cast<BasicBlock>(firstBranch->getOperand(1));
  assert(firstBranchRightBB);

  BasicBlock *secondBranchLeftBB =
      dyn_cast<BasicBlock>(secondBranch->getOperand(2));
  BasicBlock *secondBranchRightBB =
      dyn_cast<BasicBlock>(secondBranch->getOperand(1));
  assert(secondBranchLeftBB);
  assert(secondBranchRightBB);

  BranchInst *replacement =
      BranchInst::Create(firstBranchRightBB, secondBranchRightBB, sourceValue);

  /// If I add a named instruction, and the name already exist
  /// in a basic block, then LLVM will make another unique name of it
  /// To prevent this name change we need to 'drop' the existing old name
  firstBranch->setName("");

  replacement->insertAfter(firstBranch);
  firstBranch->replaceAllUsesWith(replacement);

  firstBranch->eraseFromParent();

  /// If one of the second branch's successor basic blocks has a PHI node and
  /// second branch's right basic block jumps to that successor block,
  /// we need to update PHI node to also accept a jump from a replacement
  /// branch instruction.
  for (unsigned index = 0;
       index < secondBranch->getParent()->getTerminator()->getNumSuccessors();
       index++) {
    BasicBlock *secondBranchSuccessorBlock =
        secondBranch->getParent()->getTerminator()->getSuccessor(index);
    if (secondBranchRightBB != secondBranchSuccessorBlock) {
      continue;
    }

    for (Instruction &inst : *secondBranchSuccessorBlock) {
      PHINode *PN = dyn_cast<PHINode>(&inst);

      if (!PN) {
        break;
      }

      int i = PN->getBasicBlockIndex(secondBranch->getParent());
      if (i < 0) {
        continue;
      }

      int operandIndex = PN->getOperandNumForIncomingValue(i);
      Value *operand = PN->getOperand(operandIndex);

      PN->addIncoming(operand, replacement->getParent());
    }
  }
}

void OrAndReplacementMutator::applyMutationORToAND_Pattern2(
    BranchInst *firstBranch, BranchInst *secondBranch) {

  assert(firstBranch != nullptr);
  assert(firstBranch->isConditional());

  assert(secondBranch != nullptr);
  assert(secondBranch->isConditional());

  /// Operand #0 is a comparison instruction or simply a scalar value.
  Value *sourceValue = (dyn_cast<Value>(firstBranch->getOperand(0)));
  assert(sourceValue);

  /// Left branch value is somehow operand #2, right is #1.
  BasicBlock *firstBranchLeftBB =
      dyn_cast<BasicBlock>(firstBranch->getOperand(2));
  assert(firstBranchLeftBB);

  BasicBlock *secondBranchLeftBB =
      dyn_cast<BasicBlock>(secondBranch->getOperand(2));
  BasicBlock *secondBranchRightBB =
      dyn_cast<BasicBlock>(secondBranch->getOperand(1));
  assert(secondBranchLeftBB);
  assert(secondBranchRightBB);

  BranchInst *replacement =
      BranchInst::Create(secondBranchRightBB, firstBranchLeftBB, sourceValue);

  /// If I add a named instruction, and the name already exist
  /// in a basic block, then LLVM will make another unique name of it
  /// To prevent this name change we need to 'drop' the existing old name
  firstBranch->setName("");

  replacement->insertAfter(firstBranch);
  firstBranch->replaceAllUsesWith(replacement);

  firstBranch->eraseFromParent();

  /// If one of the second branch's successor basic blocks has a PHI node and
  /// second branch's right basic block jumps to that successor block,
  /// we need to update PHI node to also accept a jump from a replacement
  /// branch instruction.
  for (unsigned index = 0;
       index < secondBranch->getParent()->getTerminator()->getNumSuccessors();
       index++) {
    BasicBlock *secondBranchSuccessorBlock =
        secondBranch->getParent()->getTerminator()->getSuccessor(index);
    if (secondBranchRightBB != secondBranchSuccessorBlock) {
      continue;
    }

    for (Instruction &inst : *secondBranchSuccessorBlock) {
      PHINode *PN = dyn_cast<PHINode>(&inst);

      if (!PN) {
        break;
      }

      int i = PN->getBasicBlockIndex(secondBranch->getParent());
      if (i < 0) {
        continue;
      }

      int operandIndex = PN->getOperandNumForIncomingValue(i);
      Value *operand = PN->getOperand(operandIndex);

      PN->addIncoming(operand, replacement->getParent());
    }
  }
}

void OrAndReplacementMutator::applyMutationORToAND_Pattern3(
    BranchInst *firstBranch, BranchInst *secondBranch) {
  Module *module = firstBranch->getParent()->getParent()->getParent();

  PHINode *phiNode;
  for (auto &instruction : *secondBranch->getParent()) {
    phiNode = dyn_cast<PHINode>(&instruction);

    if (phiNode == nullptr) {
      continue;
    }

    break;
  }
  assert(phiNode);

  ConstantInt *intValueForIncomingBlock0 =
      dyn_cast<ConstantInt>(phiNode->getOperand(0));
  assert(intValueForIncomingBlock0);

  bool boolValueOfIncomingBlock =
      intValueForIncomingBlock0->getValue().getBoolValue();

  ConstantInt *newValue = boolValueOfIncomingBlock
                              ? ConstantInt::getFalse(module->getContext())
                              : ConstantInt::getTrue(module->getContext());

  phiNode->setOperand(0, newValue);

  auto firstBranchLeftBB = dyn_cast<BasicBlock>(firstBranch->getOperand(2));
  auto firstBranchRightBB = dyn_cast<BasicBlock>(firstBranch->getOperand(1));

  Value *firstBranchConditionValue =
      dyn_cast<Value>(firstBranch->getOperand(0));
  assert(firstBranchConditionValue);

  BranchInst *replacement = BranchInst::Create(
      firstBranchRightBB, firstBranchLeftBB, firstBranchConditionValue);

  /// If I add a named instruction, and the name already exist
  /// in a basic block, then LLVM will make another unique name of it
  /// To prevent this name change we need to 'drop' the existing old name
  firstBranch->setName("");

  replacement->insertAfter(firstBranch);
  firstBranch->replaceAllUsesWith(replacement);

  firstBranch->eraseFromParent();
}

#pragma mark - Private: Finding possible mutations

OR_AND_MutationType OrAndReplacementMutator::findPossibleMutationInBranch(
    BranchInst *branchInst, BranchInst **secondBranchInst) {

  if (branchInst->isConditional() == false) {
    return OR_AND_MutationType_None;
  }

  BasicBlock *leftBB = dyn_cast<BasicBlock>(branchInst->getOperand(2));
  BasicBlock *rightBB = dyn_cast<BasicBlock>(branchInst->getOperand(1));

  bool passedBranchInst = false;
  for (BasicBlock &bb : *branchInst->getFunction()) {
    for (Instruction &instruction : bb) {
      BranchInst *candidateBranchInst = dyn_cast<BranchInst>(&instruction);

      if (candidateBranchInst == nullptr ||
          candidateBranchInst->isConditional() == false) {
        continue;
      }

      if (candidateBranchInst == branchInst) {
        passedBranchInst = true;
        continue;
      }

      if (passedBranchInst == false) {
        continue;
      }

      auto candidateBranchInst_leftBB = candidateBranchInst->getOperand(2);
      auto candidateBranchInst_rightBB = candidateBranchInst->getOperand(1);

      if (candidateBranchInst_leftBB == leftBB) {
        if (secondBranchInst) {
          *secondBranchInst = candidateBranchInst;
        }

        return OR_AND_MutationType_OR_to_AND_Pattern1;
      }

      else if (candidateBranchInst_leftBB == rightBB) {
        if (secondBranchInst) {
          *secondBranchInst = candidateBranchInst;
        }

        return OR_AND_MutationType_OR_to_AND_Pattern2;
      }

      for (auto &instruction : *candidateBranchInst->getParent()) {
        PHINode *phiNode = dyn_cast<PHINode>(&instruction);

        if (phiNode == nullptr) {
          continue;
        }

        for (BasicBlock *phiNodeIncomingBB : phiNode->blocks()) {
          if (phiNodeIncomingBB == branchInst->getParent()) {
            continue;
          }

          if (candidateBranchInst->getParent() == leftBB) {
            if (secondBranchInst) {
              *secondBranchInst = candidateBranchInst;
            }

            return OR_AND_MutationType_OR_to_AND_Pattern3;
          }
        }
      }
    }
  }

  return OR_AND_MutationType_None;
}

std::vector<MutationPoint *>
OrAndReplacementMutator::getMutations(Bitcode *bitcode,
                                      const FunctionUnderTest &function) {
  assert(bitcode);

  std::vector<MutationPoint *> mutations;

  for (auto &instruction : instructions(function.getFunction())) {
    OR_AND_MutationType mutationType = findPossibleMutation(instruction);
    if (mutationType == OR_AND_MutationType_None) {
      continue;
    }

    std::string diagnostics = "OR-AND Replacement";
    std::string replacement = getMutationTypeToString(mutationType);

    auto point = new MutationPoint(this, nullptr, &instruction, replacement,
                                   bitcode, diagnostics);
    mutations.push_back(point);
  }

  return mutations;
}

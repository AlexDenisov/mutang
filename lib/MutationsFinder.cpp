#include "MutationsFinder.h"
#include "Context.h"
#include "Filter.h"
#include "Testee.h"
#include "MutationPoint.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>

using namespace mull;
using namespace llvm;

static int GetFunctionIndex(llvm::Function *function) {
  Module *parent = function->getParent();

  auto functionIterator = std::find_if(parent->begin(), parent->end(),
                          [function] (llvm::Function &f) {
                            return &f == function;
                          });

  assert(functionIterator != parent->end()
         && "Expected function to be found in module");
  int index = std::distance(parent->begin(), functionIterator);

  return index;
}

MutationsFinder::MutationsFinder(std::vector<std::unique_ptr<MutationOperator>> operators)
: operators(std::move(operators)) {}

std::vector<MutationPoint *> MutationsFinder::getMutationPoints(const Context &context,
                                                                Testee &testee,
                                                                Filter &filter) {
  std::vector<MutationPoint *> points;

  Function *function = testee.getTesteeFunction();
  auto moduleID = function->getParent()->getModuleIdentifier();
  const MullModule &module = context.moduleWithIdentifier(moduleID);
  assert(!module.isInvalid());

  int functionIndex = GetFunctionIndex(function);
  for (auto &mutationOperator : operators) {

    int basicBlockIndex = 0;
    for (auto &basicBlock : function->getBasicBlockList()) {

      int instructionIndex = 0;
      for (auto &instruction : basicBlock.getInstList()) {
        if (filter.shouldSkipInstruction(&instruction)) {
          continue;
        }

        MutationPointAddress address(functionIndex, basicBlockIndex, instructionIndex);
        MutationPoint *point = mutationOperator->getMutationPoint(module,
                                                                  address,
                                                                  &instruction);
        if (point) {
          points.push_back(point);
          ownedPoints.emplace_back(std::unique_ptr<MutationPoint>(point));
        }
        instructionIndex++;
      }
      basicBlockIndex++;
    }

  }

  return points;
}

#pragma once

#include "Mutator.h"
#include <irm/irm.h>
#include <memory>
#include <mull/ReachableFunction.h>
#include <vector>

namespace llvm {
class Instruction;
}

namespace mull {

class Bitcode;
class MutationPoint;
class MutationPointAddress;
class FunctionUnderTest;

class RemoveVoidFunctionMutator : public Mutator {
public:
  static const std::string ID;
  static const std::string description;

  RemoveVoidFunctionMutator();

  MutatorKind mutatorKind() override {
    return MutatorKind::RemoveVoidFunctionMutator;
  }

  std::string getUniqueIdentifier() override { return ID; }
  std::string getUniqueIdentifier() const override { return ID; }
  std::string getDescription() const override { return description; }

  void applyMutation(llvm::Function *function,
                     const MutationPointAddress &address,
                     irm::IRMutation *lowLevelMutation) override;

  std::vector<MutationPoint *>
  getMutations(Bitcode *bitcode, const FunctionUnderTest &function) override;

private:
  std::vector<std::unique_ptr<irm::IRMutation>> lowLevelMutators;
};
} // namespace mull

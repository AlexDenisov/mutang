#pragma once

#include <functional>
#include <string>
#include <memory>

namespace llvm {

class Function;
class Instruction;
class Value;
class Module;

}

namespace mull {

class Compiler;
class MutationOperator;
class MullModule;

/// \brief Container class that stores information needed to find MutationPoints.
/// We need the indexes of function, basic block and instruction to find the
/// mutation point in the clone of original module, when mutation operator is
/// to apply mutation in that clone.
class MutationPointAddress {
  int FnIndex;
  int BBIndex;
  int IIndex;

  std::string identifier;
public:
  MutationPointAddress(int FnIndex, int BBIndex, int IIndex) :
    FnIndex(FnIndex), BBIndex(BBIndex), IIndex(IIndex) {

    identifier = std::to_string(FnIndex) + "_" +
      std::to_string(BBIndex) + "_" +
      std::to_string(IIndex);
  }

  int getFnIndex() { return FnIndex; }
  int getBBIndex() { return BBIndex; }
  int getIIndex() { return IIndex; }

  std::string getIdentifier() {
    return identifier;
  }

  std::string getIdentifier() const {
    return identifier;
  }

  llvm::Instruction &findInstruction(llvm::Module *module);

  static int getFunctionIndex(llvm::Function *function);
  static
  void enumerateInstructions(llvm::Function &function,
                             const std::function <void (llvm::Instruction &,
                                                        int,
                                                        int)>& block);
};

class MutationPoint {
  MutationOperator *mutationOperator;
  MutationPointAddress Address;
  llvm::Value *OriginalValue;
  const MullModule &module;
  std::string uniqueIdentifier;
  std::string diagnostics;

public:
  MutationPoint(MutationOperator *op,
                MutationPointAddress Address,
                llvm::Value *Val,
                const MullModule &m,
                std::string diagnostics = "");

  ~MutationPoint();

  MutationOperator *getOperator();
  MutationPointAddress getAddress();
  llvm::Value *getOriginalValue();
  const MullModule &getOriginalModule();

  MutationOperator *getOperator() const;
  MutationPointAddress getAddress() const;
  llvm::Value *getOriginalValue() const;
  const MullModule &getOriginalModule() const;

  void applyMutation(MullModule &module);

  std::string getUniqueIdentifier();
  std::string getUniqueIdentifier() const;

  const std::string &getDiagnostics();
  const std::string &getDiagnostics() const;
};

}

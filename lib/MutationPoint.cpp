#include "MutationPoint.h"
#include "Toolchain/Compiler.h"
#include "ModuleLoader.h"

#include "MutationOperators/MutationOperator.h"
#include "llvm/Transforms/Utils/Cloning.h"

using namespace llvm;
using namespace mull;
using namespace std;

#pragma mark - MutationPointAddress

Instruction &MutationPointAddress::findInstruction(Module *module) {
  llvm::Function &function = *(std::next(module->begin(), getFnIndex()));
  llvm::BasicBlock &bb = *(std::next(function.begin(), getBBIndex()));
  llvm::Instruction &instruction = *(std::next(bb.begin(), getIIndex()));

  return instruction;
}

int MutationPointAddress::getFunctionIndex(Function *function) {
  auto PM = function->getParent();

  auto FII = std::find_if(PM->begin(), PM->end(),
                          [function] (llvm::Function &f) {
                            return &f == function;
                          });

  assert(FII != PM->end() && "Expected function to be found in module");
  int FIndex = std::distance(PM->begin(), FII);

  return FIndex;
}

void
MutationPointAddress::enumerateInstructions(
  Function &function, const std::function <void (Instruction &,
                                                 int,
                                                 int)>& block) {

  int basicBlockIndex = 0;
  for (auto &basicBlock : function.getBasicBlockList()) {
    int instructionIndex = 0;

    for (auto &instruction : basicBlock.getInstList()) {
      block(instruction, basicBlockIndex, instructionIndex);

      instructionIndex++;
    }
    basicBlockIndex++;
  }
}

#pragma mark - MutationPoint

MutationPoint::MutationPoint(MutationOperator *op,
                             MutationPointAddress Address,
                             Value *Val,
                             const MullModule &m,
                             std::string diagnostics) :
  mutationOperator(op), Address(Address), OriginalValue(Val), module(m), diagnostics(diagnostics)
{
  string moduleID = module.getUniqueIdentifier();
  string addressID = Address.getIdentifier();
  string operatorID = mutationOperator->uniqueID();

  uniqueIdentifier = moduleID + "_" + addressID + "_" + operatorID;
}

MutationPoint::~MutationPoint() {}

MutationOperator *MutationPoint::getOperator() {
  return mutationOperator;
}

MutationPointAddress MutationPoint::getAddress() {
  return Address;
}

Value *MutationPoint::getOriginalValue() {
  return OriginalValue;
}

const MullModule &MutationPoint::getOriginalModule() {
  return module;
}

MutationOperator *MutationPoint::getOperator() const {
  return mutationOperator;
}

MutationPointAddress MutationPoint::getAddress() const {
  return Address;
}

Value *MutationPoint::getOriginalValue() const {
  return OriginalValue;
}

const MullModule &MutationPoint::getOriginalModule() const {
  return module;
}

void MutationPoint::applyMutation(MullModule &module) {
  mutationOperator->applyMutation(module.getModule(), Address, *OriginalValue);
}

std::string MutationPoint::getUniqueIdentifier() {
  return uniqueIdentifier;
}

std::string MutationPoint::getUniqueIdentifier() const {
  return uniqueIdentifier;
}

const std::string &MutationPoint::getDiagnostics() {
  return diagnostics;
}

const std::string &MutationPoint::getDiagnostics() const {
  return diagnostics;
}

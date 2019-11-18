#include "mull/Mutators/MutatorsFactory.h"

#include "mull/Logger.h"
#include "mull/Mutators/AndOrReplacementMutator.h"
#include "mull/Mutators/CXX/ArithmeticMutators.h"
#include "mull/Mutators/CXX/BitwiseMutators.h"
#include "mull/Mutators/CXX/NumberMutators.h"
#include "mull/Mutators/CXX/RelationalMutators.h"
#include "mull/Mutators/Mutator.h"
#include "mull/Mutators/NegateConditionMutator.h"
#include "mull/Mutators/OrAndReplacementMutator.h"
#include "mull/Mutators/RemoveVoidFunctionMutator.h"
#include "mull/Mutators/ReplaceCallMutator.h"
#include "mull/Mutators/ScalarValueMutator.h"
#include <llvm/ADT/STLExtras.h>
#include <set>
#include <sstream>

using namespace mull;
using namespace std;

static const string MathMutatorsGroup = "math";
static const string ConditionalMutatorsGroup = "conditional";
static const string FunctionsMutatorsGroup = "functions";
static const string ConstantMutatorsGroup = "constant";

static const string ConditionalBoundaryMutatorsGroup =
    "conditionals_boundary_mutator";
static const string NegateRelationalMutatorsGroup = "negate_relational";
static const string ArithmeticMutatorsGroup = "arithmetic";
static const string BitwiseMutatorsGroup = "bitwise";
static const string NumbersMutatorsGroup = "numbers";

static const string DefaultMutatorsGroup = "default";
static const string ExperimentalMutatorsGroup = "experimental";

static const string CXXMutatorsGroup = "cxx";

static const string AllMutatorsGroup = "all";

static void expandGroups(const vector<string> &groups,
                         const map<string, vector<string>> &mapping,
                         set<string> &expandedGroups) {
  for (const string &group : groups) {
    if (mapping.count(group) == 0) {
      expandedGroups.insert(group);
      continue;
    }
    expandGroups(mapping.at(group), mapping, expandedGroups);
  }
}

MutatorsFactory::MutatorsFactory() {
  groupsMapping[ConditionalMutatorsGroup] = {
      AndOrReplacementMutator::ID, OrAndReplacementMutator::ID,
      NegateConditionMutator::ID, ConditionalBoundaryMutatorsGroup,
      NegateRelationalMutatorsGroup};
  groupsMapping[MathMutatorsGroup] = {cxx::AddToSub::ID, cxx::SubToAdd::ID,
                                      cxx::MulToDiv::ID, cxx::DivToMul::ID};
  groupsMapping[FunctionsMutatorsGroup] = {ReplaceCallMutator::ID,
                                           RemoveVoidFunctionMutator::ID};
  groupsMapping[ConstantMutatorsGroup] = {ScalarValueMutator::ID};
  groupsMapping[DefaultMutatorsGroup] = {cxx::AddToSub::ID,
                                         NegateConditionMutator::ID,
                                         RemoveVoidFunctionMutator::ID};
  groupsMapping[ExperimentalMutatorsGroup] = {
      AndOrReplacementMutator::ID,   OrAndReplacementMutator::ID,
      NumbersMutatorsGroup,          ReplaceCallMutator::ID,
      ScalarValueMutator::ID,        ConditionalBoundaryMutatorsGroup,
      NegateRelationalMutatorsGroup, ArithmeticMutatorsGroup,
      BitwiseMutatorsGroup,
  };
  groupsMapping[CXXMutatorsGroup] = {
      ConditionalBoundaryMutatorsGroup,
      NegateRelationalMutatorsGroup,
      ArithmeticMutatorsGroup,
      NumbersMutatorsGroup,
  };
  groupsMapping[AllMutatorsGroup] = {DefaultMutatorsGroup,
                                     ExperimentalMutatorsGroup};

  groupsMapping[ConditionalBoundaryMutatorsGroup] = {
      cxx::LessOrEqualToLessThan::ID,
      cxx::LessThanToLessOrEqual::ID,
      cxx::GreaterOrEqualToGreaterThan::ID,
      cxx::GreaterThanToGreaterOrEqual::ID,
  };

  groupsMapping[NegateRelationalMutatorsGroup] = {
      cxx::GreaterThanToLessOrEqual::ID, cxx::GreaterOrEqualToLessThan::ID,
      cxx::LessThanToGreaterOrEqual::ID, cxx::LessOrEqualToGreaterThan::ID,
      cxx::EqualToNotEqual::ID,          cxx::NotEqualToEqual::ID,
  };
  groupsMapping[ArithmeticMutatorsGroup] = {
      cxx::AddToSub::ID,         cxx::AddAssignToSubAssign::ID,
      cxx::PostIncToPostDec::ID, cxx::PreIncToPreDec::ID,

      cxx::SubToAdd::ID,         cxx::SubAssignToAddAssign::ID,
      cxx::PostDecToPostInc::ID, cxx::PreDecToPreInc::ID,

      cxx::MulToDiv::ID,         cxx::MulAssignToDivAssign::ID,
      cxx::DivToMul::ID,         cxx::DivAssignToMulAssign::ID,
      cxx::RemToDiv::ID,         cxx::RemAssignToDivAssign::ID,
  };

  groupsMapping[BitwiseMutatorsGroup] = {
      cxx::LShiftToRShift::ID, cxx::LShiftAssignToRShiftAssign::ID,
      cxx::RShiftToLShift::ID, cxx::RShiftAssignToLShiftAssign::ID,

      cxx::AndToOr::ID,        cxx::AndAssignToOrAssign::ID,
      cxx::OrToAnd::ID,        cxx::OrAssignToAndAssign::ID,
      cxx::XorToOr::ID,        cxx::XorAssignToOrAssign::ID,
  };

  groupsMapping[NumbersMutatorsGroup] = {cxx::NumberInitConst::ID,
                                         cxx::NumberAssignConst::ID};
}

template <typename MutatorClass>
void addMutator(std::map<std::string, std::unique_ptr<Mutator>> &mapping) {
  mapping[MutatorClass::ID] = make_unique<MutatorClass>();
}

void MutatorsFactory::init() {
  addMutator<NegateConditionMutator>(mutatorsMapping);

  addMutator<AndOrReplacementMutator>(mutatorsMapping);
  addMutator<OrAndReplacementMutator>(mutatorsMapping);

  addMutator<ReplaceCallMutator>(mutatorsMapping);
  addMutator<RemoveVoidFunctionMutator>(mutatorsMapping);
  addMutator<ScalarValueMutator>(mutatorsMapping);

  addMutator<cxx::LessOrEqualToLessThan>(mutatorsMapping);
  addMutator<cxx::LessThanToLessOrEqual>(mutatorsMapping);
  addMutator<cxx::GreaterOrEqualToGreaterThan>(mutatorsMapping);
  addMutator<cxx::GreaterThanToGreaterOrEqual>(mutatorsMapping);

  addMutator<cxx::EqualToNotEqual>(mutatorsMapping);
  addMutator<cxx::NotEqualToEqual>(mutatorsMapping);

  addMutator<cxx::LessOrEqualToGreaterThan>(mutatorsMapping);
  addMutator<cxx::LessThanToGreaterOrEqual>(mutatorsMapping);
  addMutator<cxx::GreaterOrEqualToLessThan>(mutatorsMapping);
  addMutator<cxx::GreaterThanToLessOrEqual>(mutatorsMapping);

  addMutator<cxx::AddToSub>(mutatorsMapping);
  addMutator<cxx::AddAssignToSubAssign>(mutatorsMapping);
  addMutator<cxx::PreIncToPreDec>(mutatorsMapping);
  addMutator<cxx::PostIncToPostDec>(mutatorsMapping);

  addMutator<cxx::SubToAdd>(mutatorsMapping);
  addMutator<cxx::SubAssignToAddAssign>(mutatorsMapping);
  addMutator<cxx::PreDecToPreInc>(mutatorsMapping);
  addMutator<cxx::PostDecToPostInc>(mutatorsMapping);

  addMutator<cxx::MulToDiv>(mutatorsMapping);
  addMutator<cxx::MulAssignToDivAssign>(mutatorsMapping);

  addMutator<cxx::DivToMul>(mutatorsMapping);
  addMutator<cxx::DivAssignToMulAssign>(mutatorsMapping);

  addMutator<cxx::RemToDiv>(mutatorsMapping);
  addMutator<cxx::RemAssignToDivAssign>(mutatorsMapping);

  addMutator<cxx::LShiftToRShift>(mutatorsMapping);
  addMutator<cxx::LShiftAssignToRShiftAssign>(mutatorsMapping);
  addMutator<cxx::RShiftToLShift>(mutatorsMapping);
  addMutator<cxx::RShiftAssignToLShiftAssign>(mutatorsMapping);

  addMutator<cxx::AndToOr>(mutatorsMapping);
  addMutator<cxx::AndAssignToOrAssign>(mutatorsMapping);
  addMutator<cxx::OrToAnd>(mutatorsMapping);
  addMutator<cxx::OrAssignToAndAssign>(mutatorsMapping);
  addMutator<cxx::XorToOr>(mutatorsMapping);
  addMutator<cxx::XorAssignToOrAssign>(mutatorsMapping);

  addMutator<cxx::NumberAssignConst>(mutatorsMapping);
  addMutator<cxx::NumberInitConst>(mutatorsMapping);
}

vector<unique_ptr<Mutator>>
MutatorsFactory::mutators(const vector<string> &groups) {
  /// We need to recreate all mutators in case this method called
  /// more than once. It does not happen during normal program execution,
  /// but happens a lot during testing
  init();

  set<string> expandedGroups;

  if (groups.empty()) {
    expandGroups({DefaultMutatorsGroup}, groupsMapping, expandedGroups);
  } else {
    expandGroups(groups, groupsMapping, expandedGroups);
  }

  vector<unique_ptr<Mutator>> mutators;

  for (const string &group : expandedGroups) {
    if (mutatorsMapping.count(group) == 0) {
      Logger::error() << "Unknown mutator: '" << group << "'\n";
      continue;
    }
    mutators.emplace_back(std::move(mutatorsMapping.at(group)));
    mutatorsMapping.erase(group);
  }

  if (mutators.empty()) {
    Logger::error() << "No valid mutators found in a config file.\n";
  }

  return mutators;
}

/// Command Line Options

std::string descriptionForGroup(const std::vector<std::string> &groupMembers) {
  if (groupMembers.empty()) {
    return std::string("empty group?");
  }

  std::stringstream members;
  std::copy(groupMembers.begin(), groupMembers.end() - 1,
            std::ostream_iterator<std::string>(members, ", "));
  members << *(groupMembers.end() - 1);

  return members.str();
}

#include <iostream>

std::vector<std::pair<std::string, std::string>>
MutatorsFactory::commandLineOptions() {
  std::vector<std::pair<std::string, std::string>> options;
  for (auto &group : groupsMapping) {
    options.emplace_back(group.first, descriptionForGroup(group.second));
  }

  std::set<std::string> mutatorsSet;
  std::vector<std::string> groups({AllMutatorsGroup});
  expandGroups({AllMutatorsGroup}, groupsMapping, mutatorsSet);

  auto allMutators = mutators({AllMutatorsGroup});

  for (auto &mutator : allMutators) {
    options.emplace_back(mutator->getUniqueIdentifier(),
                         mutator->getDescription());
  }

  return options;
}

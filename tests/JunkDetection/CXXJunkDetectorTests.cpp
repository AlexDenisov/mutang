#include "FixturePaths.h"
#include "mull/BitcodeLoader.h"
#include "mull/Config/Configuration.h"
#include "mull/Config/RawConfig.h"
#include "mull/Filter.h"
#include "mull/JunkDetection/CXX/CXXJunkDetector.h"
#include "mull/MutationPoint.h"
#include "mull/MutationsFinder.h"
#include "mull/Mutators/ConditionalsBoundaryMutator.h"
#include "mull/Mutators/MathAddMutator.h"
#include "mull/Mutators/MathDivMutator.h"
#include "mull/Mutators/MathMulMutator.h"
#include "mull/Mutators/MathSubMutator.h"
#include "mull/Mutators/NegateConditionMutator.h"
#include "mull/Mutators/RemoveVoidFunctionMutator.h"
#include "mull/Mutators/ReplaceCallMutator.h"
#include "mull/Program/Program.h"
#include "mull/Testee.h"
#include "mull/Toolchain/Compiler.h"
#include "mull/Toolchain/Toolchain.h"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <mull/Mutators/AndOrReplacementMutator.h>

#include "gtest/gtest.h"

using namespace mull;
using namespace llvm;

using ::testing::TestWithParam;
using ::testing::Values;

struct CXXJunkDetectorTestParameter {
  const char *bitcodePath;
  Mutator *mutator;
  uint8_t totalMutants;
  uint8_t nonJunkMutants;
  CXXJunkDetectorTestParameter(const char *bitcodePath, Mutator *mutator,
                               uint8_t totalMutants, uint8_t nonJunkMutants)
      : bitcodePath(bitcodePath), mutator(mutator), totalMutants(totalMutants),
        nonJunkMutants(nonJunkMutants) {}

  friend std::ostream &operator<<(std::ostream &os,
                                  const CXXJunkDetectorTestParameter &bar) {
    os << "path(" << bar.bitcodePath << ") mutator("
       << bar.mutator->getUniqueIdentifier() << ") mutants("
       << std::to_string(bar.nonJunkMutants) << "/"
       << std::to_string(bar.totalMutants) << ")";
    return os;
  }
};

class CXXJunkDetectorTest : public TestWithParam<CXXJunkDetectorTestParameter> {
};

TEST_P(CXXJunkDetectorTest, detectJunk) {
  auto &parameter = GetParam();
  LLVMContext llvmContext;
  BitcodeLoader loader;
  auto bitcodeFile =
      loader.loadBitcodeAtPath(parameter.bitcodePath, llvmContext);
  auto module = bitcodeFile->getModule();

  std::vector<std::unique_ptr<Bitcode>> bitcode;
  bitcode.push_back(std::move(bitcodeFile));
  Program program({}, {}, std::move(bitcode));
  Configuration configuration;

  std::vector<std::unique_ptr<Mutator>> mutators;
  mutators.emplace_back(std::move(std::unique_ptr<Mutator>(parameter.mutator)));
  MutationsFinder finder(std::move(mutators), configuration);
  Filter filter;

  std::vector<std::unique_ptr<Testee>> testees;
  for (auto &function : *module) {
    testees.emplace_back(make_unique<Testee>(&function, nullptr, 1));
  }
  auto mergedTestees = mergeTestees(testees);

  std::vector<MutationPoint *> points =
      finder.getMutationPoints(program, mergedTestees, filter);

  ASSERT_EQ(points.size(), parameter.totalMutants);

  JunkDetectionConfig junkConfig;

  ASTStorage astStorage(junkConfig.cxxCompilationDatabasePath,
                        junkConfig.cxxCompilationFlags);
  CXXJunkDetector detector(astStorage);

  std::vector<MutationPoint *> nonJunkMutationPoints;
  for (auto point : points) {
    if (!detector.isJunk(point)) {
      nonJunkMutationPoints.push_back(point);
    }
  }

  ASSERT_EQ(nonJunkMutationPoints.size(), parameter.nonJunkMutants);
}

static const CXXJunkDetectorTestParameter parameters[] = {
    CXXJunkDetectorTestParameter(fixtures::mutators_boundary_module_bc_path(),
                                 new ConditionalsBoundaryMutator, 7, 6),
    CXXJunkDetectorTestParameter(fixtures::mutators_math_add_module_bc_path(),
                                 new MathAddMutator, 17, 16),
    CXXJunkDetectorTestParameter(fixtures::mutators_math_mul_junk_bc_path(),
                                 new MathMulMutator, 11, 10),
    CXXJunkDetectorTestParameter(fixtures::mutators_math_div_junk_bc_path(),
                                 new MathDivMutator, 11, 10),

    /// FIXME: Here MathSub should produce 14 mutants, but only 9 found because
    /// some of the '--/-= 1' instructions converted into 'add -1'
    /// There are three possible solutions:
    ///   1. Consider 'add' and 'sub' instruction as MathSub and filter out junk
    ///      later. This way we must rely on JunkDetector, otherwise we get lots
    ///      of duplicates
    ///   2. Add an extra check to the MathSub, i.e. an instruction is MathSub
    ///      if it is 'add' and one of the operands is '-1'. Then, to apply a
    ///      mutation we would need to replace -1 with 1.
    ///   3. Merge MathAdd and MathSub into one mutator. Less granulation, but
    ///      higher quality of mutants
    ///
    /// At the moment of writing the second option more.
    ///
    CXXJunkDetectorTestParameter(fixtures::mutators_math_sub_junk_bc_path(),
                                 new MathSubMutator, 9, 8),
    CXXJunkDetectorTestParameter(
        fixtures::mutators_remove_void_function_junk_bc_path(),
        new RemoveVoidFunctionMutator, 9, 6),
    CXXJunkDetectorTestParameter(fixtures::mutators_replace_call_junk_bc_path(),
                                 new ReplaceCallMutator, 12, 11),

    CXXJunkDetectorTestParameter(
        fixtures::mutators_negate_condition_junk_bc_path(),
        new NegateConditionMutator, 42, 30),
    CXXJunkDetectorTestParameter(
        fixtures::mutators_and_or_replacement_cpp_junk_bc_path(),
        new AndOrReplacementMutator, 6, 4)};

INSTANTIATE_TEST_CASE_P(CXXJunkDetection, CXXJunkDetectorTest,
                        testing::ValuesIn(parameters));

TEST(CXXJunkDetector, compdb_absolute_paths) {
  LLVMContext llvmContext;
  BitcodeLoader loader;
  auto bitcodeFile = loader.loadBitcodeAtPath(
      fixtures::junk_detection_compdb_main_bc_path(), llvmContext);
  auto module = bitcodeFile->getModule();

  std::vector<std::unique_ptr<Bitcode>> bitcode;
  bitcode.push_back(std::move(bitcodeFile));
  Program program({}, {}, std::move(bitcode));
  Configuration configuration;

  std::vector<std::unique_ptr<Mutator>> mutators;
  mutators.emplace_back(make_unique<ConditionalsBoundaryMutator>());
  MutationsFinder finder(std::move(mutators), configuration);
  Filter filter;

  std::vector<std::unique_ptr<Testee>> testees;
  for (auto &function : *module) {
    testees.emplace_back(make_unique<Testee>(&function, nullptr, 1));
  }
  auto mergedTestees = mergeTestees(testees);
  std::vector<MutationPoint *> points =
      finder.getMutationPoints(program, mergedTestees, filter);

  ASSERT_EQ(points.size(), 8U);

  JunkDetectionConfig junkConfig;
  junkConfig.cxxCompilationDatabasePath =
      fixtures::junk_detection_compdb_absolute_compile_commands_json_path();

  ASTStorage astStorage(junkConfig.cxxCompilationDatabasePath,
                        junkConfig.cxxCompilationFlags);
  CXXJunkDetector detector(astStorage);

  std::vector<MutationPoint *> nonJunkMutationPoints;
  for (auto point : points) {
    if (!detector.isJunk(point)) {
      nonJunkMutationPoints.push_back(point);
    }
  }

  ASSERT_EQ(nonJunkMutationPoints.size(), 7U);
}

TEST(CXXJunkDetector, DISABLED_compdb_relative_paths) {
  LLVMContext llvmContext;
  BitcodeLoader loader;
  auto bitcodeFile = loader.loadBitcodeAtPath(
      fixtures::junk_detection_compdb_main_bc_path(), llvmContext);
  auto module = bitcodeFile->getModule();

  std::vector<std::unique_ptr<Bitcode>> bitcode;
  bitcode.push_back(std::move(bitcodeFile));
  Program program({}, {}, std::move(bitcode));
  Configuration configuration;

  std::vector<std::unique_ptr<Mutator>> mutators;
  mutators.emplace_back(make_unique<ConditionalsBoundaryMutator>());
  MutationsFinder finder(std::move(mutators), configuration);
  Filter filter;

  std::vector<std::unique_ptr<Testee>> testees;
  for (auto &function : *module) {
    testees.emplace_back(make_unique<Testee>(&function, nullptr, 1));
  }
  auto mergedTestees = mergeTestees(testees);

  std::vector<MutationPoint *> points =
      finder.getMutationPoints(program, mergedTestees, filter);

  ASSERT_EQ(points.size(), 8U);

  JunkDetectionConfig junkConfig;
  junkConfig.cxxCompilationDatabasePath =
      fixtures::junk_detection_compdb_relative_compile_commands_json_path();

  ASTStorage astStorage(junkConfig.cxxCompilationDatabasePath,
                        junkConfig.cxxCompilationFlags);
  CXXJunkDetector detector(astStorage);

  std::vector<MutationPoint *> nonJunkMutationPoints;
  for (auto point : points) {
    if (!detector.isJunk(point)) {
      nonJunkMutationPoints.push_back(point);
    }
  }

  ASSERT_EQ(nonJunkMutationPoints.size(), 7U);
}

TEST(CXXJunkDetector, no_compdb) {
  LLVMContext llvmContext;
  BitcodeLoader loader;
  auto bitcodeFile = loader.loadBitcodeAtPath(
      fixtures::junk_detection_compdb_main_bc_path(), llvmContext);
  auto module = bitcodeFile->getModule();

  std::vector<std::unique_ptr<Bitcode>> bitcode;
  bitcode.push_back(std::move(bitcodeFile));
  Program program({}, {}, std::move(bitcode));
  Configuration configuration;

  std::vector<std::unique_ptr<Mutator>> mutatorss;
  mutatorss.emplace_back(make_unique<ConditionalsBoundaryMutator>());
  MutationsFinder finder(std::move(mutatorss), configuration);
  Filter filter;

  std::vector<std::unique_ptr<Testee>> testees;
  for (auto &function : *module) {
    testees.emplace_back(make_unique<Testee>(&function, nullptr, 1));
  }
  auto mergedTestees = mergeTestees(testees);

  std::vector<MutationPoint *> points =
      finder.getMutationPoints(program, mergedTestees, filter);

  ASSERT_EQ(points.size(), 8U);

  JunkDetectionConfig junkConfig;
  junkConfig.cxxCompilationFlags =
      std::string("-I ") + fixtures::junk_detection_compdb_include__path();

  ASTStorage astStorage(junkConfig.cxxCompilationDatabasePath,
                        junkConfig.cxxCompilationFlags);
  CXXJunkDetector detector(astStorage);

  std::vector<MutationPoint *> nonJunkMutationPoints;
  for (auto point : points) {
    if (!detector.isJunk(point)) {
      nonJunkMutationPoints.push_back(point);
    }
  }

  ASSERT_EQ(nonJunkMutationPoints.size(), 7U);
}

#include "GoogleTest/GoogleTestFinder.h"

#include "Driver.h"
#include "Context.h"
#include "Config.h"
#include "ConfigParser.h"
#include "MutationOperators/AddMutationOperator.h"
#include "TestModuleFactory.h"
#include "GoogleTest/GoogleTest_Test.h"

#include "llvm/IR/CallSite.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/YAMLTraits.h"

#include "gtest/gtest.h"

using namespace mull;
using namespace llvm;

static TestModuleFactory TestModuleFactory;

#pragma mark - Finding Tests

TEST(GoogleTestFinder, FindTest) {
  auto ModuleWithTests       = TestModuleFactory.createGoogleTestTesterModule();
  auto mullModuleWithTests = make_unique<MullModule>(std::move(ModuleWithTests), "");

  Context Ctx;
  Ctx.addModule(std::move(mullModuleWithTests));

  const char *configYAML = R"YAML(
mutation_operators:
- add_mutation_operator
- negate_mutation_operator
- remove_void_function_mutation_operator
)YAML";
  
  yaml::Input Input(configYAML);
  
  ConfigParser Parser;
  auto Cfg = Parser.loadConfig(Input);

  auto mutationOperators = Driver::mutationOperators(Cfg.getMutationOperators());
  GoogleTestFinder Finder(std::move(mutationOperators), {}, {});

  auto tests = Finder.findTests(Ctx);

  ASSERT_EQ(2U, tests.size());

  GoogleTest_Test *Test1 = dyn_cast<GoogleTest_Test>(tests[0].get());
  ASSERT_EQ("HelloTest.testSumOfTestee", Test1->getTestName());

  GoogleTest_Test *Test2 = dyn_cast<GoogleTest_Test>(tests[1].get());
  ASSERT_EQ("HelloTest.testSumOfTestee2", Test2->getTestName());
}

TEST(GoogleTestFinder, findTests_filter) {
  auto ModuleWithTests       = TestModuleFactory.createGoogleTestTesterModule();
  auto mullModuleWithTests = make_unique<MullModule>(std::move(ModuleWithTests), "");

  Context Ctx;
  Ctx.addModule(std::move(mullModuleWithTests));

  const char *configYAML = R"YAML(
mutation_operators:
  - add_mutation_operator
  - negate_mutation_operator
  - remove_void_function_mutation_operator
  )YAML";

  yaml::Input Input(configYAML);

  ConfigParser Parser;
  auto Cfg = Parser.loadConfig(Input);

  auto mutationOperators = Driver::mutationOperators(Cfg.getMutationOperators());

  GoogleTestFinder Finder(std::move(mutationOperators),
                          { "HelloTest.testSumOfTestee" },
                          {});

  auto tests = Finder.findTests(Ctx);

  ASSERT_EQ(1U, tests.size());
  GoogleTest_Test *Test1 = dyn_cast<GoogleTest_Test>(tests[0].get());
  ASSERT_EQ("HelloTest.testSumOfTestee", Test1->getTestName());
}

#pragma mark - Finding Testees

TEST(GoogleTestFinder, findTestees) {
  auto ModuleWithTests   = TestModuleFactory.createGoogleTestTesterModule();
  auto ModuleWithTestees = TestModuleFactory.createGoogleTestTesteeModule();

  auto mullModuleWithTests   = make_unique<MullModule>(std::move(ModuleWithTests), "");
  auto mullModuleWithTestees = make_unique<MullModule>(std::move(ModuleWithTestees), "");

  Context Ctx;
  Ctx.addModule(std::move(mullModuleWithTests));
  Ctx.addModule(std::move(mullModuleWithTestees));
  
  const char *configYAML = R"YAML(
mutation_operators:
  - add_mutation_operator
  )YAML";
  
  yaml::Input Input(configYAML);
  
  ConfigParser Parser;
  auto Cfg = Parser.loadConfig(Input);

  auto mutationOperators = Driver::mutationOperators(Cfg.getMutationOperators());
  GoogleTestFinder Finder(std::move(mutationOperators), {}, {});
  
  auto Tests = Finder.findTests(Ctx);

  ASSERT_EQ(2U, Tests.size());

  auto &Test = *(Tests.begin());

  std::vector<std::unique_ptr<Testee>> Testees = Finder.findTestees(Test.get(), Ctx, 4);

  ASSERT_EQ(2U, Testees.size());

  Function *Testee = Testees[0]->getTesteeFunction();
  ASSERT_FALSE(Testee->empty());

  ASSERT_EQ(0, Testees[0]->getDistance());
  ASSERT_EQ(1, Testees[1]->getDistance());
}

TEST(GoogleTestFinder, findTestees_testeesAsInvokeInstr) {
  auto module =
    TestModuleFactory.createGoogleTestFinder_invokeInstTestee_Module();

  auto mullModule = make_unique<MullModule>(std::move(module), "");

  Context Ctx;
  Ctx.addModule(std::move(mullModule));

  const char *configYAML = R"YAML(
mutation_operators:
  - add_mutation_operator
  )YAML";

  yaml::Input Input(configYAML);

  ConfigParser Parser;
  auto Cfg = Parser.loadConfig(Input);

  auto mutationOperators = Driver::mutationOperators(Cfg.getMutationOperators());
  GoogleTestFinder Finder(std::move(mutationOperators), {}, {});

  auto Tests = Finder.findTests(Ctx);

  ASSERT_EQ(1U, Tests.size());

  auto &Test = *(Tests.begin());

  std::vector<std::unique_ptr<Testee>> Testees = Finder.findTestees(Test.get(), Ctx, 4);
  ASSERT_EQ(2U, Testees.size());

  // the first testee is always test itself, its test body.
  Function *testee1 = Testees[0]->getTesteeFunction();
  ASSERT_EQ(testee1->getName().str(), "_ZN44HelloTest_testTesteeCalledAsInvokeInstr_Test8TestBodyEv");

  Function *testee2 = Testees[1]->getTesteeFunction();
  ASSERT_EQ(testee2->getName().str(), "_ZL6testeev");
}

TEST(GoogleTestFinder, findTestees_with_excludeLocations) {
  auto ModuleWithTests   = TestModuleFactory.createGoogleTestTesterModule();
  auto ModuleWithTestees = TestModuleFactory.createGoogleTestTesteeModule();

  auto mullModuleWithTests   = make_unique<MullModule>(std::move(ModuleWithTests), "");
  auto mullModuleWithTestees = make_unique<MullModule>(std::move(ModuleWithTestees), "");

  Context Ctx;
  Ctx.addModule(std::move(mullModuleWithTests));
  Ctx.addModule(std::move(mullModuleWithTestees));

  const char *configYAML = "";
  yaml::Input Input(configYAML);

  ConfigParser Parser;
  auto Cfg = Parser.loadConfig(Input);

  auto mutationOperators = Driver::mutationOperators(Cfg.getMutationOperators());
  GoogleTestFinder Finder(std::move(mutationOperators), {}, { "Testee.cpp" });

  auto Tests = Finder.findTests(Ctx);

  ASSERT_EQ(2U, Tests.size());

  auto &Test = *(Tests.begin());

  std::vector<std::unique_ptr<Testee>> Testees = Finder.findTestees(Test.get(), Ctx, 4);

  // Test itself is always the first testee - it is not filtered out.
  // But the second testee *is* filtered out so having 1 instead of 2.
  ASSERT_EQ(1U, Testees.size());
}

#pragma mark - Finding Mutation Points

TEST(GoogleTestFinder, findMutationPoints) {
  auto ModuleWithTests   = TestModuleFactory.createGoogleTestTesterModule();
  auto ModuleWithTestees = TestModuleFactory.createGoogleTestTesteeModule();

  auto mullModuleWithTests   = make_unique<MullModule>(std::move(ModuleWithTests), "");
  auto mullModuleWithTestees = make_unique<MullModule>(std::move(ModuleWithTestees), "");

  Context Ctx;
  Ctx.addModule(std::move(mullModuleWithTests));
  Ctx.addModule(std::move(mullModuleWithTestees));

  const char *configYAML = R"YAML(
mutation_operators:
  - add_mutation_operator
  )YAML";

  yaml::Input Input(configYAML);

  ConfigParser Parser;
  auto Cfg = Parser.loadConfig(Input);

  auto mutationOperators = Driver::mutationOperators(Cfg.getMutationOperators());
  ASSERT_EQ(mutationOperators.size(), 1U);

  GoogleTestFinder Finder(std::move(mutationOperators), {}, {});

  auto Tests = Finder.findTests(Ctx);

  ASSERT_EQ(2U, Tests.size());

  auto &Test = *Tests.begin();

  std::vector<std::unique_ptr<Testee>> Testees = Finder.findTestees(Test.get(), Ctx, 4);

  ASSERT_EQ(2U, Testees.size());

  Function *Testee = Testees[1]->getTesteeFunction();

  ASSERT_FALSE(Testee->empty());
  ASSERT_EQ(Testee->getName().str(), "_ZN6Testee3sumEii");

  std::vector<IMutationPoint *> MutationPoints =
    Finder.findMutationPoints(Ctx, *Testee);
  
  ASSERT_EQ(1U, MutationPoints.size());

  IMutationPoint *MP = MutationPoints[0];

  MutationPointAddress MPA = MP->getAddress();
  ASSERT_EQ(MPA.getFnIndex(), 0);
  ASSERT_EQ(MPA.getBBIndex(), 0);
  ASSERT_EQ(MPA.getIIndex(), 14);
}

TEST(GoogleTestFinder, findMutationPoints_excludeLocations) {
  auto ModuleWithTests   = TestModuleFactory.createGoogleTestTesterModule();
  auto ModuleWithTestees = TestModuleFactory.createGoogleTestTesteeModule();

  auto mullModuleWithTests   = make_unique<MullModule>(std::move(ModuleWithTests), "");
  auto mullModuleWithTestees = make_unique<MullModule>(std::move(ModuleWithTestees), "");

  Context Ctx;
  Ctx.addModule(std::move(mullModuleWithTests));
  Ctx.addModule(std::move(mullModuleWithTestees));

  const char *configYAML = R"YAML(
mutation_operators:
  - add_mutation_operator
  )YAML";

  yaml::Input Input(configYAML);

  ConfigParser Parser;
  auto Cfg = Parser.loadConfig(Input);

  auto mutationOperators = Driver::mutationOperators(Cfg.getMutationOperators());
  ASSERT_EQ(mutationOperators.size(), 1U);

  GoogleTestFinder Finder(std::move(mutationOperators), {}, {});

  auto Tests = Finder.findTests(Ctx);

  ASSERT_EQ(2U, Tests.size());

  auto &Test = *Tests.begin();

  std::vector<std::unique_ptr<Testee>> Testees = Finder.findTestees(Test.get(), Ctx, 4);

  ASSERT_EQ(2U, Testees.size());

  Function *Testee = Testees[1]->getTesteeFunction();

  ASSERT_FALSE(Testee->empty());
  ASSERT_EQ(Testee->getName().str(), "_ZN6Testee3sumEii");

  // Now we create another find with excludeLocations set to "- Testee.cpp"
  auto mutationOperators2 = Driver::mutationOperators(Cfg.getMutationOperators());
  ASSERT_EQ(mutationOperators2.size(), 1U);

  GoogleTestFinder finderWithExcludedLocations(std::move(mutationOperators2),
                                               {},
                                               { "Testee.cpp" });

  std::vector<IMutationPoint *> mutationPoints =
    finderWithExcludedLocations.findMutationPoints(Ctx, *Testee);

  ASSERT_EQ(0U, mutationPoints.size());
}

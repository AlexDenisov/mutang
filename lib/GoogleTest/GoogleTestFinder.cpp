#include "GoogleTest/GoogleTestFinder.h"

#include "Context.h"
#include "Logger.h"
#include "MutationOperators/MutationOperator.h"

#include "llvm/IR/CallSite.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "GoogleTest/GoogleTest_Test.h"

#include "MutationOperators/AddMutationOperator.h"
#include "MutationOperators/NegateConditionMutationOperator.h"
#include "MutationOperators/RemoveVoidFunctionMutationOperator.h"

#include <queue>
#include <set>
#include <vector>

#include <cxxabi.h>

using namespace mull;
using namespace llvm;

GoogleTestFinder::GoogleTestFinder(
  std::vector<std::unique_ptr<MutationOperator>> mutationOperators,
  std::vector<std::string> testsToFilter,
  std::vector<std::string> excludeLocations)
  : TestFinder(),
  mutationOperators(std::move(mutationOperators)),
  filter(GoogleTestMutationOperatorFilter(testsToFilter, excludeLocations))
{

}

/// The algorithm is the following:
///
/// Each test has an instance of type TestInfo associated with it
/// This instance is stored in a static variable
/// Before program starts this variable gets instantiated by registering
/// in the system.
///
/// The registration made by the following function:
///
///    TestInfo* internal::MakeAndRegisterTestInfo(
///              const char* test_case_name, const char* name,
///              const char* type_param,
///              const char* value_param,
///              internal::TypeId fixture_class_id,
///              Test::SetUpTestCaseFunc set_up_tc,
///              Test::TearDownTestCaseFunc tear_down_tc,
///              internal::TestFactoryBase* factory);
///
/// Here we particularly interested in the values of `test_case_name` and `name`
/// Note:
///   Values `type_param` and `value_param` are also important,
///   but ignored for the moment because we do not support Typed and
///   Value Parametrized tests yet.
///
/// To extract this information we need to find a global
/// variable of type `class.testing::TestInfo`, then find its usage (it used
/// only once), from this usage extract a call to
/// function `MakeAndRegisterTestInfo`, from the call extract values of
/// first and second parameters.
/// Concatenation of thsi parameters is exactly the test name.
/// Note: except of Typed and Value Prametrized Tests
///

std::vector<std::unique_ptr<Test>> GoogleTestFinder::findTests(Context &Ctx) {
  std::vector<std::unique_ptr<Test>> tests;

  auto testInfoTypeName = StringRef("class.testing::TestInfo");

  for (auto &currentModule : Ctx.getModules()) {
    for (auto &globalValue : currentModule->getModule()->getGlobalList()) {
      Type *globalValueType = globalValue.getValueType();
      if (globalValueType->getTypeID() != Type::PointerTyID) {
        continue;
      }

      /// Downgrading from LLVM 4.0 to 3.9:
      /// in 4.0 the pointer type is used instead of sequential type.
      // - Type *globalType = Ty->getPointerElementType();
      // - if (!globalType) {
      // -   continue;
      // - }
      // - StructType *STy = dyn_cast<StructType>(globalType);
      Type *sequentialType = globalValueType->getSequentialElementType();
      if (!sequentialType) {
        continue;
      }

      StructType *structType = dyn_cast<StructType>(sequentialType);
      if (!structType) {
        continue;
      }

      /// If two modules contain the same type, then when second modules is loaded
      /// the typename is changed a bit, e.g.:
      ///
      ///   class.testing::TestInfo     // type from first module
      ///   class.testing::TestInfo.25  // type from second module
      ///
      /// Hence we cannot just compare string, and rather should
      /// compare the beginning of the typename

      if (!structType->getName().startswith(testInfoTypeName)) {
        continue;
      }

      /// Normally the globalValue has only one usage, ut LLVM could add
      /// intrinsics such as @llvm.invariant.start
      /// We need to find a user that is a store instruction, which is
      /// a part of initialization function
      /// It looks like this:
      ///
      ///   store %"class.testing::TestInfo"* %call2, %"class.testing::TestInfo"** @_ZN16Hello_world_Test10test_info_E
      ///
      /// From here we need to extract actual user, which is a `store` instruction
      /// The `store` instruction uses variable `%call2`, which is created
      /// from the following code:
      ///
      ///   %call2 = call %"class.testing::TestInfo"* @_ZN7testing8internal23MakeAndRegisterTestInfoEPKcS2_S2_S2_PKvPFvvES6_PNS0_15TestFactoryBaseE(i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.str, i32 0, i32 0), i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.1, i32 0, i32 0), i8* null, i8* null, i8* %call, void ()* @_ZN7testing4Test13SetUpTestCaseEv, void ()* @_ZN7testing4Test16TearDownTestCaseEv, %"class.testing::internal::TestFactoryBase"* %1)
      ///
      /// Which can be roughly simplified to the following pseudo-code:
      ///
      ///   testInfo = MakeAndRegisterTestInfo("Test Suite Name",
      ///                                      "Test Case Name",
      ///                                      setUpFunctionPtr,
      ///                                      tearDownFunctionPtr,
      ///                                      some_other_ignored_parameters)
      ///
      /// Where `testInfo` is exactly the `%call2` from above.
      /// From the `MakeAndRegisterTestInfo` we need to extract test suite
      /// and test case names. Having those in place it's possible to provide
      /// correct filter for GoogleTest framework
      ///
      /// Putting lots of assertions to check the hardway whether
      /// my assumptions are correct or not

      StoreInst *storeInstruction = nullptr;
      for (auto userIterator = globalValue.user_begin();
           userIterator != globalValue.user_end();
           userIterator++) {
        auto user = *userIterator;
        if (isa<StoreInst>(user)) {
          storeInstruction = dyn_cast<StoreInst>(user);
          break;
        }
      }

      assert(storeInstruction &&
             "The Global should be used within a store instruction");
      auto valueOperand = storeInstruction->getValueOperand();

      auto callSite = CallSite(valueOperand);
      assert((callSite.isCall() || callSite.isInvoke()) &&
             "Store should be using call to MakeAndRegisterTestInfo");

      /// Once we have the CallInstruction we can extract Test Suite Name
      /// and Test Case Name
      /// To extract them we need climb to the top, i.e.:
      ///
      ///   i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.str, i32 0, i32 0)
      ///   i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.str1, i32 0, i32 0)

      auto testSuiteNameConstRef = dyn_cast<ConstantExpr>(callSite->getOperand(0));
      assert(testSuiteNameConstRef);

      auto testCaseNameConstRef = dyn_cast<ConstantExpr>(callSite->getOperand(1));
      assert(testCaseNameConstRef);

      ///   @.str = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
      ///   @.str = private unnamed_addr constant [6 x i8] c"world\00", align 1

      auto testSuiteNameConst = dyn_cast<GlobalValue>(testSuiteNameConstRef->getOperand(0));
      assert(testSuiteNameConst);

      auto testCaseNameConst = dyn_cast<GlobalValue>(testCaseNameConstRef->getOperand(0));
      assert(testCaseNameConst);

      ///   [6 x i8] c"Hello\00"
      ///   [6 x i8] c"world\00"

      auto testSuiteNameConstArray = dyn_cast<ConstantDataArray>(testSuiteNameConst->getOperand(0));
      assert(testSuiteNameConstArray);

      auto testCaseNameConstArray = dyn_cast<ConstantDataArray>(testCaseNameConst->getOperand(0));
      assert(testCaseNameConstArray);

      ///   "Hello"
      ///   "world"

      std::string testSuiteName(testSuiteNameConstArray->getRawDataValues().rtrim('\0'));
      std::string testCaseName(testCaseNameConstArray->getRawDataValues().rtrim('\0'));

      /// Once we've got the Name of a Test Suite and the name of a Test Case
      /// We can construct the name of a Test
      const auto testName = testSuiteName + "." + testCaseName;
      if (filter.shouldSkipTest(testName)) {
        continue;
      }

      /// And the part of Test Body function name

      std::string testBodyFunctionName(testSuiteName + "_" + testCaseName + "_Test8TestBodyEv");

      /// Using the TestBodyFunctionName we could find the function
      /// and finish creating the GoogleTest_Test object

      Function *testBodyFunction = nullptr;
      for (auto &func : currentModule->getModule()->getFunctionList()) {
        StringRef testBodyFunctionNameRef(testBodyFunctionName);
        auto name = func.getName();
        auto foundPosition = name.rfind(testBodyFunctionNameRef);
        if (foundPosition != StringRef::npos) {
          testBodyFunction = &func;
          break;
        }
      }

      assert(testBodyFunction && "Cannot find the TestBody function for the Test");

      tests.emplace_back(make_unique<GoogleTest_Test>(testName,
                                                      testBodyFunction,
                                                      Ctx.getStaticConstructors()));
    }

  }

  return tests;
}

std::vector<std::unique_ptr<Testee>>
GoogleTestFinder::findTestees(Test *Test,
                              Context &Ctx,
                              int maxDistance) {
  GoogleTest_Test *googleTest = dyn_cast<GoogleTest_Test>(Test);

  Function *testFunction = googleTest->GetTestBodyFunction();

  std::vector<std::unique_ptr<Testee>> testees;

  std::queue<std::unique_ptr<Testee>> traversees;
  std::set<Function *> checkedFunctions;

  Module *testBodyModule = testFunction->getParent();

  traversees.push(make_unique<Testee>(testFunction, nullptr, nullptr, 0));

  while (!traversees.empty()) {
    std::unique_ptr<Testee> traversee = std::move(traversees.front());
    Testee *traverseePointer = traversee.get();

    traversees.pop();

    Function *traverseeFunction = traversee->getTesteeFunction();
    const int mutationDistance = traversee->getDistance();

    testees.push_back(std::move(traversee));

    /// The function reached the max allowed distance
    /// Hence we don't go deeper
    if (mutationDistance == maxDistance) {
      continue;
    }

    for (auto &BB : *traverseeFunction) {
      for (auto &I : BB) {
        auto *instruction = &I;

        CallSite callInstruction(instruction);
        if (callInstruction.isCall() == false &&
            callInstruction.isInvoke() == false) {
          continue;
        }

        Function *calledFunction = callInstruction.getCalledFunction();
        if (!calledFunction) {
          continue;
        }

        /// Two modules may have static function with the same name, e.g.:
        ///
        ///   // ModuleA
        ///   define range() {
        ///     // ...
        ///   }
        ///
        ///   define test_A() {
        ///     call range()
        ///   }
        ///
        ///   // ModuleB
        ///   define range() {
        ///     // ...
        ///   }
        ///
        ///   define test_B() {
        ///     call range()
        ///   }
        ///
        /// Depending on the order of processing either `range` from `A` or `B`
        /// will be added to the `context`, hence we may find function `range`
        /// from module `B` while processing body of the `test_A`.
        /// To avoid this problem we first look for function inside of a current
        /// module.
        ///
        /// FIXME: Context should report if a function being added already exist
        /// FIXME: What other problems such behaviour may bring?

        Function *definedFunction =
          testBodyModule->getFunction(calledFunction->getName());

        if (!definedFunction || definedFunction->isDeclaration()) {
          definedFunction =
            Ctx.lookupDefinedFunction(calledFunction->getName());
        } else {
          //Logger::debug() << "GoogleTestFinder> did not find a function: "
          //                << definedFunction->getName() << '\n';
        }

        if (definedFunction) {
          auto functionWasNotProcessed =
            checkedFunctions.find(definedFunction) == checkedFunctions.end();

          checkedFunctions.insert(definedFunction);

          if (functionWasNotProcessed) {
            /// Filtering
            if (filter.shouldSkipTesteeFunction(definedFunction)) {
              continue;
            }

            /// The code below is not actually correct
            /// For each C++ constructor compiler can generate up to three
            /// functions*. Which means that the distance might be incorrect
            /// We need to find a clever way to fix this problem
            ///
            /// * Here is a good overview of what's going on:
            /// http://stackoverflow.com/a/6921467/829116
            ///
            traversees.push(make_unique<Testee>(definedFunction,
                                                instruction,
                                                traverseePointer,
                                                mutationDistance + 1));
          }
        }
      }
    }
  }
  
  return testees;
}

std::vector<MutationPoint *>
GoogleTestFinder::findMutationPoints(const Context &context,
                                     llvm::Function &testee) {

  if (MutationPointsRegistry.count(&testee) != 0) {
    return MutationPointsRegistry.at(&testee);
  }

  Function *definedFunction = context.lookupDefinedFunction(testee.getName());
  assert(definedFunction);

  std::vector<MutationPoint *> points;

  for (auto &mutationOperator : mutationOperators) {
    for (auto point : mutationOperator->getMutationPoints(context, definedFunction, filter)) {
      points.push_back(point);
      MutationPoints.emplace_back(std::unique_ptr<MutationPoint>(point));
    }
  }

  MutationPointsRegistry.insert(std::make_pair(&testee, points));
  return points;
}

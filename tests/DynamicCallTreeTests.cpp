#include "mull/Instrumentation/DynamicCallTree.h"
#include "mull/ReachableFunction.h"
#include "mull/TestFrameworks/Test.h"

#include "gtest/gtest.h"
#include <llvm/IR/Function.h>
#include <stack>

using namespace mull;
using namespace llvm;

static LLVMContext globalContext;

Function *fakeFunction(const char *name) {
  auto parameterType = Type::getInt64Ty(globalContext);
  auto voidType = Type::getVoidTy(globalContext);
  std::vector<Type *> parameterTypes(1, parameterType);

  FunctionType *callbackType =
      FunctionType::get(voidType, parameterTypes, false);

  std::vector<Value *> parameters;

  Function *function =
      Function::Create(callbackType, Function::ExternalLinkage, name, nullptr);
  return function;
}

TEST(DynamicCallTree, empty_tree) {
  std::vector<CallTreeFunction> functions;
  functions.push_back(nullptr);
  functions.push_back(fakeFunction("F1"));
  functions.push_back(fakeFunction("F2"));
  functions.push_back(fakeFunction("F3"));
  functions.push_back(fakeFunction("F4"));
  functions.push_back(fakeFunction("F5"));

  uint32_t mapping[6] = {0};

  std::unique_ptr<CallTree> callTree =
      DynamicCallTree::createCallTree(mapping, functions);
  ASSERT_EQ(callTree->function, nullptr);
  ASSERT_TRUE(callTree->children.empty());
}

TEST(DynamicCallTree, non_empty_tree) {
  Function *phonyFunction = nullptr;
  Function *F1 = fakeFunction("F1");
  Function *F2 = fakeFunction("F2");
  Function *F3 = fakeFunction("F3");
  Function *F4 = fakeFunction("F4");
  Function *F5 = fakeFunction("F5");

  std::vector<CallTreeFunction> functions;
  functions.push_back(phonyFunction);
  functions.push_back(F1);
  functions.push_back(F2);
  functions.push_back(F3);
  functions.push_back(F4);
  functions.push_back(F5);

  ///
  /// Call trace
  ///
  ///   F1 -> F2 -> F3
  ///         F2 -> F4
  ///   F1 -> F4 -> F5
  ///
  /// Call Table at each step
  ///
  /// ACTION       F1  F2  F3  F4  F5  STACK
  /// INIT         0   0   0   0   0   {}
  /// () -> F1     1   0   0   0   0   { F1 }
  /// F1 -> F2     1   1   0   0   0   { F1, F2 }
  /// F2 -> F3     1   1   2   0   0   { F1, F2, F3 }
  /// F3 ends      1   1   2   0   0   { F1, F2 }
  /// F2 -> F4     1   1   2   2   0   { F1, F2, F4 }
  /// F4 ends      1   1   2   2   0   { F1, F2 }
  /// F2 ends      1   1   2   2   0   { F1 }
  /// F1 -> F4     1   1   2   2   0   { F1, F4 }
  /// F4 -> F5     1   1   2   2   4   { F1, F4, F5 }
  /// F5 ends      1   1   2   2   4   { F1, F4 }
  /// F4 ends      1   1   2   2   4   { F1 }
  /// F1 ends      1   1   2   2   4   {}
  /// FINISH       1   1   2   2   4   {}
  ///

  uint32_t mapping[6] = {0};
  mapping[1] = 1;
  mapping[2] = 1;
  mapping[3] = 2;
  mapping[4] = 2;
  mapping[5] = 4;

  std::unique_ptr<CallTree> root =
      DynamicCallTree::createCallTree(mapping, functions);

  /// The tree:
  ///
  ///   0    1     2     3     4
  ///
  ///   0 -> F1 -> F2 -> F3
  ///              F2 -> F4
  ///                    F4 -> F5

  ASSERT_EQ(root->function, nullptr);
  ASSERT_EQ(root->children.size(), 1UL);

  CallTree *f1Node = root->children.front().get();
  ASSERT_EQ(f1Node->function, F1);
  ASSERT_EQ(f1Node->level, 1);
  ASSERT_EQ(f1Node->functionsIndex, 1UL);
  ASSERT_EQ(f1Node->children.size(), 1UL);

  CallTree *f2Node = f1Node->children.front().get();
  ASSERT_EQ(f2Node->function, F2);
  ASSERT_EQ(f2Node->level, 2);
  ASSERT_EQ(f2Node->functionsIndex, 2UL);
  ASSERT_EQ(f2Node->children.size(), 2UL);

  CallTree *f3Node = f2Node->children.front().get();
  ASSERT_EQ(f3Node->function, F3);
  ASSERT_EQ(f3Node->level, 3);
  ASSERT_EQ(f3Node->functionsIndex, 3UL);
  ASSERT_EQ(f3Node->children.size(), 0UL);

  CallTree *f4Node = f2Node->children.back().get();
  ASSERT_EQ(f4Node->function, F4);
  ASSERT_EQ(f4Node->level, 3);
  ASSERT_EQ(f4Node->functionsIndex, 4UL);
  ASSERT_EQ(f4Node->children.size(), 1UL);

  CallTree *f5Node = f4Node->children.front().get();
  ASSERT_EQ(f5Node->function, F5);
  ASSERT_EQ(f5Node->level, 4);
  ASSERT_EQ(f5Node->functionsIndex, 5UL);
  ASSERT_EQ(f5Node->children.size(), 0UL);

  /// mapping is being cleaned up while tree is created
  for (uint32_t index = 0; index < functions.size(); index++) {
    ASSERT_EQ(mapping[0], 0UL);
  }

  ASSERT_EQ(functions[0].treeRoot, nullptr);
  ASSERT_EQ(functions[1].treeRoot, nullptr);
  ASSERT_EQ(functions[2].treeRoot, nullptr);
  ASSERT_EQ(functions[3].treeRoot, nullptr);
  ASSERT_EQ(functions[4].treeRoot, nullptr);
  ASSERT_EQ(functions[5].treeRoot, nullptr);
}

TEST(DynamicCallTree, enter_leave_function) {
  ///
  /// Call trace
  ///
  ///   F1 -> F2 -> F3
  ///         F2 -> F4
  ///   F1 -> F4 -> F5

  uint32_t mapping[6] = {0};
  std::stack<uint32_t> stack;

  // clang-format off
  DynamicCallTree::enterFunction(1, mapping, stack);
    DynamicCallTree::enterFunction(2, mapping, stack);
      DynamicCallTree::enterFunction(3, mapping, stack);
      DynamicCallTree::leaveFunction(3, mapping, stack);
      DynamicCallTree::enterFunction(4, mapping, stack);
      DynamicCallTree::leaveFunction(4, mapping, stack);
    DynamicCallTree::leaveFunction(2, mapping, stack);
    DynamicCallTree::enterFunction(4, mapping, stack);
      DynamicCallTree::enterFunction(5, mapping, stack);
      DynamicCallTree::leaveFunction(5, mapping, stack);
    DynamicCallTree::leaveFunction(4, mapping, stack);
  DynamicCallTree::leaveFunction(1, mapping, stack);
  // clang-format on

  ASSERT_EQ(mapping[0], 0UL);
  ASSERT_EQ(mapping[1], 1UL);
  ASSERT_EQ(mapping[2], 1UL);
  ASSERT_EQ(mapping[3], 2UL);
  ASSERT_EQ(mapping[4], 2UL);
  ASSERT_EQ(mapping[5], 4UL);

  ASSERT_TRUE(stack.empty());
}

TEST(DynamicCallTree, enter_leave_function_recursion) {

#if 0
  void f0();
  void f1(int x);
  void f2(int x);
  void f3();
  void f4();

  void f0() {
    f1(1);
  }

  void f1(int x) {
    if (x == 1) {
      f2(1);
      f4();
    } else if (x == 2) {
      f3();
    } else {
      f4();
    }
  }

  void f2(int x) {
    if (x == 1) {
      f1(2);
    } else {
      f4();
    }
  }

  void f3() {
    f1(4);
  }

  void f4() {}

#endif
  uint32_t mapping[5] = {0};
  std::stack<uint32_t> stack;

  // clang-format off
  DynamicCallTree::enterFunction(1, mapping, stack);
    DynamicCallTree::enterFunction(2, mapping, stack);
      DynamicCallTree::enterFunction(1, mapping, stack);
        DynamicCallTree::enterFunction(3, mapping, stack);
          DynamicCallTree::enterFunction(1, mapping, stack);
            DynamicCallTree::enterFunction(4, mapping, stack);
            DynamicCallTree::leaveFunction(4, mapping, stack);
          DynamicCallTree::leaveFunction(1, mapping, stack);
        DynamicCallTree::leaveFunction(3, mapping, stack);
      DynamicCallTree::leaveFunction(1, mapping, stack);
    DynamicCallTree::leaveFunction(2, mapping, stack);
    DynamicCallTree::enterFunction(4, mapping, stack);
    DynamicCallTree::leaveFunction(4, mapping, stack);
  DynamicCallTree::leaveFunction(1, mapping, stack);
  // clang-format on

  ASSERT_EQ(mapping[0], 0UL);
  ASSERT_EQ(mapping[1], 1UL);
  ASSERT_EQ(mapping[2], 1UL);
  ASSERT_EQ(mapping[3], 1UL);
  ASSERT_EQ(mapping[4], 1UL);

  ASSERT_TRUE(stack.empty());
}

TEST(DynamicCallTree, test_subtrees) {
  Function *phonyFunction = nullptr;
  Function *F1 = fakeFunction("F1");
  Function *F2 = fakeFunction("F2");
  Function *F3 = fakeFunction("F3");
  Function *F4 = fakeFunction("F4");
  Function *F5 = fakeFunction("F5");

  std::vector<CallTreeFunction> functions;
  functions.push_back(phonyFunction);
  functions.push_back(F1);
  functions.push_back(F2);
  functions.push_back(F3);
  functions.push_back(F4);
  functions.push_back(F5);

  ///
  /// Call trace
  ///
  ///   F1 -> F2 -> F3
  ///         F2 -> F4
  ///   F1 -> F4 -> F5
  ///

  uint32_t mapping[6] = {0};
  mapping[1] = 1;
  mapping[2] = 1;
  mapping[3] = 2;
  mapping[4] = 2;
  mapping[5] = 4;

  mull::Test test("", "", "", {}, F2);

  std::unique_ptr<CallTree> callTree =
      DynamicCallTree::createCallTree(mapping, functions);
  std::vector<CallTree *> subtrees =
      DynamicCallTree::extractTestSubtrees(callTree.get(), test);

  EXPECT_EQ(1UL, subtrees.size());

  CallTree *root = *subtrees.begin();
  EXPECT_EQ(root->function, F2);
}

TEST(DynamicCallTree, reachableFunctions) {
  Function *phonyFunction = nullptr;
  Function *F1 = fakeFunction("F1");
  Function *F2 = fakeFunction("F2");
  Function *F3 = fakeFunction("F3");
  Function *F4 = fakeFunction("F4");
  Function *F5 = fakeFunction("F5");

  std::vector<CallTreeFunction> functions;
  functions.push_back(phonyFunction);
  functions.push_back(F1);
  functions.push_back(F2);
  functions.push_back(F3);
  functions.push_back(F4);
  functions.push_back(F5);

  ///
  /// Call trace
  ///
  ///   F1 -> F2 -> F3
  ///         F2 -> F4
  ///   F1 -> F4 -> F5
  ///

  uint32_t mapping[6] = {0};
  mapping[1] = 1;
  mapping[2] = 1;
  mapping[3] = 2;
  mapping[4] = 2;
  mapping[5] = 4;

  mull::Test test("", "", "", {}, F2);

  std::unique_ptr<CallTree> callTree =
      DynamicCallTree::createCallTree(mapping, functions);
  std::vector<CallTree *> subtrees =
      DynamicCallTree::extractTestSubtrees(callTree.get(), test);

  Filter nullFilter;

  {
    std::vector<std::unique_ptr<ReachableFunction>> reachableFunctions =
        DynamicCallTree::createReachableFunctions(subtrees, test, 5,
                                                  nullFilter);

    EXPECT_EQ(4U, reachableFunctions.size());

    ReachableFunction *reachableF2 = reachableFunctions.begin()->get();
    EXPECT_EQ(reachableF2->getFunction(), F2);
    EXPECT_EQ(reachableF2->getDistance(), 0);

    ReachableFunction *reachableF3 = (reachableFunctions.begin() + 1)->get();
    EXPECT_EQ(reachableF3->getFunction(), F3);
    EXPECT_EQ(reachableF3->getDistance(), 1);

    ReachableFunction *reachableF4 = (reachableFunctions.begin() + 2)->get();
    EXPECT_EQ(reachableF4->getFunction(), F4);
    EXPECT_EQ(reachableF4->getDistance(), 1);

    ReachableFunction *reachableF5 = (reachableFunctions.begin() + 3)->get();
    EXPECT_EQ(reachableF5->getFunction(), F5);
    EXPECT_EQ(reachableF5->getDistance(), 2);
  }

  {
    std::vector<std::unique_ptr<ReachableFunction>> reachableFunctions =
        DynamicCallTree::createReachableFunctions(subtrees, test, 1,
                                                  nullFilter);
    EXPECT_EQ(3U, reachableFunctions.size());

    ReachableFunction *reachableF2 = reachableFunctions.begin()->get();
    EXPECT_EQ(reachableF2->getFunction(), F2);
    EXPECT_EQ(reachableF2->getDistance(), 0);

    ReachableFunction *reachableF3 = (reachableFunctions.begin() + 1)->get();
    EXPECT_EQ(reachableF3->getFunction(), F3);
    EXPECT_EQ(reachableF3->getDistance(), 1);

    ReachableFunction *reachableF4 = (reachableFunctions.begin() + 2)->get();
    EXPECT_EQ(reachableF4->getFunction(), F4);
    EXPECT_EQ(reachableF4->getDistance(), 1);
  }

  {
    Filter filter;
    filter.skipByName("F5");
    std::vector<std::unique_ptr<ReachableFunction>> reachableFunctions =
        DynamicCallTree::createReachableFunctions(subtrees, test, 5, filter);
    EXPECT_EQ(3U, reachableFunctions.size());

    ReachableFunction *reachableF2 = reachableFunctions.begin()->get();
    EXPECT_EQ(reachableF2->getFunction(), F2);
    EXPECT_EQ(reachableF2->getDistance(), 0);

    ReachableFunction *reachableF3 = (reachableFunctions.begin() + 1)->get();
    EXPECT_EQ(reachableF3->getFunction(), F3);
    EXPECT_EQ(reachableF3->getDistance(), 1);

    ReachableFunction *reachableF4 = (reachableFunctions.begin() + 2)->get();
    EXPECT_EQ(reachableF4->getFunction(), F4);
    EXPECT_EQ(reachableF4->getDistance(), 1);
  }
}

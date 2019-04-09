#pragma once

#include "mull/TestFrameworks/Test.h"

#include <vector>

namespace llvm {

class Function;

}

namespace mull {

class CustomTest_Test : public Test {
  std::string testName;
  std::string programName;
  std::vector<std::string> arguments;
  llvm::Function *testFunction;

public:
  CustomTest_Test(std::string test, std::string program,
                  std::vector<std::string> args, llvm::Function *body);

  std::string getTestName() override;
  std::string getTestDisplayName() override;

  std::string getUniqueIdentifier() override { return getTestName(); };

  std::vector<llvm::Function *> entryPoints() override {
    return std::vector<llvm::Function *>({testFunction});
  }

  llvm::Function *testBodyFunction() override { return testFunction; }

  std::vector<std::string> &getArguments() { return arguments; }
  std::string &getProgramName() { return programName; }

  static bool classof(const Test *T) { return T->getKind() == TK_CustomTest; }
};

} // namespace mull
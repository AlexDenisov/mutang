#pragma once

#include "MutationPoint.h"
#include "Test.h"
#include "Testee.h"

#include <vector>

namespace llvm {

class Function;

}

namespace mull {

class Test;

enum ExecutionStatus {
  Invalid = 0,
  Failed,
  Passed,
  Timedout,
  Crashed,
  AbnormalExit,
  DryRun
};

struct ExecutionResult {
  ExecutionStatus Status;
  long long RunningTime;
  std::string stdoutOutput;
  std::string stderrOutput;
};

class MutationResult {
  ExecutionResult Result;
  IMutationPoint *MutPoint;
  Testee *testee;

public:
  MutationResult(ExecutionResult R, mull::IMutationPoint *MP, Testee *testee);

  ExecutionResult getExecutionResult()  { return Result; }
  IMutationPoint* getMutationPoint()     { return MutPoint; }
  int getMutationDistance()             { return testee->getDistance(); }
  Testee *getTestee()                    { return testee; }
};

class TestResult {
  /// Result of execution of a test without any mutations applied
  ExecutionResult OriginalTestResult;
  std::unique_ptr<Test> TestPtr;
  std::vector<std::unique_ptr<MutationResult>> MutationResults;
public:
  TestResult(ExecutionResult OriginalResult, std::unique_ptr<Test> T);

  void addMutantResult(std::unique_ptr<MutationResult> Res);

  std::string getTestName();
  std::string getDisplayName();

  std::vector<std::unique_ptr<MutationResult>> &getMutationResults();
  ExecutionResult getOriginalTestResult();
};

}

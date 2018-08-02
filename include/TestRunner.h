#pragma once

#include "ExecutionResult.h"

#include <llvm/Object/Binary.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Target/TargetMachine.h>

namespace mull {

class JITEngine;
class Test;
class Instrumentation;

class TestRunner {
protected:
  llvm::TargetMachine &machine;
public:
  typedef std::vector<llvm::object::ObjectFile *> ObjectFiles;
  typedef std::vector<llvm::object::OwningBinary<llvm::object::ObjectFile>> OwnedObjectFiles;

  explicit TestRunner(llvm::TargetMachine &targetMachine);

  virtual void loadInstrumentedProgram(ObjectFiles &objectFiles, Instrumentation &instrumentation) = 0;
  virtual void loadInstrumentedProgram(ObjectFiles &objectFiles, Instrumentation &instrumentation, JITEngine &jit) {};
  virtual void loadProgram(ObjectFiles &objectFiles) = 0;
  virtual void loadProgram(ObjectFiles &objectFiles, JITEngine &jit) {};
  virtual ExecutionStatus runTest(Test *test) = 0;
  virtual ExecutionStatus runTest(Test *test, JITEngine &jit) { return ExecutionStatus (); };

  virtual ~TestRunner() = default;
};

}

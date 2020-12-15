add_subdirectory(Mutators/CXX)
add_subdirectory(Diagnostics)

set(mull_sources
  AST/ASTFinder.cpp
  AST/ASTMutationFilter.cpp
  AST/ASTMutationStorage.cpp
  AST/ASTScalarMutationMatcher.cpp
  AST/ASTSearchTask.cpp
  AST/ASTVisitor.cpp
  AST/MullClangCompatibility.cpp

  Driver.cpp
  Sandbox/ProcessSandbox.cpp
  BitcodeLoader.cpp
  BitcodeMetadataReader.cpp
  MutationsFinder.cpp

  Instrumentation/DynamicCallTree.cpp
  Instrumentation/Callbacks.cpp
  Instrumentation/Instrumentation.cpp

  Parallelization/Tasks/LoadBitcodeFromBinaryTask.cpp

  Mutators/CXX/LogicalAndToOr.cpp
  Mutators/CXX/LogicalOrToAnd.cpp
  Mutators/MutatorKind.cpp
  Mutators/MutatorsFactory.cpp
  Mutators/NegateConditionMutator.cpp
  Mutators/RemoveVoidFunctionMutator.cpp
  Mutators/ReplaceCallMutator.cpp
  Mutators/ScalarValueMutator.cpp

  Toolchain/Compiler.cpp
  Toolchain/ObjectCache.cpp
  Toolchain/Toolchain.cpp
  Toolchain/JITEngine.cpp
  Toolchain/Mangler.cpp
  Toolchain/Resolvers/InstrumentationResolver.cpp
  Toolchain/Resolvers/MutationResolver.cpp
  Toolchain/Trampolines.cpp
  Toolchain/CXXRuntimeOverrides.cpp

  Bitcode.cpp
  MutationPoint.cpp
  TestFrameworks/TestRunner.cpp
  TestFrameworks/Test.cpp
  ReachableFunction.cpp


  TestFrameworks/BoostTest/BoostTestFinder.cpp

  TestFrameworks/SimpleTest/SimpleTestFinder.cpp

  TestFrameworks/GoogleTest/GoogleTestFinder.cpp
  TestFrameworks/CppUTest/CppUTestFinder.cpp

  TestFrameworks/CustomTestFramework/CustomTestFinder.cpp

  IDEDiagnostics.cpp

  Metrics/MetricsMeasure.cpp

  JunkDetection/CXX/CXXJunkDetector.cpp

  Reporters/ASTSourceInfoProvider.cpp
  Reporters/SourceCodeReader.cpp
  Reporters/SourceManager.cpp
  Reporters/SQLiteReporter.cpp

  SourceLocation.cpp

  Parallelization/Progress.cpp
  Parallelization/TaskExecutor.cpp
  Parallelization/Tasks/BitcodeLoadingTask.cpp
  Parallelization/Tasks/SearchMutationPointsTask.cpp
  Parallelization/Tasks/LoadObjectFilesTask.cpp
  Parallelization/Tasks/DryRunMutantExecutionTask.cpp
  Parallelization/Tasks/InstrumentedCompilationTask.cpp
  Parallelization/Tasks/OriginalTestExecutionTask.cpp
  Parallelization/Tasks/MutantExecutionTask.cpp
  Parallelization/Tasks/MutantPreparationTasks.cpp
  Parallelization/Tasks/MutationFilterTask.cpp
  Parallelization/Tasks/OriginalCompilationTask.cpp
  Parallelization/Tasks/ApplyMutationTask.cpp
  Parallelization/Tasks/FunctionFilterTask.cpp
  Parallelization/Tasks/InstructionSelectionTask.cpp

  Path.cpp

  Config/ConfigurationOptions.cpp
  Config/Configuration.cpp
  TestFrameworks/TestFramework.cpp
  TestFrameworks/TestFrameworkFactory.cpp
  Program/Program.cpp
  ObjectLoader.cpp
  JunkDetection/CXX/Visitors/InstructionRangeVisitor.cpp
  JunkDetection/CXX/Visitors/NegateConditionVisitor.cpp
  JunkDetection/CXX/Visitors/RemoveVoidFunctionVisitor.cpp
  JunkDetection/CXX/Visitors/ReplaceCallVisitor.cpp
  JunkDetection/CXX/Visitors/BinaryVisitor.cpp
  JunkDetection/CXX/Visitors/UnaryVisitor.cpp
  JunkDetection/CXX/ASTStorage.cpp
  JunkDetection/CXX/CompilationDatabase.cpp

  Reporters/IDEReporter.cpp
  Reporters/MutationTestingElementsReporter.cpp
  Reporters/SourceManager.cpp

  TestFrameworks/NativeTestRunner.cpp

  Filters/JunkMutationFilter.cpp
  Filters/NoDebugInfoFilter.cpp
  Filters/FilePathFilter.cpp
  JunkDetection/CXX/Visitors/ScalarValueVisitor.cpp)

set(MULL_INCLUDE_DIR ${MULL_SOURCE_DIR}/include/mull)

set(mull_header_dirs
  ${MULL_INCLUDE_DIR}
  ${MULL_INCLUDE_DIR}/Config
  ${MULL_INCLUDE_DIR}/Instrumentation
  ${MULL_INCLUDE_DIR}/JunkDetection
  ${MULL_INCLUDE_DIR}/JunkDetection/CXX/
  ${MULL_INCLUDE_DIR}/JunkDetection/CXX/Visitors
  ${MULL_INCLUDE_DIR}/Metrics
  ${MULL_INCLUDE_DIR}/Mutators
  ${MULL_INCLUDE_DIR}/MutationFilters
  ${MULL_INCLUDE_DIR}/Parallelization
  ${MULL_INCLUDE_DIR}/Parallelization/Tasks
  ${MULL_INCLUDE_DIR}/Program
  ${MULL_INCLUDE_DIR}/Reporters
  ${MULL_INCLUDE_DIR}/TestFrameworks
  ${MULL_INCLUDE_DIR}/TestFrameworks/CustomTestFramework
  ${MULL_INCLUDE_DIR}/TestFrameworks/GoogleTest
  ${MULL_INCLUDE_DIR}/TestFrameworks/CppUTest
  ${MULL_INCLUDE_DIR}/TestFrameworks/SimpleTest
  ${MULL_INCLUDE_DIR}/Toolchain
  ${MULL_INCLUDE_DIR}/Toolchain/Resolvers
  )

# Globbing is not recommended by CMake so maybe consider listing headers
# explicitly in include/CMakeLists.txt file. However LLVM goes find(GLOB) under
# the hood so probably we are also good for now.
# "We do not recommend using GLOB to collect a list of source files from your
# source tree. If no CMakeLists.txt file changes when a source is added or
# removed then the generated build system cannot know when to ask
# CMake to regenerate. https://cmake.org/cmake/help/v3.0/command/file.html
file(GLOB_RECURSE mull_header_files ${MULL_INCLUDE_DIR}/*.h)

add_library(mull
  ${mull_sources}
  ${mull_header_files}
  $<TARGET_OBJECTS:LLVMCompatibility>
  )

set_source_files_properties(${mull_header_dirs} PROPERTIES HEADER_FILE_ONLY ON)

if ("LLVM" IN_LIST LLVM_AVAILABLE_LIBS)
  set(MULL_LLVM_LIBRARIES
    LLVM
    )
else ()
  set(MULL_LLVM_LIBRARIES
    LLVMAsmParser
    LLVMOrcJIT
    LLVMSupport
    LLVMOption
    LLVM${LLVM_NATIVE_ARCH}CodeGen
    LLVM${LLVM_NATIVE_ARCH}AsmParser
    )
endif ()

find_package(Threads REQUIRED)

target_link_libraries(mull
  ${MULL_LLVM_LIBRARIES}
  sqlite3
  Threads::Threads
  irm
  mull-cxx-mutators
  Diagnostics
  )

if (TARGET clang-cpp)
  # Clang >= 9 provides a monolithic clang-cpp library which provides all clang
  # library functions previously spread across different libraries
  target_link_libraries(mull clang-cpp)
else()
  target_link_libraries(mull clangTooling)
endif()

target_include_directories(mull PUBLIC
  ${MULL_INCLUDE_DIRS}
)
target_include_directories(mull SYSTEM PRIVATE
  ${THIRD_PARTY_INCLUDE_DIRS}
)

set_target_properties(mull PROPERTIES
  COMPILE_FLAGS ${MULL_CXX_FLAGS}
  LINK_FLAGS ${MULL_LINK_FLAGS}
  )

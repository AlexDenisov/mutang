add_subdirectory(fixtures)
generate_fixture_factory(FACTORY_HEADER)

include(GoogleTest)

set(mull_unittests_sources
  AST/ASTVisitor/ASTVisitor_Arithmetic_Tests.cpp
  AST/ASTVisitor/ASTVisitor_ArithmeticAssignment_Tests.cpp
  AST/ASTVisitor/ASTVisitor_Bitwise_Tests.cpp
  AST/ASTVisitor/ASTVisitor_BitwiseAssignment_Tests.cpp
  AST/ASTVisitor/ASTVisitor_Boundary_Tests.cpp
  AST/ASTVisitor/ASTVisitor_Comparison_Tests.cpp
  AST/ASTVisitor/ASTVisitor_Logical_Tests.cpp
  AST/ASTVisitor/ASTVisitor_RemoveVoid_Tests.cpp
  AST/ASTVisitor/ASTVisitor_ReplaceCall_Tests.cpp

  Helpers/InMemoryCompiler.cpp
  Helpers/InMemoryFixture.cpp
  Helpers/MutationTestBed.cpp
  Helpers/MutationArtefact.cpp

  Mutations-E2E/CXX/Arithmetic/Mutation_Arithmetic_Add_Test.cpp
  Mutations-E2E/CXX/Arithmetic/Mutation_Arithmetic_UnaryMinusToNoop_Test.cpp
  Mutations-E2E/CXX/ConstAssignment/Mutation_ConstAssignment_AssignConst.cpp
  Mutations-E2E/CXX/ConstAssignment/Mutation_ConstAssignment_InitConst.cpp
  Mutations-E2E/CXX/RemoveNegationTest.cpp
  Mutations-E2E/Scalar/01_Mutation_Scalar_ReturnValue_Test.cpp
  Mutations-E2E/Scalar/02_Mutation_Scalar_BinaryOperand_Test.cpp

  CompilerTests.cpp
  DriverTests.cpp
  ProcessSandboxTest.cpp
  MutationPointTests.cpp
  ModuleLoaderTest.cpp
  DynamicCallTreeTests.cpp
  MutatorsFactoryTests.cpp
  TesteesTests.cpp

  TestRunnersTests.cpp
  TaskExecutorTests.cpp

  Mutators/NegateConditionMutatorTest.cpp
  Mutators/RemoveVoidFunctionMutatorTest.cpp
  Mutators/ScalarValueMutatorTest.cpp
  Mutators/ConditionalsBoundaryMutatorTests.cpp

  JunkDetection/CXXJunkDetectorTests.cpp

  SimpleTest/SimpleTestFinderTest.cpp

  GoogleTest/GoogleTestFinderTest.cpp

  CppUTest/CppUTestFinderTest.cpp

  CustomTestFramework/CustomTestRunnerTests.cpp
  CustomTestFramework/CustomTestFinderTests.cpp

  SQLiteReporterTest.cpp
  MutationTestingElementsReporterTest.cpp

  TestModuleFactory.cpp
  TestModuleFactory.h

  ${FACTORY_HEADER}

  JunkDetection/CompilationDatabaseTests.cpp
  MutationFilters/MutationFilterTests.cpp
  BoostTest/BoostTestFinderTest.cpp
  )

get_filename_component(factory_include_dir ${FACTORY_HEADER} DIRECTORY)

add_executable(mull-tests ${mull_unittests_sources})
gtest_discover_tests(mull-tests)
target_link_libraries(mull-tests
  mull
  google-test
  json11
)

if (TARGET clang-cpp)
  target_link_libraries(mull-tests clang-cpp)
else()
  target_link_libraries(mull-tests clangCodeGen)
endif()

target_include_directories(mull-tests PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${MULL_INCLUDE_DIRS}
  ${factory_include_dir}
  )
target_include_directories(mull-tests SYSTEM PRIVATE
  ${MULL_SOURCE_DIR}/vendor/googletest/include
  ${THIRD_PARTY_INCLUDE_DIRS}
)
set_target_properties(mull-tests PROPERTIES
  COMPILE_FLAGS ${MULL_CXX_FLAGS}
  LINK_FLAGS ${MULL_LINK_FLAGS}
  )
get_property(dependencies GLOBAL PROPERTY TEST_FIXTURES_DEPENDENCIES)
add_dependencies(mull-tests ${dependencies})

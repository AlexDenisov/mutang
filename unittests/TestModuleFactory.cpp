
#include "TestModuleFactory.h"

#include "Logger.h"

#include <llvm/AsmParser/Parser.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>

#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>

static LLVMContext GlobalCtx;

static inline bool fileExists(const std::string& name) {
  std::ifstream f(name.c_str());
  return f.good();
}

static std::string fixturePath(const char *fixtureName) {
  char cFixtureFullPath[256];

  getcwd(cFixtureFullPath, 255);
  strcat(cFixtureFullPath, "/fixtures/");
  strcat(cFixtureFullPath, fixtureName);

  std::string fixtureFullPath(cFixtureFullPath);

  return fixtureFullPath;
}

static std::string createFixture(const char *fixtureName) {
  std::string fixtureFullPath = fixturePath(fixtureName);

  if (fileExists(fixtureFullPath) == false) {
    mull::Logger::debug() << "Could not find a fixture at path: "
                          << fixtureFullPath << '\n';

    exit(1);
  }

  std::ifstream file(fixtureFullPath);
  std::string str;
  std::string file_contents;
  while (std::getline(file, str)) {
    file_contents += str;
    file_contents.push_back('\n');
  }

  return file_contents;
}

static std::unique_ptr<Module> parseIR(const char *rawBitcode) {
  SMDiagnostic error;
  auto module = parseAssemblyString(rawBitcode, error, GlobalCtx);

  /// FIXME: is there another way to check for errors?
  if (error.getMessage().empty() == false) {
    error.print("test", dbgs());
  }

  assert(module && "Expected module to be parsed correctly");

  assert(!llvm::verifyModule(*module, &dbgs()));

  return module;
}

static std::unique_ptr<MullModule>
createModuleFromBitcode(const char *fixtureName,
                        const char *moduleIdentifier) {
  std::string fixtureFullPath = fixturePath(fixtureName);

  auto bufferOrError = MemoryBuffer::getFile(fixtureFullPath);
  if (!bufferOrError) {
    Logger::error() << "TestModuleFactory> Can't load module " << fixtureFullPath << '\n';
    abort();
  }

  auto llvmModule = parseBitcodeFile(bufferOrError->get()->getMemBufferRef(), GlobalCtx);
  if (!llvmModule) {
    Logger::error() << "TestModuleFactory> Can't load module " << fixtureFullPath << '\n';
    abort();
  }

  llvmModule.get()->setModuleIdentifier(moduleIdentifier);

  auto module = make_unique<MullModule>(std::move(llvmModule.get()), "fake_hash");
  return module;
}

#pragma mark - Mutation Operators

#pragma mark - Math Operators

std::unique_ptr<MullModule> TestModuleFactory::create_SimpleTest_MathSub_module() {
  const char *fixture = "simple_test/mutation_operators/math_sub/math_sub.bc";
  return createModuleFromBitcode(fixture, fixture);
}

std::unique_ptr<MullModule> TestModuleFactory::create_SimpleTest_MathMul_module() {
  const char *fixture = "simple_test/mutation_operators/math_mul/math_mul.bc";
  return createModuleFromBitcode(fixture, fixture);
}

std::unique_ptr<MullModule> TestModuleFactory::create_SimpleTest_MathDiv_module() {
  const char *fixture = "simple_test/mutation_operators/math_div/math_div.bc";
  return createModuleFromBitcode(fixture, fixture);
}

#pragma mark - Negate Condition

std::unique_ptr<MullModule>
TestModuleFactory::createModule(const char *fixtureName,
                                const char *moduleIdentifier) {
  std::string contents = createFixture(fixtureName);
  
  auto module = parseIR(contents.c_str());
  
  module->setModuleIdentifier(moduleIdentifier);

  return make_unique<MullModule>(std::move(module), "");
}

std::unique_ptr<MullModule> TestModuleFactory::create_SimpleTest_NegateCondition_Tester_Module() {
  const char *fixture = "simple_test/mutation_operators/negate_condition/tester.bc";
  return createModuleFromBitcode(fixture, fixture);
}

std::unique_ptr<MullModule> TestModuleFactory::create_SimpleTest_NegateCondition_Testee_Module() {
  const char *fixture = "simple_test/mutation_operators/negate_condition/testee.bc";
  return createModuleFromBitcode(fixture, fixture);
}

#pragma mark - Remove Void Function

std::unique_ptr<MullModule> TestModuleFactory::create_SimpleTest_RemoveVoidFunction_Tester_Module() {
  const char *fixture = "simple_test/mutation_operators/remove_void_function/tester.bc";
  return createModuleFromBitcode(fixture, fixture);
}

std::unique_ptr<MullModule> TestModuleFactory::create_SimpleTest_RemoveVoidFunction_Testee_Module() {
  const char *fixture = "simple_test/mutation_operators/remove_void_function/testee.bc";
  return createModuleFromBitcode(fixture, fixture);
}

#pragma mark - AND <-> OR Replacement

std::unique_ptr<MullModule> TestModuleFactory::create_SimpleTest_ANDORReplacement_Module() {
  const char *fixture = "simple_test/mutation_operators/and_or_replacement/test_and_or_operators.bc";
  return createModuleFromBitcode(fixture, fixture);
}

std::unique_ptr<MullModule> TestModuleFactory::create_SimpleTest_ANDORReplacement_CPPContent_Module() {
  const char *fixture = "simple_test/mutation_operators/and_or_replacement_cpp/test_and_or_operators.bc";
  return createModuleFromBitcode(fixture, fixture);
}

#pragma mark - Scalar Value

std::unique_ptr<MullModule> TestModuleFactory::create_SimpleTest_ScalarValue_module() {
  const char *fixture = "fixture_simple_test_scalar_value_operator.ll";
  return createModule(fixture, fixture);
}

#pragma mark - Replace Call

std::unique_ptr<MullModule> TestModuleFactory::create_SimpleTest_ReplaceCall_module() {
  const char *fixture = "simple_test/mutation_operators/replace_call/replace_call.bc";
  return createModuleFromBitcode(fixture, fixture);
}

#pragma mark -

std::string TestModuleFactory::testerModulePath_IR() {
  return fixturePath("fixture_simple_test_tester_module.ll");
}

std::string TestModuleFactory::testerModulePath_Bitcode() {
  return fixturePath("fixture_simple_test_tester_module.bc");
}

std::unique_ptr<MullModule> TestModuleFactory::createTesterModule() {
  const char *fixture = "fixture_simple_test_tester_module.ll";
  return createModule(fixture, fixture);
}

std::unique_ptr<MullModule> TestModuleFactory::createTesteeModule() {
  return createModule("fixture_testee_module.ll", "count_letters");
}

std::unique_ptr<MullModule> TestModuleFactory::createLibCTesterModule() {
  return createModule("fixture_lib_c_tester_module.ll", "test_main");
}

std::unique_ptr<MullModule> TestModuleFactory::createLibCTesteeModule() {
  return createModule("fixture_lib_c_testee_module.ll", "sum");
}

std::unique_ptr<MullModule> TestModuleFactory::createExternalLibTesterModule() {
  return createModule("fixture_external_lib_tester_module.ll", "test_in_memory_db");
}

std::unique_ptr<MullModule> TestModuleFactory::createExternalLibTesteeModule() {
  return createModule("fixture_external_lib_testee_module.ll", "in_memory_db");
}

#pragma mark - Google Test

std::unique_ptr<MullModule> TestModuleFactory::createGoogleTestTesterModule() {
  return createModule("fixture_google_test_tester_module.ll",
                      "google_test_tester");
}

std::unique_ptr<MullModule> TestModuleFactory::createGoogleTestTesteeModule() {
  return createModule("fixture_google_test_testee_module.ll",
                      "google_test_testee");
}

std::unique_ptr<MullModule> TestModuleFactory::createGoogleTestFinder_invokeInstTestee_Module() {
  return createModule("fixture_google_test_finder_invoke_instruction.ll",
                      "google_test_finder_invoke_instr_testee");
}

#pragma mark -

std::unique_ptr<MullModule> TestModuleFactory::APInt_9a3c2a89c9f30b6c2ab9a1afce2b65d6_213_0_17_negate_mutation_operatorModule() {
  const char *fixture = "APInt_9a3c2a89c9f30b6c2ab9a1afce2b65d6_213_0_17_negate_mutation_operator.ll";
  return createModule(fixture, fixture);
}

std::unique_ptr<MullModule> TestModuleFactory::APFloat_019fc57b8bd190d33389137abbe7145e_214_2_7_negate_mutation_operatorModule() {
  const char *fixture = "APFloat_019fc57b8bd190d33389137abbe7145e_214_2_7_negate_mutation_operator.ll";
  return createModule(fixture, fixture);
}

std::unique_ptr<MullModule> TestModuleFactory::APFloat_019fc57b8bd190d33389137abbe7145e_5_1_3_negate_mutation_operatorModule() {
  const char *fixture = "APFloat_019fc57b8bd190d33389137abbe7145e_5_1_3_negate_mutation_operator.ll";
  return createModule(fixture, fixture);
}

std::unique_ptr<MullModule> TestModuleFactory::createCustomTest_Distance_DistanceModule() {
  const char *fixture = "custom_test/distance/distance.bc";
  return createModuleFromBitcode(fixture, fixture);
}

std::unique_ptr<MullModule> TestModuleFactory::createCustomTest_Distance_MainModule() {
  const char *fixture = "custom_test/distance/main.bc";
  return createModuleFromBitcode(fixture, fixture);
}

std::unique_ptr<MullModule> TestModuleFactory::createCustomTest_Distance_TestModule() {
  const char *fixture = "custom_test/distance/test.bc";
  return createModuleFromBitcode(fixture, fixture);
}

std::unique_ptr<MullModule> TestModuleFactory::create_CustomTest_OpenSSL_bio_enc_test_module() {
  const char *fixture = "openssl_bio_enc_test.o.ll";
  return createModule(fixture, fixture);
}

#pragma mark - Rust

std::unique_ptr<MullModule> TestModuleFactory::rustModule() {
  const char *fixture = "fixture_rust.ll";
  return createModule(fixture, "rust");
}

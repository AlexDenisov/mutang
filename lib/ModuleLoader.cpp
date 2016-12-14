#include "ModuleLoader.h"

#include "llvm/AsmParser/Parser.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/MD5.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"

using namespace llvm;
using namespace Mutang;

static std::string MD5HashFromBuffer(StringRef buffer) {
  MD5 Hasher;
  Hasher.update(buffer);
  MD5::MD5Result Hash;
  Hasher.final(Hash);
  SmallString<32> Result;
  MD5::stringifyResult(Hash, Result);
  return Result.str();
}

std::unique_ptr<MutangModule> ModuleLoader::loadModuleAtPath(const std::string &path) {
  auto BufferOrError = MemoryBuffer::getFile(path);
  if (!BufferOrError) {
    printf("can't load module '%s'\n", path.c_str());
    return nullptr;
  }

  std::string hash = MD5HashFromBuffer(BufferOrError->get()->getBuffer());

  auto llvmModule = parseBitcodeFile(BufferOrError->get()->getMemBufferRef(), Ctx);
  if (!llvmModule) {
    printf("can't load module '%s'\n", path.c_str());
    return nullptr;
  }

  auto module = make_unique<MutangModule>(std::move(llvmModule.get()), hash);
  return module;
}

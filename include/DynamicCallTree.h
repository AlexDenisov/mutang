#pragma once

#include <list>
#include <stack>
#include <vector>

#include <llvm/IR/Function.h>

namespace mull {

  struct CallTree {
    llvm::Function *function;
    int level;
    uint64_t functionsIndex;
    std::list<std::unique_ptr<CallTree>> children;
    CallTree(llvm::Function *f) : function(f), level(0), functionsIndex(0) {}
  };

  struct CallTreeFunction {
    llvm::Function *function;
    CallTree *treeRoot;

    CallTreeFunction(llvm::Function *f) : function(f), treeRoot(nullptr) {}
  };

  class DynamicCallTree {
  public:
    DynamicCallTree(std::vector<CallTreeFunction> &f);

    void prepare(uint64_t *m);
    std::unique_ptr<CallTree> createCallTree();
    void cleanupCallTree(std::unique_ptr<CallTree> root);

    static void enterFunction(const uint64_t functionIndex,
                              uint64_t *mapping,
                              std::stack<uint64_t> &stack);
    static void leaveFunction(const uint64_t functionIndex,
                              uint64_t *mapping,
                              std::stack<uint64_t> &stack);
  private:
    uint64_t *mapping;
    std::vector<CallTreeFunction> &functions;
  };

}

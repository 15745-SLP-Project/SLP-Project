#include "llvm/IR/Function.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>

using namespace llvm;

/*
 * A Pack is an n-tuple, <s1, ..., sn>, where s1, ..., sn are independent
 * isomorphic statements in a basic block
 */
class Pack {};

/*
 * A PackSet is a set of Packs
 */
class PackSet {};

/*
 * A Pair is a Pack of size two, where the first statement is considered as the
 * left element, and the second statment is considered the right element
 */
class Pair : public Pack {};

class SLP : public FunctionPass {
public:
  static char ID;
  SLP() : FunctionPass(ID) {}
  ~SLP() {}

  // We modify the program within each basic block, but preserve the CFG
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
  }

  bool doInitialization(Module &M) override {
    return false;
  }

  // Apply transforms and print summary
  bool runOnFunction(Function &F) override {
    bool change = false;
    for (auto &BB : F) {
      change |= SLP_extract(BB);
    }
    return change;
  }

  bool SLP_extract(BasicBlock &BB) {
    return false;
  }

  bool doFinalization(Module &M) {
    return false;
  }
};

char SLP::ID = 0;
static RegisterPass<SLP> X("slp", "Superword level parallelism", false, false);

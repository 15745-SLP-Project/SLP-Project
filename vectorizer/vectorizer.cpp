#include "llvm/IR/Function.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/NoFolder.h"

#include <vector>
#include <iostream>

using namespace llvm;

namespace {

class Vectorizer : public FunctionPass {

public:
  static char ID;
  Vectorizer() : FunctionPass(ID) {}
  ~Vectorizer() {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
  }

  bool doInitialization(Module &M) override {
    return false;
  }

  bool runOnFunction(Function &F) override {
    if (F.getName() == "test1") {

      for (auto &bb : F) {
        IRBuilder<NoFolder> builder(&bb);
        builder.SetInsertPoint(&bb.back());
        auto* int_type = builder.getInt32Ty();

        // create arr
        auto* arr_type = ArrayType::get(builder.getInt32Ty(), 4);
        auto* arr = builder.CreateAlloca(arr_type, builder.getInt32(1), "arr");

        // get ptr to arr
        auto* zero = builder.getInt32(0);
        auto* idx = builder.getInt32(0);
        auto* arr_ptr = builder.CreateGEP(arr_type, arr, {zero, idx}, "arr_ptr");

        // bitcast arr ptr to vec ptr
        auto* vec_type = VectorType::get(int_type, 4);
        auto* vec_ptr_type = PointerType::get(vec_type, 0);
        auto* vec_ptr = builder.CreateBitCast(arr_ptr, vec_ptr_type, "vec_ptr");

        // load vec
        auto* vec = builder.CreateLoad(vec_type, vec_ptr, "vec");

        // do vec add
        auto* vec_sum = builder.CreateAdd(vec, vec, "vec_sum");

        // store vec
        builder.CreateStore(vec_sum, vec_ptr);
      }
    }

    else if (F.getName() == "test2") {

      for (auto &bb : F) {
        IRBuilder<NoFolder> builder(&bb);
        builder.SetInsertPoint(&bb.back());
        auto* int_type = builder.getInt32Ty();

        // create 4 variables
        std::vector<Value*> vars;
        for (int i=0; i < 4; i++) {
          std::string name = "a" + std::to_string(i);
          auto* var = builder.CreateAdd(builder.getInt32(i), builder.getInt32(i), name);
          vars.push_back(var);
        }

        // create vec
        auto* vec_type = VectorType::get(int_type, 4);
        auto* zero = builder.getInt32(0);
        auto* size = builder.getInt32(1);
        auto* vec = UndefValue::get(vec_type);
        vec->setName("vec");

        Value* curr_vec = vec;

        // pack 4 variables into vec
        for (int i=0; i < 4; i++) {
          std::string name = "insert" + std::to_string(i);
          curr_vec = builder.CreateInsertElement(curr_vec, vars[i], i, name);
        }
        
        // do vec add
        auto* vec_sum = builder.CreateAdd(curr_vec, curr_vec, "vec_sum");

        // do unpacking
        std::vector<Value*> new_vars;
        for (int i=0; i < 4; i++) {
          std::string name = "extract" + std::to_string(i);
          auto* new_var = builder.CreateExtractElement(vec_sum, i, name);
          new_vars.push_back(new_var);
        }

        Value* tmp = new_vars[0];
        for (int i=1; i < 4; i++) {
          tmp = builder.CreateAdd(tmp, new_vars[i]);
        }

        // create new ret and erase old one
        builder.CreateRet(tmp);
        bb.back().eraseFromParent();
      }
    }

    return true;
  }

  bool doFinalization(Module &M) { 
    return false; 
  }
};

} // namespace

// LLVM uses the address of this static member to identify the pass, so the
// initialization value is unimportant.
char Vectorizer::ID = 0;
static RegisterPass<Vectorizer> X("vectorizer", "Vectorization", false, false);

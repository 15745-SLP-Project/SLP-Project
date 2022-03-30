#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>
#include <set>

using namespace llvm;

/*
 * A Pack is an n-tuple, <s1, ..., sn>, where s1, ..., sn are independent
 * isomorphic statements in a basic block
 */
class Pack {
public:
  Pack() {}

  Pack(Instruction *s1, Instruction *s2) {
    pack[0] = s1;
    pack[1] = s2;
  }

  size_t getSize() const {
    return pack.size();
  }

  Instruction *getNthElement(size_t n) const {
    return pack[n];
  }

  /*
   * A Pair is a Pack of size two, where the first statement is considered as
   * the left element, and the second statment is considered the right element
   */
  bool isPair() const {
    return getSize() == 2;
  }

  Instruction *getLeftElement() const {
    assert(isPair());
    return getNthElement(0);
  }

  Instruction *getRightElement() const {
    assert(isPair());
    return getNthElement(1);
  }

  bool operator==(const Pack &r) const {
    if (getSize() != r.getSize()) {
      return false;
    }
    for (size_t i = 0; i < getSize(); i++) {
      if (getNthElement(i) != r.getNthElement(i)) {
        return false;
      }
    }
    return true;
  }

  bool operator!=(const Pack &r) const {
    return !(*this == r);
  }

  bool operator<(const Pack &r) const {
    if (getSize() < r.getSize()) {
      return true;
    }
    if (getSize() > r.getSize()) {
      return false;
    }
    for (size_t i = 0; i < getSize(); i++) {
      if (getNthElement(i) < r.getNthElement(i)) {
        return true;
      }
    }
    return false;
  }

private:
  std::vector<Instruction *> pack;
};

/*
 * A PackSet is a set of Packs
 */
class PackSet {
public:
  PackSet() {}

  void addPair(Instruction *s1, Instruction *s2) {
    packSet.emplace(Pack(s1, s2));
  }

  typedef std::set<Pack>::iterator PackSetIterator;

  PackSetIterator begin() {
    return packSet.begin();
  }

  PackSetIterator end() {
    return packSet.end();
  }

private:
  std::set<Pack> packSet;
};

class AlignInfo {
public:
  AlignInfo(Value *base, Value *inductionVar, unsigned int index)
      : base(base), inductionVar(inductionVar), index(index) {}

  Value *base;
  Value *inductionVar;
  unsigned int index;
};

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
      change |= slpExtract(BB);
    }
    return change;
  }

  bool slpExtract(BasicBlock &BB) {
    PackSet P;
    findAdjRefs(BB, P);
    extendPacklist(BB, P);
    combinePacks(P);
    return false;
  }

  void findAdjRefs(BasicBlock &BB, PackSet &P) {
    // Find the base addresses of memory reference
    setAlignRef(BB);

    // Find all adjacent memory references and add to PackSet
    for (auto &s1 : BB) {
      for (auto &s2 : BB) {
        if ((&s2 != &s1) && s1.mayReadOrWriteMemory() &&
            s2.mayReadOrWriteMemory()) {
          if (adjacent(&s1, &s2)) {
            auto align = getAlignment(&s1);
            if (stmtsCanPack(BB, P, &s1, &s2, align))
              P.addPair(&s1, &s2);
          }
        }
      }
    }
  }

  void setAlignRef(BasicBlock &BB) {
    for (auto &s : BB) {
      // Only look at memory access instructions
      if (s.mayReadOrWriteMemory()) {
        // getelementptr instruction
        GetElementPtrInst *gep = NULL;

        // Load instruction
        if (auto loadInst = dyn_cast<LoadInst>(&s)) {
          auto loadPtr = loadInst->getPointerOperand();
          gep = dyn_cast<GetElementPtrInst>(loadPtr);
        }
        // Store instruction
        if (auto storeInst = dyn_cast<StoreInst>(&s)) {
          auto storePtr = storeInst->getPointerOperand();
          gep = dyn_cast<GetElementPtrInst>(storePtr);
        }

        // Only look at load/store with simple gep
        if (!gep || gep->getNumIndices() != 2) {
          continue;
        }

        // Base address
        Value *b = gep->getPointerOperand();
        baseAddress.insert(b);

        // Induction variable
        Value *iv = nullptr;
        Value *v = gep->getOperand(2);
        if (auto addInst = dyn_cast<BinaryOperator>(v)) {
          if (addInst->getOpcode() == Instruction::Add) {
            Value *operand0 = addInst->getOperand(0);
            Value *opearnd1 = addInst->getOperand(1);
            if (isa<ConstantInt>(opearnd1)) {
              // Get the memory reference index w.r.t. base address
              unsigned int index = cast<ConstantInt>(opearnd1)->getZExtValue();
              setAlignment(&s, b, operand0, index);
            }
          }
        } else {
          setAlignment(&s, b, iv, 0);
        }
      }
    }
  }

  bool adjacent(Instruction *s1, Instruction *s2) {
    return checkAlignment(getAlignment(s1), getAlignment(s2), 1);
  }

  void setAlignment(Instruction *s, Value *b, Value *iv, unsigned int index) {
    alignInfo.emplace(std::map<Instruction *, AlignInfo>::value_type(
        s, AlignInfo(b, iv, index)));
  }

  AlignInfo *getAlignment(Instruction *s) {
    if (alignInfo.find(s) == alignInfo.end()) {
      return nullptr;
    }
    return &(alignInfo.find(s)->second);
  }

  bool checkAlignment(AlignInfo *s1, AlignInfo *s2, unsigned int offset) {
    if (s1 == nullptr || s2 == nullptr) {
      return false;
    }
    if (s1->base != s2->base) {
      return false;
    }
    if (s1->inductionVar != s2->inductionVar) {
      return false;
    }
    return (s1->index + offset == s2->index);
  }

  bool stmtsCanPack(BasicBlock &BB, PackSet &P, Instruction *s1,
                    Instruction *s2, AlignInfo *align) {
    if (isIsomorphic(s1, s2) && isIndependent(s1, s2)) {
      if (!packedInLeft(P, s1) && !packedInRight(P, s2)) {
        auto align_s1 = getAlignment(s1);
        auto align_s2 = getAlignment(s2);
        if (align_s1 == nullptr || checkAlignment(align, align_s1, 0)) {
          if (align_s2 == nullptr || checkAlignment(align, align_s2, 1)) {
            return true;
          }
        }
      }
    }
    return false;
  }

  bool isIsomorphic(Instruction *s1, Instruction *s2) {
    // If two instructions have the same operation and type, they are isomorphic
    return (s1->getOpcode() == s2->getOpcode()) &&
           (s1->getType() == s2->getType());
  }

  bool isIndependent(Instruction *s1, Instruction *s2) {
    // todo
    // If two instructions have no dependency, they are independent
    return false;
  }

  bool packedInLeft(PackSet &P, Instruction *s) {
    for (auto &p : P) {
      if (p.getLeftElement() == s) {
        return true;
      }
    }
    return false;
  }

  bool packedInRight(PackSet &P, Instruction *s) {
    for (auto &p : P) {
      if (p.getRightElement() == s) {
        return true;
      }
    }
    return false;
  }

  void extendPacklist(BasicBlock &BB, PackSet &P) {
    bool change;
    do {
      change = false;
      for (auto &p : P) {
        change |= followUseDefs(BB, P, p);
        change |= followDefUses(BB, P, p);
      }
    } while (change);
  }

  bool followUseDefs(BasicBlock &BB, PackSet &P, Pack p) {
    Instruction *s1 = p.getLeftElement();
    Instruction *s2 = p.getRightElement();
    auto align = getAlignment(s1);
    auto m = s1->getNumOperands();
    assert(m == s2->getNumOperands());
    for (unsigned int j = 0; j < m; j++) {
      // todo
    }
    return false;
  }

  bool followDefUses(BasicBlock &BB, PackSet &P, Pack p) {
    // todo
    return false;
  }

  void combinePacks(PackSet &P) {
    // todo
  }

  bool doFinalization(Module &M) override {
    return false;
  }

private:
  std::set<Value *> baseAddress;
  std::map<Instruction *, AlignInfo> alignInfo;
};

char SLP::ID = 0;
static RegisterPass<SLP> X("slp", "Superword level parallelism", false, false);

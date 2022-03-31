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

  Instruction::BinaryOps getBinOpCode(); // TODO

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

  // Pack iterator
  typedef std::vector<Instruction *>::iterator PackIterator;

  PackIterator begin() {
    return pack.begin();
  }

  PackIterator end() {
    return pack.end();
  }

  std::vector<Instruction *> getPack() {
    return pack;
  }

private:
  std::vector<Instruction *> pack;
  Instruction::BinaryOps binOpCode;
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

  // PackSet iterator
  typedef std::set<Pack>::iterator PackSetIterator;

  PackSetIterator begin() {
    return packSet.begin();
  }

  PackSetIterator end() {
    return packSet.end();
  }

  // PackSet const iterator
  typedef std::set<Pack>::const_iterator PackSetConstIterator;

  PackSetConstIterator cbegin() {
    return packSet.cbegin();
  }

  PackSetIterator cend() {
    return packSet.cend();
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
    bool changed = false;
    for (auto &BB : F) {
      changed |= slpExtract(BB);
    }
    return changed;
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

  void setAlignment(Instruction *dst, Instruction *src) {
    AlignInfo *align = getAlignment(src);
    alignInfo.emplace(
        std::map<Instruction *, AlignInfo>::value_type(dst, *align));
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
    // also have to be binary ops
    return (s1->getOpcode() == s2->getOpcode()) &&
           (s1->getType() == s2->getType()) &&
           (isa<BinaryOperator>(s1)) &&
           (isa<BinaryOperator>(s2));
  }

  bool isIndependent(Instruction *s1, Instruction *s2) {
    // If two instructions have no dependency, they are independent
    // Check the use chain of s1 and s2
    for (auto *s1User : s1->users()) {
      if ((Value *)s1User == (Value *)s2) {
        return false;
      }
    }
    for (auto *s2User : s2->users()) {
      if ((Value *)s2User == (Value *)s1) {
        return false;
      }
    }
    return true;
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
    bool changed;
    do {
      changed = false;
      for (auto &p : P) {
        changed |= followUseDefs(BB, P, p);
        changed |= followDefUses(BB, P, p);
      }
    } while (changed);
  }

  int estSavings(Instruction *t1, Instruction *t2, PackSet &P) {
    return 1;
  }

  bool followUseDefs(BasicBlock &BB, PackSet &P, Pack p) {
    bool changed = false;

    Instruction *s1 = p.getLeftElement();
    Instruction *s2 = p.getRightElement();
    auto align_s1 = getAlignment(s1);
    auto align_s2 = getAlignment(s2);
    auto m = s1->getNumOperands();
    assert(m == s2->getNumOperands());
    for (unsigned int j = 0; j < m; j++) {
      Instruction *t1 = cast<Instruction>(s1->getOperand(j));
      Instruction *t2 = cast<Instruction>(s2->getOperand(j));
      if (t1->getParent() == &BB && t2->getParent() == &BB) {
        if (stmtsCanPack(BB, P, t1, t2, align_s1)) {
          if (estSavings(t1, t2, P) >= 0) {
            P.addPair(t1, t2);
            setAlignment(t1, s1);
            setAlignment(t2, s2);
            changed = true;
          }
        }
      }
    }

    return changed;
  }

  bool followDefUses(BasicBlock &BB, PackSet &P, Pack p) {
    bool changed = false;

    int savings = -1;

    Instruction *s1 = p.getLeftElement();
    Instruction *s2 = p.getRightElement();
    auto align_s1 = getAlignment(s1);
    auto align_s2 = getAlignment(s2);
    Instruction *t1 = nullptr;
    Instruction *t2 = nullptr;
    Instruction *u1 = nullptr;
    Instruction *u2 = nullptr;

    for (auto *s1User : s1->users()) {
      t1 = cast<Instruction>(s1User);
      if (t1->getParent() != &BB) {
        continue;
      }
      for (auto *s2User : s2->users()) {
        t2 = cast<Instruction>(s2User);
        if (t2->getParent() != &BB) {
          continue;
        }
        if (stmtsCanPack(BB, P, t1, t2, align_s1)) {
          if (estSavings(t1, t2, P) > savings) {
            savings = estSavings(t1, t2, P);
            u1 = t1;
            u2 = t2;
          }
        }
      }
    }

    if (savings >= 0) {
      P.addPair(u1, u2);
      setAlignment(u1, s1);
      setAlignment(u2, s2);
      changed = true;
    }

    return changed;
  }

  void combinePacks(PackSet &P) {
    // todo
  }

  /*
    A PackSet consists of pack(s). Each vectorizable pack is of the form:
      x0 = y0 OP z0
      x1 = y1 OP z1
      x2 = y2 OP z2
      x3 = y3 OP z3

    x0-3 are either temps or array elems
    y0-3 are either temps, array elems, constants
    z0-3 are either temps, array elems, or constants

    The general procedure is to:
      1. pack y0-3 into vector y
      2. pack z0-3 into vector z
      3. perform x = y OP z
      4. unpack x into x0-3

    We are imposing the following restrictions to limit the number of cases
    we can handle:
      1. 
        if src_i is an array elem, then all src_j have to be array elems of the
        same array. this waywe will only have to do a bitcast to convert src0-3
        into a vector
      2. 
        if dest_i is an array elem, then all dest_j have to be array elems of
        the same array. this way we will only have to do a bitcast to convert
        dest0-3 into a vector


    For now, we are just doing the simple, naive thing where we pack everything,
    do the op, then unpack
  */
  void codeGen(PackSet &P) {

    // pack operands into a vector
    auto packOperands = [] (std::vector<Value*> &operands, IRBuilder<> &builder) {
      auto *vecType = VectorType::get(operands[0]->getType(), operands.size());
      auto *vec = UndefValue::get(vecType);
      for (int i=0; i < operands.size(); i++) {
        vec = builder.CreateInsertElement(vec, operands[i], i);
      }
      return vec;
    };

    // iterate over all packs
    for (auto packSetIter = P.begin(); packSetIter != P.end(); packSetIter++) {
      Pack &pack = *packSetIter;

      // IRBuilder for this pack
      // have it start adding new instructions before first instruction of pack
      IRBuilder<> builder(pack.getNthElement(0));

      // separate operand0s and operand1s
      std::vector<Value*> operand0s, operand1s;
      for (int i=0; i < pack.getSize(); i++) {
        auto *instr = pack.getNthElement(i);
        operand0s.push_back(instr->getOperand(0));
        operand0s.push_back(instr->getOperand(1));
      }

      // pack operands
      auto *operand0Vec = packOperands(operand0s, builder);
      auto *operand1Vec = packOperands(operand1s, builder);

      // vector bin op
      auto *destVec = builder.CreateBinOp(pack.getBinOpCode(), operand0Vec, operand1Vec);

      // unpack res
      for (int i=0; i < pack.getSize(); i++) {
        auto *instr = pack.getNthElement(i);
        auto *newVal = builder.CreateExtractElement(destVec, i);
        instr->replaceAllUsesWith(newVal);
      }

      // delete all instrs in pack
      for (int i=0; i < pack.getSize(); i++) {
        pack.getNthElement(i)->eraseFromParent();
      }
    }
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

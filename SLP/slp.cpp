#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>
#include <set>

#include "utils.hpp"

using namespace llvm;

const static bool verbose = true;

/*
 * A Pack is an n-tuple, <s1, ..., sn>, where s1, ..., sn are independent
 * isomorphic statements in a basic block
 */
class Pack {
public:
  Pack() {}

  Pack(Instruction *s1, Instruction *s2) {
    pack.push_back(s1);
    pack.push_back(s2);
  }

  // Only used to combine two packs
  Pack(std::vector<Instruction *> v1, std::vector<Instruction *> v2) {
    pack.reserve(v1.size() + v2.size() - 1);
    pack.insert(pack.end(), v1.begin(), v1.end());
    pack.insert(pack.end(), ++(v2.begin()), v2.end());
  }

  void print(unsigned int index) const {
    outs() << "\tPack " << index << " (" << this << ")\n";
    for (unsigned int i = 0; i < pack.size(); i++) {
      outs() << "\t\t" << i << ": " << *(pack[i]) << "\n";
    }
  }

  size_t getSize() const {
    return pack.size();
  }

  Instruction *getNthElement(size_t n) const {
    return pack[n];
  }

  Instruction *getFirstElement() const {
    return pack[0];
  }

  Instruction *getLastElement() const {
    return pack[getSize() - 1];
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

  friend class PackSet;

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

  Pack &getNthPack(unsigned int n) {
    return packSet[n];
  }

  void printPackSet() {
    outs() << "PackSet\n";
    unsigned int index = 0;
    for (auto &p : packSet) {
      p.print(index);
      index++;
    }
    outs() << "\n";
  }

  void printScheduledPackList() {
    outs() << "Scheduled Pack List\n";
    unsigned int index = 0;
    for (unsigned int i = 0; i < scheduledPackList.size(); i++) {
      scheduledPackList[i]->print(i);
    }
    outs() << "\n";
  }

  void addPair(Instruction *s1, Instruction *s2) {
    add(Pack(s1, s2));
    if (verbose)
      outs() << "[addPair] (" << *s1 << ") and (" << *s2 << ")\n";
  }

  void addCombination(Pack &p1, Pack &p2) {
    add(Pack(p1.pack, p2.pack));
  }

  void remove(Pack &p) {
    erase(p);
  }

  bool pairExists(Instruction *s1, Instruction *s2) {
    for (auto &t : packSet) {
      if (t.isPair()) {
        if (t.getLeftElement() == s1 && t.getRightElement() == s2) {
          return true;
        }
      }
    }
    return false;
  }

  // PackSet iterator
  typedef std::vector<Pack>::iterator PackSetIterator;

  PackSetIterator begin() {
    return packSet.begin();
  }

  PackSetIterator end() {
    return packSet.end();
  }

  // Scheduled PackList iterator
  typedef std::vector<Pack *>::iterator ScheduledPackListIterator;

  ScheduledPackListIterator lbegin() {
    return scheduledPackList.begin();
  }

  ScheduledPackListIterator lend() {
    return scheduledPackList.end();
  }

  bool schedule() {
    buildDependency();

    std::set<Pack *> scheduled;
    unsigned int scheduledOldSize;
    do {
      scheduledOldSize = scheduled.size();
      for (auto &pRef : packSet) {
        Pack *p = &pRef;

        // Don't look at already scheduled packs
        if (scheduled.find(p) != scheduled.end()) {
          continue;
        }

        // No dependency
        if (dependency.find(p) == dependency.end()) {
          scheduled.insert(p);
          scheduledPackList.push_back(p);
          break;
        }

        // All dependency already scheduled
        std::set<Pack *> pDepSet = dependency[p];
        bool checkAllDep = true;
        for (auto pDep : pDepSet) {
          if (scheduled.find(pDep) == scheduled.end()) {
            checkAllDep = false;
            break;
          }
        }
        if (checkAllDep) {
          scheduled.insert(p);
          scheduledPackList.push_back(p);
          break;
        }
      }
    } while (scheduledOldSize != scheduled.size());
    return scheduledPackList.size() == packSet.size();
  }

  size_t size() {
    return packSet.size();
  }

private:
  std::vector<Pack> packSet;
  std::map<Pack *, std::set<Pack *>> dependency;
  std::vector<Pack *> scheduledPackList;

  void add(Pack &&p) {
    for (auto &t : packSet) {
      if (t == p) {
        return;
      }
    }
    packSet.emplace_back(p);
  }

  void erase(Pack &p) {
    for (auto iter = packSet.begin(); iter != packSet.end(); iter++) {
      if (p == *iter) {
        packSet.erase(iter);
      }
    }
  }

  void buildDependency() {
    for (auto pIter = begin(); pIter != end(); pIter++) {
      for (auto pDepIter = begin(); pDepIter != end(); pDepIter++) {
        Pack *p = &(*pIter);
        Pack *pDep = &(*pDepIter);
        if (p == pDep) {
          continue;
        }

        bool dependent = false;
        // Check whether p depends on pDep
        for (auto s : *p) {
          for (auto sDep : *pDep) {
            // Check whether s depends on sDep
            if (isDependentOn(s, sDep)) {
              dependent = true;
              if (dependency.find(p) == dependency.end()) {
                dependency[p] = std::set<Pack *>();
              }
              // If p depends on pDep, add to dependency graph
              dependency[p].insert(pDep);
            }
          }
          if (dependent) {
            break;
          }
        }
      }
    }
    unsigned int i = 0, j;
    for (auto it = dependency.begin(); it != dependency.end(); it++) {
      Pack *p = it->first;
      std::set<Pack *> pDeps = it->second;
      outs() << "The pack " << p << " depends on packs:";
      for (auto pDep : pDeps) {
        outs() << " " << pDep;
      }
      outs() << "\n";
    }
  }
};

/*
 * AlignInfo class stores the basic alignment information described in the
 * paper, including a base address, an induction variable, and the index (or
 * offset against the base address).
 *
 * In the foo example,
 *  A[i + 0] = A[i + 0] * A[i + 0];
 *  A[i + 1] = A[i + 1] * A[i + 1];
 *  A[i + 2] = A[i + 2] * A[i + 2];
 *  A[i + 3] = A[i + 3] * A[i + 3]; <--
 * Look at the last instruction, base = A, inductionVar = i, index = 3
 *
 * Alignment information will first be assigned to load and store instruction,
 * and in next steps the align info of memory access instructions will be copied
 * to instructions that have dependency relationship with them.
 */
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

    if (F.getName() == "foo") {
      for (auto &BB : F) {
        baseAddress.clear();
        alignInfo.clear();
        changed |= slpExtract(BB);
      }
    }
    return changed;
  }

  bool slpExtract(BasicBlock &BB) {
    PackSet P;
    findAdjRefs(BB, P);
    extendPacklist(BB, P);
    combinePacks(P);
    P.printPackSet();
    P.schedule();
    P.printScheduledPackList();
    if (P.size() > 0) {
      // codeGen(P);
      return true;
    }
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
            if (stmtsCanPack(BB, P, &s1, &s2, align)) {
              P.addPair(&s1, &s2);
            }
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

        // Find the induction variable
        Value *v = gep->getOperand(2);
        if (auto addInst = dyn_cast<BinaryOperator>(v)) {
          if (addInst->getOpcode() == Instruction::Add) {
            Value *operand0 = addInst->getOperand(0);
            Value *opearnd1 = addInst->getOperand(1);
            if (isa<ConstantInt>(opearnd1)) {
              // Get the memory reference index w.r.t. base address
              unsigned int index = cast<ConstantInt>(opearnd1)->getZExtValue();
              setAlignment(&s, b, operand0, index);
              if (verbose)
                outs() << "[setAlignRef] set alignment for (" << s
                       << "), base = " << b->getName()
                       << ", iv = " << operand0->getName()
                       << ", index = " << index << "\n";
            }
          }
        } else {
          setAlignment(&s, b, v, 0);
          if (verbose)
            outs() << "[setAlignRef] set alignment for (" << s
                   << "), base = " << b->getName() << ", iv = " << v->getName()
                   << ", index = 0\n";
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
    // Apply BFS to search the def-use chain and extend pack list
    unsigned int head = 0, tail;
    do {
      tail = P.size();
      while (head < tail) {
        followUseDefs(BB, P, P.getNthPack(head));
        followDefUses(BB, P, P.getNthPack(head));
        head++;
      }
    } while (head < P.size());
  }

  int estSavings(Instruction *t1, Instruction *t2, PackSet &P) {
    if (P.pairExists(t1, t2))
      return -1;
    else
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
    bool changed;
    do {
      changed = false;
      for (auto pi1 = P.begin(); pi1 != P.end(); pi1++) {
        for (auto pi2 = P.begin(); pi2 != P.end(); pi2++) {
          Pack p1 = *pi1;
          Pack p2 = *pi2;
          if (&p1 == &p2) {
            continue;
          }
          if (p1.getLastElement() == p2.getFirstElement()) {
            P.addCombination(p1, p2);
            P.remove(p1);
            P.remove(p2);
            changed = true;
            break;
          }
        }
        if (changed) {
          break;
        }
      }
    } while (changed);
  }

  /*
   * A PackSet consists of pack(s). Each vectorizable pack is of the form:
   *   x0 = y0 OP z0
   *   x1 = y1 OP z1
   *   x2 = y2 OP z2
   *   x3 = y3 OP z3
   *
   * x0-3 are either temps or array elems
   * y0-3 are either temps, array elems, constants
   * z0-3 are either temps, array elems, or constants
   *
   * The general procedure is to:
   *   1. pack y0-3 into vector y
   *   2. pack z0-3 into vector z
   *   3. perform x = y OP z
   *   4. unpack x into x0-3
   *
   * We are imposing the following restrictions to limit the number of cases
   * we can handle:
   *  1. if src_i is an array elem, then all src_j have to be array elems of the
   *     same array. this way we will only have to do a bitcast to convert
   *     src0-3 into a vector
   *  2. if dest_i is an array elem, then all dest_j have to
   *     be array elems of the same array. this way we will only have to do a
   *     bitcast to convert dest0-3 into a vector
   *
   * For now, we are just doing the simple, naive thing where we pack
   * everything, do the op, then unpack
   */
  void codeGen(PackSet &P) {
    // pack operands into a vector
    auto packOperands = [](std::vector<Value *> &operands,
                           IRBuilder<> &builder) {
      auto *vecType = VectorType::get(operands[0]->getType(), operands.size());
      Value *vec = UndefValue::get(vecType);
      for (int i = 0; i < operands.size(); i++) {
        vec = builder.CreateInsertElement(vec, operands[i], i);
      }
      return vec;
    };

    // iterate over all packs in the scheduledPackList
    for (auto packListIter = P.lbegin(); packListIter != P.lend();
         packListIter++) {
      Pack *pack = *packListIter;

      // IRBuilder for this pack
      // have it start adding new instructions before first instruction of pack
      IRBuilder<> builder(pack->getFirstElement());

      // separate operand0s and operand1s
      std::vector<Value *> operand0s, operand1s;
      for (int i = 0; i < pack->getSize(); i++) {
        auto *instr = pack->getNthElement(i);
        operand0s.push_back(instr->getOperand(0));
        operand0s.push_back(instr->getOperand(1));
      }

      // pack operands
      auto *operand0Vec = packOperands(operand0s, builder);
      auto *operand1Vec = packOperands(operand1s, builder);

      // vector bin op
      auto *destVec =
          builder.CreateBinOp(pack->getBinOpCode(), operand0Vec, operand1Vec);

      // unpack res
      for (int i = 0; i < pack->getSize(); i++) {
        auto *instr = pack->getNthElement(i);
        auto *newVal = builder.CreateExtractElement(destVec, i);
        instr->replaceAllUsesWith(newVal);
      }

      // delete all instrs in pack
      for (int i = 0; i < pack->getSize(); i++) {
        pack->getNthElement(i)->eraseFromParent();
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

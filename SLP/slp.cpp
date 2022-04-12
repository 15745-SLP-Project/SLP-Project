#include "slp.hpp"

Value *Pack::getOperand(unsigned int n, PackSet &P) {
  assert(pack.size() > 0);
  assert(n < pack[0]->getNumOperands());
  Pack *p = P.findPack((Instruction *)(pack[0]->getOperand(n)));
  return p->getValue();
}

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
      baseAddress.clear();
      alignInfo.clear();
      changed |= slpExtract(BB);
    }
    return changed;
  }

  bool slpExtract(BasicBlock &BB) {
    PackSet P;
    findAdjRefs(BB, P);
    extendPacklist(BB, P);
    combinePacks(P);
    P.printPackSet();
    bool sched = P.schedule();
    if (sched) {
      P.printScheduledPackList();
      P.findPrePack();
      P.findPostPack();
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

  /*
   * Check alignment information
   *
   * Check whether s1 and s2 shares the same base address, same induction
   * variable, and the index differs by offset
   */
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
    if (isIsomorphic(s1, s2) && isIndependent(s1, s2) && (s1 != s2)) {
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
      Instruction *t1 = dyn_cast<Instruction>(s1->getOperand(j));
      Instruction *t2 = dyn_cast<Instruction>(s2->getOperand(j));
      if (!t1 || !t2) {
        continue;
      }
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
    outs() << "Code generation\n";

    // iterate over all packs in the scheduledPackList
    for (auto packListIter = P.lbegin(); packListIter != P.lend();
         packListIter++) {
      Pack *pack = *packListIter;

      // IRBuilder for this pack
      // have it start adding new instructions before first instruction of pack
      IRBuilder<> builder(pack->getFirstElement());

      unsigned int opcode = pack->getOpcode();
      unsigned int vecWidth = pack->getVecWidth();
      Type *type = pack->getType();

      // Vector types
      auto vecType = VectorType::get(type, vecWidth);
      auto vecPtrType = PointerType::get(vecType, 0);

      switch (opcode) {
      case Instruction::Load: {
        // Load pointer
        auto align = getAlignment(pack->getFirstElement());
        auto iv = align->inductionVar;
        auto firstLoad = dyn_cast<LoadInst>(pack->getFirstElement());
        auto basePtr = firstLoad->getPointerOperand();
        auto vecPtr = builder.CreateBitCast(basePtr, vecPtrType);

        outs() << "\t" << *vecPtr << "\n";

        // Load instruction
        auto load = builder.CreateLoad(vecType, vecPtr);
        pack->setDest(load);

        outs() << "\t" << *load << "\n";
        break;
      }
      case Instruction::Store: {
        // Store pointer
        auto align = getAlignment(pack->getFirstElement());
        auto iv = align->inductionVar;
        auto firstLoad = dyn_cast<StoreInst>(pack->getFirstElement());
        auto basePtr = firstLoad->getPointerOperand();
        auto vecPtr = builder.CreateBitCast(basePtr, vecPtrType);

        outs() << "\t" << *vecPtr << "\n";

        // Store instruction
        auto store = builder.CreateStore(pack->getOperand(0, P), vecPtr);

        outs() << "\t" << *store << "\n";
        break;
      }
      case Instruction::Add:
      case Instruction::Mul: {
        auto binOp = builder.CreateBinOp(
            pack->getBinOp(), pack->getOperand(0, P), pack->getOperand(1, P));
        pack->setDest(binOp);

        outs() << "\t" << *binOp << "\n";
        break;
      }
      default:
        outs() << "Unsupported opcode " << opcode << "\n";
      }

      // Delete all the instructions in pack
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

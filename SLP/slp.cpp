#include "slp.hpp"

Value *Pack::getOperand(unsigned int n, PackSet &P) {
  assert(pack.size() > 0);
  assert(n < pack[0]->getNumOperands());
  Pack *p = P.findPack((Instruction *)(pack[0]->getOperand(n)));
  if (p == nullptr)
    return nullptr;
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
    outs() << "-----" << F.getName() << "-----\n\n";

    bool changed = false;

    for (auto &BB : F) {
      baseAddress.clear();
      alignInfo.clear();
      changed |= slpExtract(BB);
    }

    if (changed)
      outs() << F.getName() << " SLP completed\n";
    outs() << "\n";
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
      codeGen(P);
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
    unsigned int divisor = UINT32_MAX;
    for (auto &s : BB) {
      // Only look at memory access instructions
      if (s.mayReadOrWriteMemory()) {
        // getelementptr instruction
        GetElementPtrInst *gep = nullptr;

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
        if (!gep || gep->getNumIndices() > 2) {
          continue;
        }
        unsigned int gepNumIndices = gep->getNumIndices();

        // When GEP uses global address (e.g., @A in foo), check whether the
        // first index is 0
        if (gepNumIndices == 2) {
          if (auto gepIndex0 = dyn_cast<ConstantInt>(gep->getOperand(1))) {
            if (gepIndex0->getZExtValue() != 0) {
              continue;
            }
          }
        }

        // Base address
        Value *b = gep->getPointerOperand();
        baseAddress.insert(b);

        // Find the induction variable
        Value *v = gep->getOperand(gepNumIndices);
        unsigned int index = 0;
        Value *operand0, *operand1;

        while (auto addInst = dyn_cast<BinaryOperator>(v)) {
          if (addInst->getOpcode() == Instruction::Add ||
              addInst->getOpcode() == Instruction::Or) {
            operand0 = addInst->getOperand(0);
            operand1 = addInst->getOperand(1);
            if (isa<ConstantInt>(operand1)) {
              index += cast<ConstantInt>(operand1)->getZExtValue();
            }
            v = operand0;
          } else {
            break;
          }
        }
        setAlignment(&s, b, v, index);
        // Find the minimum positive divisor for all alignment info
        if (index > 0 && index < divisor) {
          divisor = index;
        }
      }
    }

    if (divisor == UINT32_MAX) {
      return;
    }

    for (auto alignInfoEntry : alignInfo) {
      auto instr = alignInfoEntry.first;
      auto align = getAlignment(instr);

      // Need to further check the correctness
      // setAlignmentIndex(instr, align->index / divisor);

      if (verbose)
        outs() << "[setAlignRef] set alignment for (" << *instr
               << "), base = " << align->base->getName()
               << ", iv = " << align->inductionVar->getName()
               << ", index = " << align->index << "\n";
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

  void setAlignmentIndex(Instruction *s, unsigned int newIndex) {
    auto align = getAlignment(s);
    if (align) {
      align->index = newIndex;
    }
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
    /*
      - if all operands are not from the same pack, then this means
      that there is not currently a llvm vec that holds all of them
        - so we need to create a new llvm vec to hold them
        - for each operand
          - if operand comes from a pack
            - ExtractElem
            - prepack into new llvm vec
          - else
            - prepack into new llvm vec
        - return new llvm vec

      - else
        - return pack's llvm vec
    */
    auto getOperandVec = [](IRBuilder<> &builder, PackSet &P, Pack *pack,
                            int operandNum) {
      // determine if all operands comes from same pack
      bool fromSamePack = true;
      Pack *samePack = nullptr;
      for (auto packIter = pack->begin(); packIter != pack->end(); packIter++) {
        Instruction *instr = *packIter;
        Value *operand = instr->getOperand(operandNum);

        // if operand is defined by instruction
        if (Instruction *def = dyn_cast<Instruction>(operand)) {
          // get the pack which defines this operand
          Pack *operandPack = P.findPack(def);

          if (operandPack == nullptr) {
            fromSamePack = false;
            break;
          }

          // first iter, so just set samePack to the pack of first instr's
          // operand
          if (samePack == nullptr) {
            samePack = operandPack;
          }
          // if packs dont match, then not all operands from same pack
          else if (samePack != operandPack) {
            fromSamePack = false;
            break;
          }
        }

        // if operand is not defined by instruction, then we definitely need to
        // pack
        else {
          fromSamePack = false;
          break;
        }
      }

      // if from the same pack, then just return the pack's vec
      if (fromSamePack) {
        return pack->getOperand(operandNum, P);
      }

      // not from same pack, so need to prepack operands
      else {

        // determine element type in vector
        Type *baseType;
        if (pack->getOpcode() == Instruction::Store) {
          auto *storeInstr = dyn_cast<StoreInst>(pack->getFirstElement());
          baseType = storeInstr->getValueOperand()->getType();
        } else {
          baseType = pack->getFirstElement()->getType();
        }

        // create new vec
        auto *vecType = VectorType::get(baseType, pack->getSize());
        auto *zero = builder.getInt32(0);
        auto *size = builder.getInt32(1);
        auto *initVec = UndefValue::get(vecType);

        Value *currVec = initVec;

        for (int i = 0; i < pack->getSize(); i++) {
          Instruction *instr = pack->getNthElement(i);
          Value *operand = instr->getOperand(operandNum);

          // if operand is instruction, means it was defined before
          if (Instruction *def = dyn_cast<Instruction>(operand)) {
            // get the pack which defines this operand
            Pack *operandPack = P.findPack(def);

            // if operand is not in a pack, then can directly insert into
            // currVec
            if (operandPack == nullptr) {
              currVec = builder.CreateInsertElement(currVec, def, i);
              outs() << "\t" << *currVec << "\n";
            }
            // operand is in a pack, so need to extract it and insert it
            else {
              // get index of operand in its pack
              int index = operandPack->getIndex(def);
              auto *newDef =
                  builder.CreateExtractElement(operandPack->getValue(), index);
              outs() << "\t" << *newDef << "\n";
              currVec = builder.CreateInsertElement(currVec, newDef, i);
              outs() << "\t" << *currVec << "\n";
            }
          }

          // operand was not instruction, so just insert it
          else {
            currVec = builder.CreateInsertElement(currVec, operand, i);
            outs() << "\t" << *currVec << "\n";
          }
        }

        // return new vec
        return currVec;
      }
    };

    outs() << "Code generation\n";

    std::map<Pack *, bool> shouldDelete;

    // iterate over all packs in the scheduledPackList
    for (auto packListIter = P.lbegin(); packListIter != P.lend();
         packListIter++) {
      Pack *pack = *packListIter;
      shouldDelete[pack] = true;

      // only do codegen if pack is not independent stores
      if (dyn_cast<StoreInst>(pack->getFirstElement())) {
        if (!P.hasDependency(pack)) {
          shouldDelete[pack] = false;
          continue;
        }
      }

      // IRBuilder for this pack
      // have it start adding new instructions before last instruction of pack
      IRBuilder<> builder(pack->getLastElement());

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
        auto firstStore = dyn_cast<StoreInst>(pack->getFirstElement());
        auto basePtr = firstStore->getPointerOperand();
        auto vecPtr = builder.CreateBitCast(basePtr, vecPtrType);

        outs() << "\t" << *vecPtr << "\n";

        // Store instruction
        Value *operand0 = getOperandVec(builder, P, pack, 0);
        auto store = builder.CreateStore(operand0, vecPtr);

        outs() << "\t" << *store << "\n";
        break;
      }

      case Instruction::Call: {
        // Intrinsic call instructions
        if (auto intrinsicInst =
                dyn_cast<IntrinsicInst>(pack->getFirstElement())) {
          // Function return type
          std::vector<Type *> types;
          types.push_back(vecType);
          auto typesArrayRef = ArrayRef<Type *>(types);

          // Function arguments
          std::vector<Value *> values;
          for (unsigned int i = 0; i < intrinsicInst->getNumArgOperands();
               i++) {
            values.push_back(getOperandVec(builder, P, pack, i));
          }
          auto valuesArrayRef = ArrayRef<Value *>(values);

          auto intrinsic = builder.CreateIntrinsic(
              intrinsicInst->getIntrinsicID(), typesArrayRef, valuesArrayRef);
          pack->setDest(intrinsic);
          outs() << "\t" << *intrinsic << "\n";
        } else {
          outs() << "Unsupported instruction call\n";
        }
        break;
      }

      default: {
        if (isa<BinaryOperator>(pack->getFirstElement())) {
          Value *operand0 = getOperandVec(builder, P, pack, 0);
          Value *operand1 = getOperandVec(builder, P, pack, 1);
          auto binOp =
              builder.CreateBinOp(pack->getBinOp(), operand0, operand1);
          pack->setDest(binOp);

          outs() << "\t" << *binOp << "\n";
          break;
        } else {
          outs() << "Unsupported opcode " << opcode << " ("
                 << pack->getFirstElement()->getOpcodeName() << ")\n";
        }
      }
      }

      /*
      there may be instructions not within a pack that will require the output
      of one of the instructions in this pack. in this case, extract the item
      out of the pack and replace it as the operand of the depdent instruction
      */
      for (int i = 0; i < pack->getSize(); i++) {
        Instruction *def = pack->getNthElement(i);
        for (auto *user : def->users()) {
          Instruction *userInstr = cast<Instruction>(user);
          // if userInstr not in pack
          if (P.findPack(userInstr) == nullptr) {
            int index = pack->getIndex(def);
            auto *newDef =
                builder.CreateExtractElement(pack->getValue(), index);
            // replace def with newDef
            def->replaceAllUsesWith(newDef);
          }
        }
      }
    }

    // Delete all the instructions in all packs
    for (auto packListIter = P.lbegin(); packListIter != P.lend();
         packListIter++) {
      Pack *pack = *packListIter;
      if (shouldDelete[pack]) {
        for (int i = 0; i < pack->getSize(); i++) {
          pack->getNthElement(i)->eraseFromParent();
        }
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

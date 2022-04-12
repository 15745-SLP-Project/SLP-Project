#ifndef __SLP_SLP_HPP__
#define __SLP_SLP_HPP__

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

class PackSet;

/*
 * A Pack is an n-tuple, <s1, ..., sn>, where s1, ..., sn are independent
 * isomorphic statements in a basic block
 */
class Pack {
public:
  Pack() : value(nullptr) {}

  Pack(Instruction *s1, Instruction *s2) : value(nullptr) {
    pack.push_back(s1);
    pack.push_back(s2);
  }

  // Only used to combine two packs
  Pack(std::vector<Instruction *> v1, std::vector<Instruction *> v2)
      : value(nullptr) {
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

  unsigned int getOpcode() {
    assert(pack.size() > 0);
    return pack[0]->getOpcode();
  }

  Type *getType() {
    assert(pack.size() > 0);
    if (auto storeInst = dyn_cast<StoreInst>(pack[0])) {
      PointerType *ptrType = (PointerType *)storeInst->getPointerOperandType();
      return ptrType->getPointerElementType();
    }
    return pack[0]->getType();
  }

  unsigned int getVecWidth() {
    return pack.size();
  }

  void setDest(Value *dest) {
    value = dest;
  }

  Value *getValue() {
    return value;
  }

  Value *getOperand(unsigned int n, PackSet &P);

  Instruction::BinaryOps getBinOp() {
    assert(pack.size() > 0);
    assert(pack[0]->isBinaryOp());
    auto binOp = dyn_cast<BinaryOperator>(pack[0]);
    return binOp->getOpcode();
  }

private:
  std::vector<Instruction *> pack;

  // Destination value
  Value *value;
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
    if (packSet.size() == 0)
      return;

    outs() << "PackSet\n";
    unsigned int index = 0;
    for (auto &p : packSet) {
      p.print(index);
      index++;
    }
    outs() << "\n";
  }

  void printScheduledPackList() {
    if (scheduledPackList.size() == 0)
      return;

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

  // Combine pack p1 and p2, only used in the combination process
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

    // Already scheduled packs
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

  // Find the pack of instruction s
  Pack *findPack(Instruction *s) {
    for (auto &p : packSet) {
      for (auto &i : p) {
        if (s == i) {
          return &p;
        }
      }
    }
    return nullptr;
  }

private:
  /*
   * packSet, stored in a vector
   *
   * The reason why we don't use std::set is that the default iterator is
   * always const, so instead we use std::vector to replace the set.
   */
  std::vector<Pack> packSet;

  /*
   * Dependency graph
   * dependency[A] = {B, C} means that the pack A depends on B, C.
   */
  std::map<Pack *, std::set<Pack *>> dependency;

  /*
   * Scheduled pack list, sorted by topological order according to the
   * dependency graph built earlier.
   */
  std::vector<Pack *> scheduledPackList;

  // Add pack p to packSet (stored in a vector)
  void add(Pack &&p) {
    for (auto &t : packSet) {
      if (t == p) {
        return;
      }
    }
    packSet.emplace_back(p);
  }

  // Erase pack p from packSet (stored in a vector)
  void erase(Pack &p) {
    for (auto iter = packSet.begin(); iter != packSet.end(); iter++) {
      if (p == *iter) {
        packSet.erase(iter);
      }
    }
  }

  // Construct the dependency graph
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
      if (verbose) {
        outs() << "The pack " << p << " depends on packs:";
        for (auto pDep : pDeps) {
          outs() << " " << pDep;
        }
        outs() << "\n";
      }
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

#endif // __SLP_UTILS_HPP__

#include "utils.hpp"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/raw_ostream.h"

bool isIsomorphic(Instruction *s1, Instruction *s2) {
  bool bothBinaryOperator = isa<BinaryOperator>(s1) && isa<BinaryOperator>(s2);
  bool bothLoadInst = isa<LoadInst>(s1) && isa<LoadInst>(s2);
  bool bothStoreInst = isa<StoreInst>(s1) && isa<StoreInst>(s2);
  bool bothIntrinsicCallInst = false;
  auto c1 = dyn_cast<IntrinsicInst>(s1);
  auto c2 = dyn_cast<IntrinsicInst>(s2);
  if (c1 && c2) {
    auto i1 = c1->getIntrinsicID();
    auto i2 = c2->getIntrinsicID();
    if (i1 == i2) {
      bothIntrinsicCallInst = true;
    }
  }

  return (s1->getOpcode() == s2->getOpcode()) &&
         (s1->getType() == s2->getType()) &&
         (bothBinaryOperator || bothLoadInst || bothStoreInst ||
          bothIntrinsicCallInst);
}

bool isDependentOn(Instruction *s, Instruction *sDep) {
  for (auto *sDepUser : sDep->users()) {
    if ((Value *)sDepUser == (Value *)s) {
      return true;
    }
  }
  return false;
}

bool isIndependent(Instruction *s1, Instruction *s2) {
  return (!isDependentOn(s1, s2)) && (!isDependentOn(s2, s1));
}

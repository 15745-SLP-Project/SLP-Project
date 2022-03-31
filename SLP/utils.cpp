#include "utils.hpp"

bool isIsomorphic(Instruction *s1, Instruction *s2) {
  return (s1->getOpcode() == s2->getOpcode()) &&
         (s1->getType() == s2->getType()) && (isa<BinaryOperator>(s1)) &&
         (isa<BinaryOperator>(s2));
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

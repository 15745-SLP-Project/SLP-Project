#ifndef __SLP_UTILS_HPP__
#define __SLP_UTILS_HPP__

#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

// If two instructions have the same operation and type, and both are binary
// operations, they are isomorphic
bool isIsomorphic(Instruction *s1, Instruction *s2);

// Check whether s depends on sDep (RAW data dependency)
bool isDependentOn(Instruction *s, Instruction *sDep);

// If two instructions have no dependency, they are independent
bool isIndependent(Instruction *s1, Instruction *s2);

#endif // __SLP_UTILS_HPP__

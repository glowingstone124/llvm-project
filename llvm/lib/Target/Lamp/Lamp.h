#ifndef LLVM_LIB_TARGET_LAMP_LAMP_H
#define LLVM_LIB_TARGET_LAMP_LAMP_H

#include "llvm/Support/CodeGen.h"
#include "llvm/Support/Compiler.h"

namespace llvm {
class FunctionPass;
class LampTargetMachine;
class PassRegistry;
class Target;

FunctionPass *createLampISelDag(LampTargetMachine &TM, CodeGenOptLevel OptLevel);
FunctionPass *createLampZeroBranchPrepPass();

void initializeLampDAGToDAGISelLegacyPass(PassRegistry &);

LLVM_ABI Target &getTheLampTarget();

} // namespace llvm

#endif

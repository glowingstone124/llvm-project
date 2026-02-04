#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

Target &getTheLampTarget();

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeLampTargetMC() {
  (void)getTheLampTarget();
}

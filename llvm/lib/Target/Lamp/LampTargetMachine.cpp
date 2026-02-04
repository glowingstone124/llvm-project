#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

Target &getTheLampTarget();

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeLampTarget() {
}

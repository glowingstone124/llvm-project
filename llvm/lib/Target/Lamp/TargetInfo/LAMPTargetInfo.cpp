#include "llvm/Support/TargetRegistry.h"
using namespace llvm;

Target &getTheLampTarget() {
  static Target TheTarget;
  return TheTarget;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeLampTargetInfo() {
  RegisterTarget<Triple::UnknownArch, /*HasJIT=*/false>
      X(getTheLAMPTarget(), "lamp", "LampVM Target", "LAMP");
}
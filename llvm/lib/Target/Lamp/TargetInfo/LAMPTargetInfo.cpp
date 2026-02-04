#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

Target &getTheLampTarget() {
  static Target TheTarget;
  return TheTarget;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeLampTargetInfo() {
  RegisterTarget<Triple::UnknownArch, /*HasJIT=*/false>
      X(getTheLampTarget(), "lamp", "LampVM Target", "Lamp");
}
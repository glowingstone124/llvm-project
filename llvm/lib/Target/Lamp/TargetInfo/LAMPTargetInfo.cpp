#include "LAMPTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

Target &llvm::getTheLampTarget() {
  static Target TheLampTarget;
  return TheLampTarget;
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeLampTargetInfo() {
  RegisterTarget<Triple::UnknownArch, /*HasJIT=*/false>
      X(getTheLampTarget(), "lamp", "LampVM Target", "Lamp");
}

#include "LampMCTargetDesc.h"
#include "LampInstPrinter.h"
#include "TargetInfo/LAMPTargetInfo.h"
#include "llvm/MC/MCAsmInfoELF.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

namespace {
class LampMCAsmInfo : public llvm::MCAsmInfoELF {
public:
  LampMCAsmInfo() {
    // Keep MCAsmInfo consistent with Triple::getDefaultExceptionHandling()
    // for Triple::lamp to satisfy target machine initialization checks.
    ExceptionsType = llvm::ExceptionHandling::DwarfCFI;
    // Lamp now provides an MC asm parser; keep inline asm on the normal
    // integrated-assembler path for both .s output and object emission.
    UseIntegratedAssembler = true;
    ParseInlineAsmUsingAsmParser = true;
  }
};
} // namespace

using namespace llvm;

#define GET_INSTRINFO_MC_DESC
#define ENABLE_INSTR_PREDICATE_VERIFIER
#include "LampGenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "LampGenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "LampGenRegisterInfo.inc"

static MCInstrInfo *createLampMCInstrInfo() {
  auto *X = new MCInstrInfo();
  InitLampMCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createLampMCRegisterInfo(const Triple &TT) {
  auto *X = new MCRegisterInfo();
  InitLampMCRegisterInfo(X, Lamp::R0);
  return X;
}

static MCAsmInfo *createLampMCAsmInfo(const MCRegisterInfo &MRI,
                                      const Triple &TT,
                                      const MCTargetOptions &Options) {
  return new LampMCAsmInfo();
}

static MCInstPrinter *createLampMCInstPrinter(const Triple &T,
                                                unsigned SyntaxVariant,
                                                const MCAsmInfo &MAI,
                                                const MCInstrInfo &MII,
                                                const MCRegisterInfo &MRI) {
  if (SyntaxVariant == 0)
    return new LampInstPrinter(MAI, MII, MRI);
  return nullptr;
}

static MCSubtargetInfo *createLampMCSubtargetInfo(const Triple &TT,
                                                   StringRef CPU,
                                                   StringRef FS) {
  return createLampMCSubtargetInfoImpl(TT, CPU, CPU, FS);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeLampTargetMC() {
  Target &T = getTheLampTarget();

  TargetRegistry::RegisterMCAsmInfo(T, createLampMCAsmInfo);
  TargetRegistry::RegisterMCInstrInfo(T, createLampMCInstrInfo);
  TargetRegistry::RegisterMCRegInfo(T, createLampMCRegisterInfo);
  TargetRegistry::RegisterMCSubtargetInfo(T, createLampMCSubtargetInfo);
  TargetRegistry::RegisterMCInstPrinter(T, createLampMCInstPrinter);
  TargetRegistry::RegisterMCCodeEmitter(T, createLampMCCodeEmitter);
  TargetRegistry::RegisterMCAsmBackend(T, createLampMCAsmBackend);
}

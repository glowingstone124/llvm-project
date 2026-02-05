#include "LampSubtarget.h"
#include "LampTargetMachine.h"

using namespace llvm;

#define DEBUG_TYPE "lamp-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "LampGenSubtargetInfo.inc"

void LampSubtarget::anchor() {}

LampSubtarget::LampSubtarget(const Triple &TT, StringRef CPU, StringRef FS,
                             const LampTargetMachine &TM)
    : LampGenSubtargetInfo(TT, CPU, CPU, FS),
      InstrInfo(*this),
      FrameLowering(*this),
      TLInfo(TM, *this) {
  TSInfo = std::make_unique<SelectionDAGTargetInfo>();
}

#ifndef LLVM_LIB_TARGET_LAMP_LAMPSUBTARGET_H
#define LLVM_LIB_TARGET_LAMP_LAMPSUBTARGET_H

#include "LampFrameLowering.h"
#include "LampISelLowering.h"
#include "LampInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"

#define GET_SUBTARGETINFO_HEADER
#include "LampGenSubtargetInfo.inc"

namespace llvm {

class LampTargetMachine;

class LampSubtarget : public LampGenSubtargetInfo {
  LampInstrInfo InstrInfo;
  LampFrameLowering FrameLowering;
  LampTargetLowering TLInfo;

  void anchor();

public:
  LampSubtarget(const Triple &TT, StringRef CPU, StringRef FS,
                const LampTargetMachine &TM);

  void ParseSubtargetFeatures(StringRef CPU, StringRef TuneCPU, StringRef FS);

  const LampInstrInfo *getInstrInfo() const override { return &InstrInfo; }
  const LampRegisterInfo *getRegisterInfo() const override {
    return &InstrInfo.getRegisterInfo();
  }
  const LampFrameLowering *getFrameLowering() const override {
    return &FrameLowering;
  }
  const LampTargetLowering *getTargetLowering() const override {
    return &TLInfo;
  }
};

} // namespace llvm

#endif

#ifndef LLVM_LIB_TARGET_LAMP_LAMPREGISTERINFO_H
#define LLVM_LIB_TARGET_LAMP_LAMPREGISTERINFO_H

#include "llvm/CodeGen/TargetRegisterInfo.h"

#define GET_REGINFO_HEADER
#include "LampGenRegisterInfo.inc"

namespace llvm {

class LampRegisterInfo : public LampGenRegisterInfo {
public:
  LampRegisterInfo();

  const MCPhysReg *getCalleeSavedRegs(const MachineFunction *MF) const override;
  BitVector getReservedRegs(const MachineFunction &MF) const override;
  const TargetRegisterClass *getPointerRegClass(unsigned Kind = 0) const override;

  bool eliminateFrameIndex(MachineBasicBlock::iterator II,
                           int SPAdj,
                           unsigned FIOperandNum,
                           RegScavenger *RS = nullptr) const override;

  Register getFrameRegister(const MachineFunction &MF) const override;
};

} // namespace llvm

#endif

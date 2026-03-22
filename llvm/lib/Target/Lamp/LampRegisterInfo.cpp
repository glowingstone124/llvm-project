#include "LampRegisterInfo.h"
#include "LampFrameLowering.h"
#include "MCTargetDesc/LampMCTargetDesc.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/Support/MathExtras.h"

#define GET_REGINFO_TARGET_DESC
#include "LampGenRegisterInfo.inc"

using namespace llvm;

LampRegisterInfo::LampRegisterInfo() : LampGenRegisterInfo(Lamp::R0) {}

const MCPhysReg *
LampRegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  static const MCPhysReg CalleeSavedRegs[] = {Lamp::R31, 0};
  return CalleeSavedRegs;
}

BitVector LampRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());
  Reserved.set(Lamp::R30);
  Reserved.set(Lamp::R31);
  Reserved.set(Lamp::FLAGS);
  return Reserved;
}

const TargetRegisterClass *
LampRegisterInfo::getPointerRegClass(unsigned Kind) const {
  return &Lamp::PtrRCRegClass;
}

bool LampRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                           int SPAdj,
                                           unsigned FIOperandNum,
                                           RegScavenger *RS) const {
  MachineInstr &MI = *II;
  MachineFunction &MF = *MI.getParent()->getParent();
  const MachineFrameInfo &MFI = MF.getFrameInfo();

  int FI = MI.getOperand(FIOperandNum).getIndex();
  int64_t Offset = MFI.getObjectOffset(FI);
  const uint64_t StackSize =
      alignTo(MFI.getStackSize(),
              MF.getSubtarget().getFrameLowering()->getStackAlign());
  // SP is adjusted in prologue; frame indices are relative to pre-adjusted SP.
  // Round the local allocation to the target stack alignment so frame objects
  // stay aligned even when the raw object area size is not a multiple of 8.
  Offset += StackSize;
  Offset += MI.getOperand(FIOperandNum + 1).getImm();

  MI.getOperand(FIOperandNum).ChangeToRegister(getFrameRegister(MF), false);
  MI.getOperand(FIOperandNum + 1).ChangeToImmediate(Offset);
  return false;
}

Register LampRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return MF.getSubtarget().getFrameLowering()->hasFP(MF) ? Lamp::R31
                                                          : Lamp::R30;
}

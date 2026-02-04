#include "LampRegisterInfo.h"
#include "MCTargetDesc/LampMCTargetDesc.h"
#include "LampFrameLowering.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/ADT/BitVector.h"

#define GET_REGINFO_TARGET_DESC
#include "LampGenRegisterInfo.inc"

using namespace llvm;

LampRegisterInfo::LampRegisterInfo() : LampGenRegisterInfo(Lamp::R0) {}

const MCPhysReg *
LampRegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  return nullptr;
}

BitVector LampRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());
  Reserved.set(Lamp::R30);
  Reserved.set(Lamp::R31);
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
  return false;
}

Register LampRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return Lamp::R30;
}

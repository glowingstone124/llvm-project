#include "LampInstrInfo.h"
#include "LampSubtarget.h"
#include "MCTargetDesc/LampMCTargetDesc.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineMemOperand.h"

#define GET_INSTRINFO_CTOR_DTOR
#include "LampGenInstrInfo.inc"

using namespace llvm;

LampInstrInfo::LampInstrInfo(const LampSubtarget &STI)
    : LampGenInstrInfo(STI, RI, -1, -1) {}

void LampInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator I,
                                const DebugLoc &DL,
                                Register DestReg,
                                Register SrcReg,
                                bool KillSrc,
                                bool RenamableDest,
                                bool RenamableSrc) const {
  BuildMI(MBB, I, DL, get(Lamp::MOV), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc));
}

void LampInstrInfo::storeRegToStackSlot(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator I,
                                        Register SrcReg,
                                        bool isKill,
                                        int FrameIndex,
                                        const TargetRegisterClass *RC,
                                        Register VReg,
                                        MachineInstr::MIFlag Flags) const {
  if (RC != &Lamp::GPRRegClass)
    report_fatal_error("Lamp storeRegToStackSlot: unsupported reg class");

  DebugLoc DL = I != MBB.end() ? I->getDebugLoc() : DebugLoc();
  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIndex),
      MachineMemOperand::MOStore, MFI.getObjectSize(FrameIndex),
      MFI.getObjectAlign(FrameIndex));

  auto MIB = BuildMI(MBB, I, DL, get(Lamp::STORE32))
      .addReg(SrcReg, getKillRegState(isKill))
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .addMemOperand(MMO);
  MIB.setMIFlags(Flags);
}

void LampInstrInfo::loadRegFromStackSlot(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator I,
                                         Register DestReg,
                                         int FrameIndex,
                                         const TargetRegisterClass *RC,
                                         Register VReg,
                                         unsigned SubReg,
                                         MachineInstr::MIFlag Flags) const {
  if (RC != &Lamp::GPRRegClass)
    report_fatal_error("Lamp loadRegFromStackSlot: unsupported reg class");

  DebugLoc DL = I != MBB.end() ? I->getDebugLoc() : DebugLoc();
  MachineFunction &MF = *MBB.getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  MachineMemOperand *MMO = MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIndex),
      MachineMemOperand::MOLoad, MFI.getObjectSize(FrameIndex),
      MFI.getObjectAlign(FrameIndex));

  auto MIB = BuildMI(MBB, I, DL, get(Lamp::LOAD32), DestReg)
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .addMemOperand(MMO);
  MIB.setMIFlags(Flags);
}

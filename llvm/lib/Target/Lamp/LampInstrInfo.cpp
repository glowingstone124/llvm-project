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

static bool isUncondBranchOpcode(unsigned Opc) {
  return Opc == Lamp::JMP || Opc == Lamp::RJMP;
}

static bool isZeroCondBranchOpcode(unsigned Opc) {
  return Opc == Lamp::JZ || Opc == Lamp::JNZ ||
         Opc == Lamp::RJZ || Opc == Lamp::RJNZ;
}

static bool isFlagCondBranchOpcode(unsigned Opc) {
  return Opc == Lamp::JG || Opc == Lamp::JGE || Opc == Lamp::JL ||
         Opc == Lamp::JLE || Opc == Lamp::JC || Opc == Lamp::JNC ||
         Opc == Lamp::RJG || Opc == Lamp::RJGE || Opc == Lamp::RJL ||
         Opc == Lamp::RJLE || Opc == Lamp::RJC || Opc == Lamp::RJNC;
}

static bool isCondBranchOpcode(unsigned Opc) {
  return isZeroCondBranchOpcode(Opc) || isFlagCondBranchOpcode(Opc);
}

static MachineBasicBlock *getBranchDest(const MachineInstr &MI) {
  return MI.getOperand(isZeroCondBranchOpcode(MI.getOpcode()) ? 1 : 0).getMBB();
}

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
  if (RC != &Lamp::GPRRegClass) {
    assert(false && "Lamp storeRegToStackSlot: unsupported reg class");
    return;
  }

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
  if (RC != &Lamp::GPRRegClass) {
    assert(false && "Lamp loadRegFromStackSlot: unsupported reg class");
    return;
  }

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

bool LampInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                  MachineBasicBlock *&TBB,
                                  MachineBasicBlock *&FBB,
                                  SmallVectorImpl<MachineOperand> &Cond,
                                  bool AllowModify) const {
  TBB = nullptr;
  FBB = nullptr;
  Cond.clear();

  auto I = MBB.getLastNonDebugInstr();
  if (I == MBB.end())
    return false;

  if (!I->isBranch())
    return false;

  unsigned LastOpc = I->getOpcode();
  if (!isUncondBranchOpcode(LastOpc) && !isCondBranchOpcode(LastOpc))
    return true;

  MachineInstr *LastBr = &*I;

  if (isUncondBranchOpcode(LastOpc)) {
    TBB = getBranchDest(*LastBr);

    if (I == MBB.begin())
      return false;

    auto Prev = std::prev(I);
    while (Prev != MBB.begin() && Prev->isDebugInstr())
      --Prev;

    unsigned PrevOpc = Prev->getOpcode();
    if (Prev->isBranch() && isCondBranchOpcode(PrevOpc)) {
      TBB = getBranchDest(*Prev);
      FBB = getBranchDest(*LastBr);
      Cond.push_back(MachineOperand::CreateImm(PrevOpc));
      if (isZeroCondBranchOpcode(PrevOpc))
        Cond.push_back(Prev->getOperand(0));
    }
    return false;
  }

  TBB = getBranchDest(*LastBr);
  Cond.push_back(MachineOperand::CreateImm(LastOpc));
  if (isZeroCondBranchOpcode(LastOpc))
    Cond.push_back(LastBr->getOperand(0));
  return false;
}

unsigned LampInstrInfo::removeBranch(MachineBasicBlock &MBB,
                                     int *BytesRemoved) const {
  unsigned Count = 0;
  if (BytesRemoved)
    *BytesRemoved = 0;

  auto I = MBB.getLastNonDebugInstr();
  while (I != MBB.end()) {
    unsigned Opc = I->getOpcode();
    if (!isUncondBranchOpcode(Opc) && !isCondBranchOpcode(Opc))
      break;

    if (BytesRemoved)
      *BytesRemoved += get(Opc).getSize();
    I->eraseFromParent();
    ++Count;

    I = MBB.getLastNonDebugInstr();
  }
  return Count;
}

unsigned LampInstrInfo::insertBranch(MachineBasicBlock &MBB,
                                     MachineBasicBlock *TBB,
                                     MachineBasicBlock *FBB,
                                     ArrayRef<MachineOperand> Cond,
                                     const DebugLoc &DL,
                                     int *BytesAdded) const {
  assert(TBB && "insertBranch requires a target block");
  if (BytesAdded)
    *BytesAdded = 0;

  unsigned Count = 0;
  auto noteBytes = [&](unsigned Opc) {
    if (BytesAdded)
      *BytesAdded += get(Opc).getSize();
  };

  if (Cond.empty()) {
    BuildMI(&MBB, DL, get(Lamp::RJMP)).addMBB(TBB);
    noteBytes(Lamp::RJMP);
    return 1;
  }

  unsigned Opc = Cond[0].getImm();
  if (!isCondBranchOpcode(Opc))
    llvm_unreachable("Lamp insertBranch: unsupported branch condition");

  auto MIB = BuildMI(&MBB, DL, get(Opc));
  if (isZeroCondBranchOpcode(Opc)) {
    assert(Cond.size() == 2 && "zero-branch condition needs a register");
    MIB.add(Cond[1]);
  } else {
    assert(Cond.size() == 1 && "flag branch condition has no explicit regs");
  }
  MIB.addMBB(TBB);
  noteBytes(Opc);
  ++Count;

  if (FBB) {
    BuildMI(&MBB, DL, get(Lamp::RJMP)).addMBB(FBB);
    noteBytes(Lamp::RJMP);
    ++Count;
  }

  return Count;
}

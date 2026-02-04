#include "LampInstrInfo.h"
#include "LampSubtarget.h"
#include "MCTargetDesc/LampMCTargetDesc.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

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

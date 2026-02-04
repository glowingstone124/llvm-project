#include "LampFrameLowering.h"

using namespace llvm;

void LampFrameLowering::emitPrologue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {}

void LampFrameLowering::emitEpilogue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {}

bool LampFrameLowering::hasFPImpl(const MachineFunction &MF) const {
  return false;
}

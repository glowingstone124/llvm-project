#include "LampFrameLowering.h"
#include "LampSubtarget.h"
#include "MCTargetDesc/LampMCTargetDesc.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

void LampFrameLowering::emitPrologue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  uint64_t StackSize = MFI.getStackSize();
  if (!StackSize)
    return;
  assert(isInt<32>(StackSize) && "stack size does not fit in immediate");

  const auto &TII = *MF.getSubtarget<LampSubtarget>().getInstrInfo();
  const DebugLoc DL;
  BuildMI(MBB, MBB.begin(), DL, TII.get(Lamp::SUBI), Lamp::R30)
      .addReg(Lamp::R30)
      .addImm(StackSize);
}

void LampFrameLowering::emitEpilogue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  uint64_t StackSize = MFI.getStackSize();
  if (!StackSize)
    return;
  assert(isInt<32>(StackSize) && "stack size does not fit in immediate");

  const auto &TII = *MF.getSubtarget<LampSubtarget>().getInstrInfo();
  const DebugLoc DL;
  MachineBasicBlock::iterator InsertPt = MBB.getFirstTerminator();
  BuildMI(MBB, InsertPt, DL, TII.get(Lamp::ADDI), Lamp::R30)
      .addReg(Lamp::R30)
      .addImm(StackSize);
}

bool LampFrameLowering::hasFPImpl(const MachineFunction &MF) const {
  return false;
}

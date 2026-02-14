#include "LampFrameLowering.h"
#include "LampSubtarget.h"
#include "MCTargetDesc/LampMCTargetDesc.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

void LampFrameLowering::emitPrologue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  uint64_t StackSize = MFI.getStackSize();
  const bool HasFP = hasFP(MF);
  if (!StackSize && !HasFP)
    return;
  assert(isInt<32>(StackSize) && "stack size does not fit in immediate");

  const auto &TII = *MF.getSubtarget<LampSubtarget>().getInstrInfo();
  const DebugLoc DL;
  auto InsertPt = MBB.begin();
  if (StackSize != 0) {
    BuildMI(MBB, InsertPt, DL, TII.get(Lamp::SUBI), Lamp::R30)
        .addReg(Lamp::R30)
        .addImm(StackSize);
  }
  if (HasFP) {
    BuildMI(MBB, InsertPt, DL, TII.get(Lamp::MOV), Lamp::R31)
        .addReg(Lamp::R30);
  }
}

void LampFrameLowering::emitEpilogue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  uint64_t StackSize = MFI.getStackSize();
  const bool HasFP = hasFP(MF);
  if (!StackSize && !HasFP)
    return;
  assert(isInt<32>(StackSize) && "stack size does not fit in immediate");

  const auto &TII = *MF.getSubtarget<LampSubtarget>().getInstrInfo();
  const DebugLoc DL;
  MachineBasicBlock::iterator InsertPt = MBB.getFirstTerminator();
  if (HasFP) {
    BuildMI(MBB, InsertPt, DL, TII.get(Lamp::MOV), Lamp::R30)
        .addReg(Lamp::R31);
  }
  if (StackSize != 0) {
    BuildMI(MBB, InsertPt, DL, TII.get(Lamp::ADDI), Lamp::R30)
        .addReg(Lamp::R30)
        .addImm(StackSize);
  }
}

bool LampFrameLowering::hasFPImpl(const MachineFunction &MF) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  return MF.getTarget().Options.DisableFramePointerElim(MF) ||
         MFI.hasVarSizedObjects();
}

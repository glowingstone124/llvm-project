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
  const uint64_t StackSize = alignTo(MFI.getStackSize(), getStackAlign());
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
    // Keep the live stack pointer 8-byte aligned at call sites. The frame
    // index math expects FP to name the top of the fixed frame (old SP minus
    // StackSize), so reserve an extra 8-byte call-alignment slot, store the
    // caller FP at FP-4, and then re-anchor FP above the pad.
    BuildMI(MBB, InsertPt, DL, TII.get(Lamp::SUBI), Lamp::R30)
        .addReg(Lamp::R30)
        .addImm(8);
    BuildMI(MBB, InsertPt, DL, TII.get(Lamp::STORE32))
        .addReg(Lamp::R31)
        .addReg(Lamp::R30)
        .addImm(4);
    BuildMI(MBB, InsertPt, DL, TII.get(Lamp::ADDI), Lamp::R31)
        .addReg(Lamp::R30)
        .addImm(8);
  }
}

void LampFrameLowering::emitEpilogue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const uint64_t StackSize = alignTo(MFI.getStackSize(), getStackAlign());
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
    BuildMI(MBB, InsertPt, DL, TII.get(Lamp::LOAD32), Lamp::R31)
        .addReg(Lamp::R30)
        .addImm(-4);
  }
  if (StackSize != 0) {
    BuildMI(MBB, InsertPt, DL, TII.get(Lamp::ADDI), Lamp::R30)
        .addReg(Lamp::R30)
        .addImm(StackSize);
  }
}

bool LampFrameLowering::hasFPImpl(const MachineFunction &MF) const {
  // SP (R30) changes across calls with outgoing stack args, making frame index
  // offsets inconsistent between pre-call and post-call accesses when R30 is
  // the frame register. Always use FP (R31) as the stable frame base.
  return true;
}

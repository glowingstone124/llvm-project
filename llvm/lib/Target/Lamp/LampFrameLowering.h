#ifndef LLVM_LIB_TARGET_LAMP_LAMPFRAMELOWERING_H
#define LLVM_LIB_TARGET_LAMP_LAMPFRAMELOWERING_H

#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {
class LampSubtarget;

class LampFrameLowering : public TargetFrameLowering {
public:
  explicit LampFrameLowering(const LampSubtarget &STI)
      : TargetFrameLowering(StackGrowsDown, Align(4), 0) {}

  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

protected:
  bool hasFPImpl(const MachineFunction &MF) const override;
};

} // namespace llvm

#endif

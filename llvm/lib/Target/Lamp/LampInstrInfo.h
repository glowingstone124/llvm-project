#ifndef LLVM_LIB_TARGET_LAMP_LAMPINSTRINFO_H
#define LLVM_LIB_TARGET_LAMP_LAMPINSTRINFO_H

#include "LampRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#define GET_INSTRINFO_HEADER
#include "LampGenInstrInfo.inc"

namespace llvm {

class LampSubtarget;

class LampInstrInfo : public LampGenInstrInfo {
  const LampRegisterInfo RI;

public:
  explicit LampInstrInfo(const LampSubtarget &STI);

  const LampRegisterInfo &getRegisterInfo() const { return RI; }

  void copyPhysReg(MachineBasicBlock &MBB,
                   MachineBasicBlock::iterator I,
                   const DebugLoc &DL,
                   Register DestReg,
                   Register SrcReg,
                   bool KillSrc,
                   bool RenamableDest = false,
                   bool RenamableSrc = false) const override;
};

} // namespace llvm

#endif

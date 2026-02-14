#ifndef LLVM_LIB_TARGET_LAMP_LAMPMACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_LAMP_LAMPMACHINEFUNCTIONINFO_H

#include "llvm/CodeGen/MachineFunction.h"
#include <limits>

namespace llvm {

class LampMachineFunctionInfo : public MachineFunctionInfo {
  int VarArgsFrameIndex = std::numeric_limits<int>::max();

public:
  LampMachineFunctionInfo(const Function &F, const TargetSubtargetInfo *STI) {}

  MachineFunctionInfo *
  clone(BumpPtrAllocator &Allocator, MachineFunction &DestMF,
        const DenseMap<MachineBasicBlock *, MachineBasicBlock *> &Src2DstMBB)
      const override {
    return DestMF.cloneInfo<LampMachineFunctionInfo>(*this);
  }

  bool hasVarArgsFrameIndex() const {
    return VarArgsFrameIndex != std::numeric_limits<int>::max();
  }

  int getVarArgsFrameIndex() const { return VarArgsFrameIndex; }
  void setVarArgsFrameIndex(int FI) { VarArgsFrameIndex = FI; }
};

} // namespace llvm

#endif

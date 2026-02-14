#ifndef LLVM_LIB_TARGET_LAMP_LAMPTARGETMACHINE_H
#define LLVM_LIB_TARGET_LAMP_LAMPTARGETMACHINE_H

#include "LampSubtarget.h"
#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include "llvm/Support/Allocator.h"
#include <optional>

namespace llvm {

class Function;
struct MachineFunctionInfo;
class TargetSubtargetInfo;

class LampTargetMachine : public CodeGenTargetMachineImpl {
  std::unique_ptr<TargetLoweringObjectFile> TLOF;
  LampSubtarget Subtarget;

public:
  LampTargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                    StringRef FS, const TargetOptions &Options,
                    std::optional<Reloc::Model> RM,
                    std::optional<CodeModel::Model> CM, CodeGenOptLevel OL,
                    bool JIT);

  ~LampTargetMachine() override;

  const LampSubtarget *getSubtargetImpl(const Function &F) const override {
    return &Subtarget;
  }

  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;
  MachineFunctionInfo *
  createMachineFunctionInfo(BumpPtrAllocator &Allocator, const Function &F,
                            const TargetSubtargetInfo *STI) const override;

  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }
};

} // namespace llvm

#endif

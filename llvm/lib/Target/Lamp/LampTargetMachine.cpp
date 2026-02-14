#include "LampTargetMachine.h"
#include "Lamp.h"
#include "LampMachineFunctionInfo.h"
#include "TargetInfo/LAMPTargetInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/Function.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include <optional>

using namespace llvm;

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeLampTarget() {
  RegisterTargetMachine<LampTargetMachine> X(getTheLampTarget());
  PassRegistry &PR = *PassRegistry::getPassRegistry();
  initializeLampDAGToDAGISelLegacyPass(PR);}

static std::string computeDataLayout(const Triple &TT) {
  return "e-m:e-p:32:32-i32:32-n32-S32";
}

static Reloc::Model getEffectiveRelocModel(std::optional<Reloc::Model> RM) {
  return RM.value_or(Reloc::Static);
}

LampTargetMachine::LampTargetMachine(const Target &T, const Triple &TT,
                                     StringRef CPU, StringRef FS,
                                     const TargetOptions &Options,
                                     std::optional<Reloc::Model> RM,
                                     std::optional<CodeModel::Model> CM,
                                     CodeGenOptLevel OL, bool JIT)
    : CodeGenTargetMachineImpl(T, computeDataLayout(TT), TT, CPU, FS, Options,
                               getEffectiveRelocModel(RM),
                               getEffectiveCodeModel(CM, CodeModel::Small), OL),
      TLOF(std::make_unique<TargetLoweringObjectFileELF>()),
      Subtarget(TT, CPU, FS, *this) {
  initAsmInfo();
}

LampTargetMachine::~LampTargetMachine() = default;

namespace {
class LampPassConfig : public TargetPassConfig {
public:
  LampPassConfig(LampTargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  LampTargetMachine &getLampTargetMachine() const {
    return getTM<LampTargetMachine>();
  }

  bool addInstSelector() override {
    addPass(createLampISelDag(getLampTargetMachine(), getOptLevel()));
    return false;
  }
};
} // namespace

TargetPassConfig *LampTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new LampPassConfig(*this, PM);
}

MachineFunctionInfo *LampTargetMachine::createMachineFunctionInfo(
    BumpPtrAllocator &Allocator, const Function &F,
    const TargetSubtargetInfo *STI) const {
  return LampMachineFunctionInfo::create<LampMachineFunctionInfo>(Allocator, F,
                                                                  STI);
}

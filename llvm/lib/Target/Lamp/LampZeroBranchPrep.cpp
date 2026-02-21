#include "Lamp.h"
#include "LampSubtarget.h"
#include "MCTargetDesc/LampMCTargetDesc.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetOpcodes.h"

using namespace llvm;

namespace {
class LampZeroBranchPrepPass : public MachineFunctionPass {
public:
  static char ID;

  LampZeroBranchPrepPass() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override { return "Lamp Zero-Branch Flag Prep"; }

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().setNoVRegs();
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    const auto &TII = *MF.getSubtarget<LampSubtarget>().getInstrInfo();
    bool Changed = false;

    for (MachineBasicBlock &MBB : MF) {
      for (auto It = MBB.begin(); It != MBB.end(); ++It) {
        MachineInstr &MI = *It;
        switch (MI.getOpcode()) {
        case Lamp::JZ:
        case Lamp::JNZ:
        case Lamp::RJZ:
        case Lamp::RJNZ:
          break;
        default:
          continue;
        }

        if (MI.getNumOperands() < 1 || !MI.getOperand(0).isReg())
          continue;

        Register Rs = MI.getOperand(0).getReg();
        if (!Rs)
          continue;

        bool AlreadyPrepared = false;
        if (It != MBB.begin()) {
          auto Prev = It;
          while (Prev != MBB.begin()) {
            --Prev;
            if (Prev->isDebugInstr() || Prev->isCFIInstruction() ||
                Prev->isKill() || Prev->getOpcode() == TargetOpcode::KILL ||
                Prev->getOpcode() == TargetOpcode::IMPLICIT_DEF)
              continue;
            if (Prev->getOpcode() == Lamp::SUBI && Prev->getNumOperands() >= 3 &&
                Prev->getOperand(0).isReg() &&
                Prev->getOperand(2).isImm() &&
                Prev->getOperand(0).getReg() == Rs &&
                Prev->getOperand(2).getImm() == 0) {
              AlreadyPrepared = true;
            }
            break;
          }
        }
        if (AlreadyPrepared)
          continue;

        BuildMI(MBB, It, MI.getDebugLoc(), TII.get(Lamp::SUBI), Rs)
            .addReg(Rs)
            .addImm(0);
        Changed = true;
      }
    }

    return Changed;
  }
};
} // namespace

char LampZeroBranchPrepPass::ID = 0;

FunctionPass *llvm::createLampZeroBranchPrepPass() {
  return new LampZeroBranchPrepPass();
}

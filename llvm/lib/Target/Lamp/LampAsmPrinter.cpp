#include "Lamp.h"
#include "MCTargetDesc/LampMCTargetDesc.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

namespace {

class LampAsmPrinter : public AsmPrinter {
public:
  explicit LampAsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override { return "Lamp Assembly Printer"; }

  bool lowerOperand(const MachineOperand &MO, MCOperand &OutMO) const {
    switch (MO.getType()) {
    case MachineOperand::MO_Register:
      if (!MO.getReg())
        return false;
      OutMO = MCOperand::createReg(MO.getReg());
      return true;
    case MachineOperand::MO_Immediate:
      OutMO = MCOperand::createImm(MO.getImm());
      return true;
    case MachineOperand::MO_MachineBasicBlock:
      OutMO = MCOperand::createExpr(
          MCSymbolRefExpr::create(MO.getMBB()->getSymbol(), OutContext));
      return true;
    case MachineOperand::MO_GlobalAddress:
      OutMO = MCOperand::createExpr(
          MCSymbolRefExpr::create(getSymbol(MO.getGlobal()), OutContext));
      return true;
    case MachineOperand::MO_ExternalSymbol:
      OutMO = MCOperand::createExpr(
          MCSymbolRefExpr::create(GetExternalSymbolSymbol(MO.getSymbolName()),
                                  OutContext));
      return true;
    default:
      return false;
    }
  }

  void emitInstruction(const MachineInstr *MI) override {
    MCInst OutMI;
    OutMI.setOpcode(MI->getOpcode());

    for (const MachineOperand &MO : MI->operands()) {
      MCOperand MCOp;
      if (lowerOperand(MO, MCOp))
        OutMI.addOperand(MCOp);
    }

    EmitToStreamer(*OutStreamer, OutMI);
  }
};

} // namespace

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeLampAsmPrinter() {
  RegisterAsmPrinter<LampAsmPrinter> X(getTheLampTarget());
}

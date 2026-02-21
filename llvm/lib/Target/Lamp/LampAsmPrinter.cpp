#include "Lamp.h"
#include "MCTargetDesc/LampInstPrinter.h"
#include "MCTargetDesc/LampMCTargetDesc.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

namespace {

class LampAsmPrinter : public AsmPrinter {
public:
  explicit LampAsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override { return "Lamp Assembly Printer"; }

  void printOperand(const MachineInstr *MI, int OpNum, raw_ostream &OS) {
    const MachineOperand &MO = MI->getOperand(OpNum);
    switch (MO.getType()) {
    default:
      llvm_unreachable("unsupported operand kind in LampAsmPrinter");
    case MachineOperand::MO_Register:
      OS << LampInstPrinter::getRegisterName(MO.getReg() ? MO.getReg() : Lamp::R0);
      return;
    case MachineOperand::MO_Immediate:
      OS << MO.getImm();
      return;
    case MachineOperand::MO_MachineBasicBlock:
      MO.getMBB()->getSymbol()->print(OS, MAI);
      return;
    case MachineOperand::MO_GlobalAddress:
      getSymbol(MO.getGlobal())->print(OS, MAI);
      if (MO.getOffset() > 0)
        OS << '+' << MO.getOffset();
      else if (MO.getOffset() < 0)
        OS << MO.getOffset();
      return;
    case MachineOperand::MO_ExternalSymbol:
      GetExternalSymbolSymbol(MO.getSymbolName())->print(OS, MAI);
      return;
    }
  }

  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                       const char *ExtraCode, raw_ostream &OS) override {
    if (ExtraCode && ExtraCode[0])
      return AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, OS);
    printOperand(MI, OpNo, OS);
    return false;
  }

  bool PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNo,
                             const char *ExtraCode,
                             raw_ostream &OS) override {
    if (ExtraCode && ExtraCode[0])
      return AsmPrinter::PrintAsmMemoryOperand(MI, OpNo, ExtraCode, OS);
    printOperand(MI, OpNo, OS);
    return false;
  }

  bool lowerOperand(const MachineOperand &MO, MCOperand &OutMO) const {
    auto lowerSymbolExpr = [&](const MachineOperand &SymMO,
                               const MCSymbol *Sym) -> const MCExpr * {
      const MCExpr *Expr = MCSymbolRefExpr::create(Sym, OutContext);
      const int64_t Off = SymMO.getOffset();
      if (Off > 0)
        Expr = MCBinaryExpr::createAdd(
            Expr, MCConstantExpr::create(Off, OutContext), OutContext);
      else if (Off < 0)
        Expr = MCBinaryExpr::createSub(
            Expr, MCConstantExpr::create(-Off, OutContext), OutContext);
      return Expr;
    };

    if (MO.isReg() && MO.isImplicit())
      return false;
    switch (MO.getType()) {
    case MachineOperand::MO_Register:
      OutMO = MCOperand::createReg(MO.getReg() ? MO.getReg() : Lamp::R0);
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
          lowerSymbolExpr(MO, getSymbol(MO.getGlobal())));
      return true;
    case MachineOperand::MO_ExternalSymbol:
      OutMO = MCOperand::createExpr(
          lowerSymbolExpr(MO, GetExternalSymbolSymbol(MO.getSymbolName())));
      return true;
    default:
      return false;
    }
  }

  void emitInstruction(const MachineInstr *MI) override {
    if (MI->isMetaInstruction() || MI->isPseudo())
      return;

    if (MI->getOpcode() == TargetOpcode::COPY) {
      const MachineOperand &Dst = MI->getOperand(0);
      const MachineOperand &Src = MI->getOperand(1);
      if (Dst.isReg() && Src.isReg() && Dst.getReg() && Src.getReg()) {
        MCInst CopyMI;
        CopyMI.setOpcode(Lamp::MOV);
        CopyMI.addOperand(MCOperand::createReg(Dst.getReg()));
        CopyMI.addOperand(MCOperand::createReg(Src.getReg()));
        EmitToStreamer(*OutStreamer, CopyMI);
      }
      return;
    }

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

#include "LampInstPrinter.h"
#include "MCTargetDesc/LampMCTargetDesc.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"

using namespace llvm;

#define PRINT_ALIAS_INSTR
#include "LampGenAsmWriter.inc"

void LampInstPrinter::printRegName(raw_ostream &OS, MCRegister Reg) {
  OS << getRegisterName(Reg);
}

void LampInstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                   raw_ostream &OS) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isReg()) {
    OS << getRegisterName(Op.getReg());
  } else if (Op.isImm()) {
    OS << Op.getImm();
  } else {
    assert(Op.isExpr() && "unknown operand kind");
    MAI.printExpr(OS, *Op.getExpr());
  }
}

void LampInstPrinter::printImm(const MCInst *MI, unsigned OpNo,
                               raw_ostream &OS) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isImm()) {
    OS << Op.getImm();
  } else {
    assert(Op.isExpr() && "imm operand must be imm or expr");
    MAI.printExpr(OS, *Op.getExpr());
  }
}

void LampInstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                StringRef Annot, const MCSubtargetInfo &STI,
                                raw_ostream &OS) {
  if (!printAliasInstr(MI, Address, OS))
    printInstruction(MI, Address, OS);
  printAnnotation(OS, Annot);
}

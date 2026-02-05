#ifndef LLVM_LIB_TARGET_LAMP_MCTARGETDESC_LAMPINSTPRINTER_H
#define LLVM_LIB_TARGET_LAMP_MCTARGETDESC_LAMPINSTPRINTER_H

#include "llvm/MC/MCInstPrinter.h"

namespace llvm {

class LampInstPrinter : public MCInstPrinter {
public:
  LampInstPrinter(const MCAsmInfo &MAI, const MCInstrInfo &MII,
                  const MCRegisterInfo &MRI)
      : MCInstPrinter(MAI, MII, MRI) {}

  void printRegName(raw_ostream &OS, MCRegister Reg) override;

  void printInst(const MCInst *MI, uint64_t Address, StringRef Annot,
                 const MCSubtargetInfo &STI, raw_ostream &OS) override;

  std::pair<const char *, uint64_t> getMnemonic(const MCInst &MI) const override;
  void printInstruction(const MCInst *MI, uint64_t Address, raw_ostream &OS);
  bool printAliasInstr(const MCInst *MI, uint64_t Address, raw_ostream &OS);
  void printCustomAliasOperand(const MCInst *MI, uint64_t Address,
                               unsigned OpIdx, unsigned PrintMethodIdx,
                               raw_ostream &OS);

  static const char *getRegisterName(MCRegister Reg);

private:
  void printOperand(const MCInst *MI, unsigned OpNo, raw_ostream &OS);
  void printImm(const MCInst *MI, unsigned OpNo, raw_ostream &OS);
};

} // namespace llvm

#endif

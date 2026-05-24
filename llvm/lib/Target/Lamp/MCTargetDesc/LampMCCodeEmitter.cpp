#include "MCTargetDesc/LampMCTargetDesc.h"
#include "MCTargetDesc/LampFixupKinds.h"

#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

enum class LampInstForm {
  None,
  I,
  Target,
  RsTarget,
  Rd,
  RdImm,
  RdRs,
  RdRsRs,
  RsRs,
  RsImm,
  RdRsImm,
  RsRsImm,
  RdRsRsImm,
  RdRsRsRsImm,
  Call,
};

class LampMCCodeEmitter : public MCCodeEmitter {
public:
  explicit LampMCCodeEmitter(MCContext &Ctx) : Ctx(Ctx) {}

  ~LampMCCodeEmitter() override = default;

  void encodeInstruction(const MCInst &MI, SmallVectorImpl<char> &CB,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override;

private:
  MCContext &Ctx;

  uint8_t getOpcodeByte(unsigned Opc) const;
  LampInstForm getInstForm(unsigned Opc) const;
  bool isPCRelativeTargetOpcode(unsigned Opc) const;

  uint8_t encodeReg(const MCOperand &Op) const;
  uint32_t encodeImm(const MCOperand &Op, SmallVectorImpl<MCFixup> &Fixups,
                     MCFixupKind Kind = Lamp::fixup_lamp_32,
                     bool IsPCRel = false) const;
};

uint8_t LampMCCodeEmitter::getOpcodeByte(unsigned Opc) const {
  switch (Opc) {
  case Lamp::ADD: return 0x01;
  case Lamp::SUB: return 0x02;
  case Lamp::MUL: return 0x03;
  case Lamp::DIV: return 0x04;
  case Lamp::HALT: return 0x05;
  case Lamp::JMP: return 0x06;
  case Lamp::JZ: return 0x07;
  case Lamp::PUSH: return 0x08;
  case Lamp::POP: return 0x09;
  case Lamp::RJMP: return 0x4A;
  case Lamp::CALL: return 0x0A;
  case Lamp::RCALL: return 0x4B;
  case Lamp::CALLR: return 0x49;
  case Lamp::RET: return 0x0B;
  case Lamp::LOAD: return 0x0C;
  case Lamp::LOAD16: return 0x52;
  case Lamp::LOAD32: return 0x0D;
  case Lamp::LOADS8: return 0x54;
  case Lamp::LOADS16: return 0x55;
  case Lamp::LOADX32: return 0x0E;
  case Lamp::STORE: return 0x0F;
  case Lamp::STORE16: return 0x53;
  case Lamp::STORE32: return 0x10;
  case Lamp::STOREX32: return 0x11;
  case Lamp::CMP: return 0x12;
  case Lamp::CMPI: return 0x13;
  case Lamp::MOV: return 0x14;
  case Lamp::MOVI: return 0x15;
  case Lamp::MEMSET: return 0x16;
  case Lamp::MEMCPY: return 0x17;
  case Lamp::IN: return 0x18;
  case Lamp::OUT: return 0x19;
  case Lamp::INT: return 0x1A;
  case Lamp::INTI: return 0x5C;
  case Lamp::IRET: return 0x1B;
  case Lamp::MOD: return 0x1C;
  case Lamp::AND: return 0x1D;
  case Lamp::OR: return 0x1E;
  case Lamp::XOR: return 0x1F;
  case Lamp::NOT: return 0x20;
  case Lamp::SHL: return 0x21;
  case Lamp::SHR: return 0x22;
  case Lamp::SAR: return 0x23;
  case Lamp::ROL: return 0x4E;
  case Lamp::ROR: return 0x4F;
  case Lamp::JNZ: return 0x24;
  case Lamp::RJZ: return 0x4C;
  case Lamp::RJNZ: return 0x4D;
  case Lamp::RJG: return 0x56;
  case Lamp::RJGE: return 0x57;
  case Lamp::RJL: return 0x58;
  case Lamp::RJLE: return 0x59;
  case Lamp::RJC: return 0x5A;
  case Lamp::RJNC: return 0x5B;
  case Lamp::JG: return 0x25;
  case Lamp::JGE: return 0x26;
  case Lamp::JL: return 0x27;
  case Lamp::JLE: return 0x28;
  case Lamp::JC: return 0x29;
  case Lamp::JNC: return 0x2A;
  case Lamp::FADD: return 0x2B;
  case Lamp::FSUB: return 0x2C;
  case Lamp::FMUL: return 0x2D;
  case Lamp::FDIV: return 0x2E;
  case Lamp::FNEG: return 0x2F;
  case Lamp::FABS: return 0x30;
  case Lamp::FSQRT: return 0x31;
  case Lamp::FCMP: return 0x32;
  case Lamp::ITOF: return 0x33;
  case Lamp::FTOI: return 0x34;
  case Lamp::FLOAD32: return 0x35;
  case Lamp::FSTORE32: return 0x36;
  case Lamp::INC: return 0x37;
  case Lamp::ADDI: return 0x38;
  case Lamp::SUBI: return 0x39;
  case Lamp::ANDI: return 0x3A;
  case Lamp::ORI: return 0x3B;
  case Lamp::XORI: return 0x3C;
  case Lamp::SHLI: return 0x3D;
  case Lamp::SHRI: return 0x3E;
  case Lamp::ROLI: return 0x50;
  case Lamp::RORI: return 0x51;
  case Lamp::CAS: return 0x3F;
  case Lamp::XADD: return 0x40;
  case Lamp::XCHG: return 0x41;
  case Lamp::LDAR: return 0x42;
  case Lamp::STLR: return 0x43;
  case Lamp::FENCE: return 0x44;
  case Lamp::PAUSE: return 0x45;
  case Lamp::STARTAP: return 0x46;
  case Lamp::IPI: return 0x47;
  case Lamp::CPUID: return 0x48;
  default:
    Ctx.reportError(SMLoc(), "LampMCCodeEmitter: unknown opcode");
    return 0;
  }
}

LampInstForm LampMCCodeEmitter::getInstForm(unsigned Opc) const {
  switch (Opc) {
  case Lamp::HALT:
  case Lamp::RET:
  case Lamp::IRET:
  case Lamp::FENCE:
  case Lamp::PAUSE:
    return LampInstForm::None;

  case Lamp::JMP:
  case Lamp::JG:
  case Lamp::JGE:
  case Lamp::JL:
  case Lamp::JLE:
  case Lamp::JC:
  case Lamp::JNC:
  case Lamp::RJMP:
  case Lamp::RJG:
  case Lamp::RJGE:
  case Lamp::RJL:
  case Lamp::RJLE:
  case Lamp::RJC:
  case Lamp::RJNC:
    return LampInstForm::Target;

  case Lamp::JZ:
  case Lamp::JNZ:
  case Lamp::RJZ:
  case Lamp::RJNZ:
    return LampInstForm::RsTarget;

  case Lamp::INC:
  case Lamp::PUSH:
  case Lamp::POP:
  case Lamp::INT:
  case Lamp::CPUID:
    return LampInstForm::Rd;

  case Lamp::INTI:
    return LampInstForm::I;

  case Lamp::MOVI:
    return LampInstForm::RdImm;

  case Lamp::MOV:
  case Lamp::NOT:
  case Lamp::IN:
  case Lamp::OUT:
  case Lamp::FNEG:
  case Lamp::FABS:
  case Lamp::FSQRT:
  case Lamp::FCMP:
  case Lamp::ITOF:


  case Lamp::FTOI:
    return LampInstForm::RdRs;

  case Lamp::ADD:
  case Lamp::SUB:
  case Lamp::MUL:
  case Lamp::DIV:
  case Lamp::MOD:
  case Lamp::AND:
  case Lamp::OR:
  case Lamp::XOR:
  case Lamp::SHL:
  case Lamp::SHR:
  case Lamp::SAR:
  case Lamp::ROL:
  case Lamp::ROR:
  case Lamp::FADD:
  case Lamp::FSUB:
  case Lamp::FMUL:
  case Lamp::FDIV:
    return LampInstForm::RdRsRs;

  case Lamp::CMP:
    return LampInstForm::RsRs;

  case Lamp::LOAD:
  case Lamp::LOAD16:
  case Lamp::LOAD32:
  case Lamp::LOADS8:
  case Lamp::LOADS16:
  case Lamp::FLOAD32:
  case Lamp::ADDI:
  case Lamp::SUBI:
  case Lamp::ANDI:
  case Lamp::ORI:
  case Lamp::XORI:
  case Lamp::SHLI:
  case Lamp::SHRI:
  case Lamp::ROLI:
  case Lamp::RORI:
  case Lamp::LDAR:
    return LampInstForm::RdRsImm;

  case Lamp::CMPI:
    return LampInstForm::RsImm;

  case Lamp::STORE:
  case Lamp::STORE16:
  case Lamp::STORE32:
  case Lamp::MEMSET:
  case Lamp::MEMCPY:
  case Lamp::FSTORE32:
  case Lamp::STLR:
  case Lamp::STARTAP:
    return LampInstForm::RsRsImm;

  case Lamp::LOADX32:
  case Lamp::STOREX32:
  case Lamp::XADD:
  case Lamp::XCHG:
    return LampInstForm::RdRsRsImm;

  case Lamp::CAS:
    return LampInstForm::RdRsRsRsImm;

  case Lamp::CALL:
  case Lamp::RCALL:
    return LampInstForm::Call;
  case Lamp::CALLR:
    return LampInstForm::Rd;
  case Lamp::IPI:
    return LampInstForm::RsRs;

  default:
    Ctx.reportError(SMLoc(), "LampMCCodeEmitter: unknown instruction form");
    return LampInstForm::None;
  }
}

bool LampMCCodeEmitter::isPCRelativeTargetOpcode(unsigned Opc) const {
  switch (Opc) {
  case Lamp::RJMP:
  case Lamp::RCALL:
  case Lamp::RJZ:
  case Lamp::RJNZ:
  case Lamp::RJG:
  case Lamp::RJGE:
  case Lamp::RJL:
  case Lamp::RJLE:
  case Lamp::RJC:
  case Lamp::RJNC:
    return true;
  default:
    return false;
  }
}

uint8_t LampMCCodeEmitter::encodeReg(const MCOperand &Op) const {
  if (!Op.isReg()) {
    Ctx.reportError(SMLoc(), "LampMCCodeEmitter: expected register operand");
    return 0;
  }
  const MCRegisterInfo *MRI = Ctx.getRegisterInfo();
  if (!MRI) {
    Ctx.reportError(SMLoc(), "LampMCCodeEmitter: missing register info");
    return 0;
  }
  return static_cast<uint8_t>(MRI->getEncodingValue(Op.getReg()));
}

uint32_t LampMCCodeEmitter::encodeImm(const MCOperand &Op,
                                      SmallVectorImpl<MCFixup> &Fixups,
                                      MCFixupKind Kind, bool IsPCRel) const {
  if (Op.isImm()) {
    const int64_t Val = Op.getImm();
    if (!isInt<32>(Val) && !isUInt<32>(Val)) {
      Ctx.reportError(SMLoc(),
                      "LampMCCodeEmitter: immediate out of 32-bit range");
      return 0;
    }
    return static_cast<uint32_t>(Val);
  }
  if (Op.isExpr()) {
    Fixups.push_back(MCFixup::create(0, Op.getExpr(), Kind, IsPCRel));
    return 0;
  }
  Ctx.reportError(SMLoc(),
                  "LampMCCodeEmitter: expected immediate/expression operand");
  return 0;
}

void LampMCCodeEmitter::encodeInstruction(const MCInst &MI,
                                          SmallVectorImpl<char> &CB,
                                          SmallVectorImpl<MCFixup> &Fixups,
                                          const MCSubtargetInfo &STI) const {
  (void)STI;

  const uint8_t opcode = getOpcodeByte(MI.getOpcode());
  uint8_t rd = 0;
  uint8_t rs1 = 0;
  uint8_t rs2 = 0;
  uint32_t imm = 0;

  const unsigned NumOps = MI.getNumOperands();
  bool HadError = false;

  auto dumpMI = [&]() {
    errs() << "LampMCCodeEmitter: opcode=" << MI.getOpcode()
           << " numops=" << NumOps << '\n';
    for (unsigned I = 0; I < NumOps; ++I) {
      const MCOperand &Op = MI.getOperand(I);
      errs() << "  op" << I << ": ";
      if (Op.isReg())
        errs() << "reg " << Op.getReg();
      else if (Op.isImm())
        errs() << "imm " << Op.getImm();
      else if (Op.isExpr())
        errs() << "expr";
      else
        errs() << "other";
      errs() << '\n';
    }
  };

  auto encodeRegOperand = [&](unsigned I) -> uint8_t {
    if (I >= NumOps) {
      dumpMI();
      Ctx.reportError(SMLoc(), "LampMCCodeEmitter: missing register operand");
      HadError = true;
      return 0;
    }
    const MCOperand &Op = MI.getOperand(I);
    if (!Op.isReg()) {
      dumpMI();
      Ctx.reportError(SMLoc(), "LampMCCodeEmitter: expected register operand");
      HadError = true;
      return 0;
    }
    return encodeReg(Op);
  };

  switch (getInstForm(MI.getOpcode())) {
  case LampInstForm::None:
    break;
  case LampInstForm::I:
    if (NumOps < 1) {
      Ctx.reportError(SMLoc(), "LampMCCodeEmitter: missing immediate operand");
      return;
    }
    imm = encodeImm(MI.getOperand(0), Fixups);
    break;
  case LampInstForm::Target:
  case LampInstForm::Call:
    if (NumOps < 1) {
      Ctx.reportError(SMLoc(), "LampMCCodeEmitter: missing immediate operand");
      return;
    }
    imm = encodeImm(MI.getOperand(0),
                    Fixups,
                    isPCRelativeTargetOpcode(MI.getOpcode()) ? Lamp::fixup_lamp_pc32 : Lamp::fixup_lamp_32,
                    isPCRelativeTargetOpcode(MI.getOpcode()));
    break;
  case LampInstForm::RsTarget:
    if (NumOps < 2) {
      Ctx.reportError(SMLoc(), "LampMCCodeEmitter: missing rs,target operands");
      return;
    }
    rd = encodeRegOperand(0);
    imm = encodeImm(MI.getOperand(1),
                    Fixups,
                    isPCRelativeTargetOpcode(MI.getOpcode()) ? Lamp::fixup_lamp_pc32 : Lamp::fixup_lamp_32,
                    isPCRelativeTargetOpcode(MI.getOpcode()));
    break;
  case LampInstForm::Rd:
    if (NumOps < 1) {
      Ctx.reportError(SMLoc(), "LampMCCodeEmitter: missing rd operand");
      return;
    }
    rd = encodeRegOperand(0);
    break;
  case LampInstForm::RdImm:
    if (NumOps < 2) {
      Ctx.reportError(SMLoc(), "LampMCCodeEmitter: missing rd,imm operands");
      return;
    }
    rd = encodeRegOperand(0);
    imm = encodeImm(MI.getOperand(1), Fixups);
    break;
  case LampInstForm::RdRs:
    if (NumOps < 1) {
      Ctx.reportError(SMLoc(), "LampMCCodeEmitter: missing rd operand");
      return;
    }
    rd = encodeRegOperand(0);
    if (NumOps >= 2 && MI.getOperand(1).isReg())
      rs1 = encodeRegOperand(1);
    else
      rs1 = rd;
    break;
  case LampInstForm::RdRsRs:
    if (NumOps < 3) {
      Ctx.reportError(SMLoc(),
                      "LampMCCodeEmitter: missing rd,rs1,rs2 operands");
      return;
    }
    rd = encodeRegOperand(0);
    rs1 = encodeRegOperand(1);
    rs2 = encodeRegOperand(2);
    break;
  case LampInstForm::RsRs:
    if (NumOps < 2) {
      Ctx.reportError(SMLoc(), "LampMCCodeEmitter: missing rs,rs1 operands");
      return;
    }
    rd = encodeRegOperand(0);
    rs1 = encodeRegOperand(1);
    break;
  case LampInstForm::RsImm:
    if (NumOps < 2) {
      Ctx.reportError(SMLoc(), "LampMCCodeEmitter: missing rs,imm operands");
      return;
    }
    rd = encodeRegOperand(0);
    imm = encodeImm(MI.getOperand(1), Fixups);
    break;
  case LampInstForm::RdRsImm:
    if (NumOps < 3) {
      Ctx.reportError(SMLoc(),
                      "LampMCCodeEmitter: missing rd,rs1,imm operands");
      return;
    }
    rd = encodeRegOperand(0);
    rs1 = encodeRegOperand(1);
    imm = encodeImm(MI.getOperand(2), Fixups);
    break;
  case LampInstForm::RsRsImm:
    if (NumOps < 3) {
      Ctx.reportError(SMLoc(),
                      "LampMCCodeEmitter: missing rs,rs1,imm operands");
      return;
    }
    rd = encodeRegOperand(0);
    rs1 = encodeRegOperand(1);
    imm = encodeImm(MI.getOperand(2), Fixups);
    break;
  case LampInstForm::RdRsRsImm:
    if (NumOps < 4) {
      Ctx.reportError(SMLoc(),
                      "LampMCCodeEmitter: missing rd,rs1,rs2,imm operands");
      return;
    }
    rd = encodeRegOperand(0);
    rs1 = encodeRegOperand(1);
    rs2 = encodeRegOperand(2);
    imm = encodeImm(MI.getOperand(3), Fixups);
    break;
  case LampInstForm::RdRsRsRsImm:
    if (NumOps < 4) {
      Ctx.reportError(SMLoc(),
                      "LampMCCodeEmitter: missing rd,rs1,rs2,imm operands");
      return;
    }
    rd = encodeRegOperand(0);
    rs1 = encodeRegOperand(1);
    rs2 = encodeRegOperand(2);
    imm = encodeImm(MI.getOperand(NumOps >= 5 ? 4 : 3), Fixups);
    break;
  }

  if (HadError)
    return;

  uint64_t Enc = (static_cast<uint64_t>(opcode) << 56) |
                 (static_cast<uint64_t>(rd) << 48) |
                 (static_cast<uint64_t>(rs1) << 40) |
                 (static_cast<uint64_t>(rs2) << 32) |
                 static_cast<uint64_t>(imm);

  support::endian::write<uint64_t>(CB, Enc, llvm::endianness::little);
}

} // namespace

MCCodeEmitter *llvm::createLampMCCodeEmitter(const MCInstrInfo &MCII,
                                             MCContext &Ctx) {
  (void)MCII;
  return new LampMCCodeEmitter(Ctx);
}

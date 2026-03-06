#include "MCTargetDesc/LampMCTargetDesc.h"
#include "TargetInfo/LAMPTargetInfo.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/Casting.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCParser/AsmLexer.h"
#include "llvm/MC/MCParser/MCAsmParser.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include <cctype>
#include <memory>

using namespace llvm;

namespace {

class LampOperand : public MCParsedAsmOperand {
public:
  enum KindTy { Token, Reg, Imm } Kind;

  SMLoc StartLoc, EndLoc;
  StringRef Tok;
  MCRegister RegNo = MCRegister();
  const MCExpr *Expr = nullptr;

  explicit LampOperand(KindTy K) : Kind(K) {}

  static std::unique_ptr<LampOperand> createToken(StringRef Tok, SMLoc Loc) {
    auto Op = std::make_unique<LampOperand>(Token);
    Op->Tok = Tok;
    Op->StartLoc = Loc;
    Op->EndLoc = Loc;
    return Op;
  }

  static std::unique_ptr<LampOperand> createReg(MCRegister RegNo, SMLoc S,
                                                 SMLoc E) {
    auto Op = std::make_unique<LampOperand>(KindTy::Reg);
    Op->RegNo = RegNo;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static std::unique_ptr<LampOperand> createImm(const MCExpr *E, SMLoc S,
                                                 SMLoc ELoc) {
    auto Op = std::make_unique<LampOperand>(Imm);
    Op->Expr = E;
    Op->StartLoc = S;
    Op->EndLoc = ELoc;
    return Op;
  }

  bool isToken() const override { return Kind == Token; }
  bool isReg() const override { return Kind == Reg; }
  bool isImm() const override { return Kind == Imm; }
  bool isMem() const override { return false; }

  SMLoc getStartLoc() const override { return StartLoc; }
  SMLoc getEndLoc() const override { return EndLoc; }

  MCRegister getReg() const override { return RegNo; }
  const MCExpr *getImm() const { return Expr; }
  StringRef getToken() const { return Tok; }

  void print(raw_ostream &OS, const MCAsmInfo &MAI) const override {
    if (isToken()) {
      OS << Tok;
      return;
    }
    if (isReg()) {
      OS << "<reg:" << RegNo.id() << ">";
      return;
    }
    MAI.printExpr(OS, *Expr);
  }
};

class LampAsmParser : public MCTargetAsmParser {
  MCAsmParser &Parser;

public:
  LampAsmParser(const MCSubtargetInfo &STI, MCAsmParser &Parser,
                const MCInstrInfo &MII, const MCTargetOptions &Options)
      : MCTargetAsmParser(Options, STI, MII), Parser(Parser) {}

  bool parseRegister(MCRegister &Reg, SMLoc &StartLoc,
                     SMLoc &EndLoc) override;
  ParseStatus tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                               SMLoc &EndLoc) override;

  bool parseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc, OperandVector &Operands) override;

  bool matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                               OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override;

  void convertToMapAndConstraints(unsigned Kind,
                                  const OperandVector &Operands) override {}

private:
  MCRegister matchRegName(StringRef Name) const;
  ParseStatus parseOperand(OperandVector &Operands);

  static bool addRegOp(MCInst &Inst, MCParsedAsmOperand &Op) {
    auto &LOp = static_cast<LampOperand &>(Op);
    if (!LOp.isReg())
      return false;
    Inst.addOperand(MCOperand::createReg(LOp.getReg()));
    return true;
  }

  static bool addImmOp(MCInst &Inst, MCParsedAsmOperand &Op) {
    auto &LOp = static_cast<LampOperand &>(Op);
    if (!LOp.isImm())
      return false;
    if (const auto *CE = dyn_cast<MCConstantExpr>(LOp.getImm()))
      Inst.addOperand(MCOperand::createImm(CE->getValue()));
    else
      Inst.addOperand(MCOperand::createExpr(LOp.getImm()));
    return true;
  }
};

MCRegister LampAsmParser::matchRegName(StringRef Name) const {
  if (Name.equals_insensitive("sp"))
    return Lamp::R30;

  if (Name.size() >= 2 && (Name[0] == 'r' || Name[0] == 'R')) {
    unsigned N = 0;
    for (char C : Name.drop_front()) {
      if (!std::isdigit(static_cast<unsigned char>(C)))
        return MCRegister();
      N = N * 10 + (C - '0');
    }
    switch (N) {
    case 0: return Lamp::R0;   case 1: return Lamp::R1;
    case 2: return Lamp::R2;   case 3: return Lamp::R3;
    case 4: return Lamp::R4;   case 5: return Lamp::R5;
    case 6: return Lamp::R6;   case 7: return Lamp::R7;
    case 8: return Lamp::R8;   case 9: return Lamp::R9;
    case 10: return Lamp::R10; case 11: return Lamp::R11;
    case 12: return Lamp::R12; case 13: return Lamp::R13;
    case 14: return Lamp::R14; case 15: return Lamp::R15;
    case 16: return Lamp::R16; case 17: return Lamp::R17;
    case 18: return Lamp::R18; case 19: return Lamp::R19;
    case 20: return Lamp::R20; case 21: return Lamp::R21;
    case 22: return Lamp::R22; case 23: return Lamp::R23;
    case 24: return Lamp::R24; case 25: return Lamp::R25;
    case 26: return Lamp::R26; case 27: return Lamp::R27;
    case 28: return Lamp::R28; case 29: return Lamp::R29;
    case 30: return Lamp::R30; case 31: return Lamp::R31;
    default: return MCRegister();
    }
  }
  return MCRegister();
}

bool LampAsmParser::parseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                  SMLoc &EndLoc) {
  if (Parser.getTok().isNot(AsmToken::Identifier))
    return true;
  StartLoc = Parser.getTok().getLoc();
  Reg = matchRegName(Parser.getTok().getString());
  if (!Reg)
    return true;
  EndLoc = Parser.getTok().getEndLoc();
  Parser.Lex();
  return false;
}

ParseStatus LampAsmParser::tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                            SMLoc &EndLoc) {
  if (parseRegister(Reg, StartLoc, EndLoc))
    return ParseStatus::NoMatch;
  return ParseStatus::Success;
}

ParseStatus LampAsmParser::parseOperand(OperandVector &Operands) {
  SMLoc S = Parser.getTok().getLoc();
  if (Parser.getTok().is(AsmToken::LBrac)) {
    Parser.Lex(); // '['
    MCRegister R;
    SMLoc RS, RE;
    if (parseRegister(R, RS, RE))
      return ParseStatus::Failure;
    if (Parser.getTok().isNot(AsmToken::RBrac)) {
      Parser.Error(Parser.getTok().getLoc(), "expected ']'");
      return ParseStatus::Failure;
    }
    SMLoc End = Parser.getTok().getEndLoc();
    Parser.Lex(); // ']'
    Operands.push_back(LampOperand::createReg(R, S, End));
    return ParseStatus::Success;
  }

  if (Parser.getTok().is(AsmToken::Identifier)) {
    if (MCRegister R = matchRegName(Parser.getTok().getString())) {
      SMLoc E = Parser.getTok().getEndLoc();
      Parser.Lex();
      Operands.push_back(LampOperand::createReg(R, S, E));
      return ParseStatus::Success;
    }
  }

  const MCExpr *E = nullptr;
  if (Parser.parseExpression(E))
    return ParseStatus::Failure;
  Operands.push_back(LampOperand::createImm(E, S, Parser.getTok().getLoc()));
  return ParseStatus::Success;
}

bool LampAsmParser::parseInstruction(ParseInstructionInfo &, StringRef Name,
                                     SMLoc NameLoc,
                                     OperandVector &Operands) {
  Operands.push_back(LampOperand::createToken(Name, NameLoc));

  while (Parser.getTok().isNot(AsmToken::EndOfStatement)) {
    if (Parser.getTok().is(AsmToken::Comma)) {
      Parser.Lex();
      continue;
    }
    if (!parseOperand(Operands).isSuccess())
      return true;
    if (Parser.getTok().is(AsmToken::Comma))
      Parser.Lex();
  }
  Parser.Lex();
  return false;
}

bool LampAsmParser::matchAndEmitInstruction(SMLoc IDLoc, unsigned &,
                                            OperandVector &Operands,
                                            MCStreamer &Out,
                                            uint64_t &,
                                            bool) {
  if (Operands.empty())
    return Parser.Error(IDLoc, "missing instruction");

  auto *TokOp = static_cast<LampOperand *>(Operands[0].get());
  if (!TokOp->isToken())
    return Parser.Error(IDLoc, "invalid instruction token");

  std::string MnStorage = TokOp->getToken().lower();
  StringRef Mn(MnStorage);
  MCInst Inst;

  auto need = [&](unsigned N) -> bool {
    if (Operands.size() != N + 1) {
      Parser.Error(IDLoc, "wrong number of operands");
      return false;
    }
    return true;
  };

  auto rr = [&](unsigned Opc) -> bool {
    if (!need(2)) return false;
    Inst.setOpcode(Opc);
    return addRegOp(Inst, *Operands[1]) && addRegOp(Inst, *Operands[2]);
  };
  auto rrr = [&](unsigned Opc) -> bool {
    if (!need(3)) return false;
    Inst.setOpcode(Opc);
    return addRegOp(Inst, *Operands[1]) && addRegOp(Inst, *Operands[2]) &&
           addRegOp(Inst, *Operands[3]);
  };
  auto r = [&](unsigned Opc) -> bool {
    if (!need(1)) return false;
    Inst.setOpcode(Opc);
    return addRegOp(Inst, *Operands[1]);
  };
  auto ri = [&](unsigned Opc) -> bool {
    if (!need(2)) return false;
    Inst.setOpcode(Opc);
    return addRegOp(Inst, *Operands[1]) && addImmOp(Inst, *Operands[2]);
  };
  auto rri = [&](unsigned Opc) -> bool {
    if (!need(3)) return false;
    Inst.setOpcode(Opc);
    return addRegOp(Inst, *Operands[1]) && addRegOp(Inst, *Operands[2]) &&
           addImmOp(Inst, *Operands[3]);
  };
  auto rrri = [&](unsigned Opc) -> bool {
    if (!need(4)) return false;
    Inst.setOpcode(Opc);
    return addRegOp(Inst, *Operands[1]) && addRegOp(Inst, *Operands[2]) &&
           addRegOp(Inst, *Operands[3]) && addImmOp(Inst, *Operands[4]);
  };
  auto rrrri = [&](unsigned Opc) -> bool {
    if (!need(5)) return false;
    Inst.setOpcode(Opc);
    return addRegOp(Inst, *Operands[1]) && addRegOp(Inst, *Operands[2]) &&
           addRegOp(Inst, *Operands[3]) && addRegOp(Inst, *Operands[4]) &&
           addImmOp(Inst, *Operands[5]);
  };
  auto t = [&](unsigned Opc) -> bool {
    if (!need(1)) return false;
    Inst.setOpcode(Opc);
    return addImmOp(Inst, *Operands[1]);
  };
  auto rt = [&](unsigned Opc) -> bool {
    if (!need(2)) return false;
    Inst.setOpcode(Opc);
    return addRegOp(Inst, *Operands[1]) && addImmOp(Inst, *Operands[2]);
  };

  bool OK = true;
  if (Mn == "halt") {
    if (!need(0)) return true;
    Inst.setOpcode(Lamp::HALT);
  } else if (Mn == "iret") {
    if (!need(0)) return true;
    Inst.setOpcode(Lamp::IRET);
  } else if (Mn == "ret") {
    if (!need(0)) return true;
    Inst.setOpcode(Lamp::RET);
  } else if (Mn == "fence") {
    if (!need(0)) return true;
    Inst.setOpcode(Lamp::FENCE);
  } else if (Mn == "pause") {
    if (!need(0)) return true;
    Inst.setOpcode(Lamp::PAUSE);
  } else if (Mn == "jmp") {
    OK = t(Lamp::JMP);
  } else if (Mn == "rjmp") {
    OK = t(Lamp::RJMP);
  } else if (Mn == "call") {
    OK = t(Lamp::CALL);
  } else if (Mn == "rcall") {
    OK = t(Lamp::RCALL);
  } else if (Mn == "callr") {
    OK = r(Lamp::CALLR);
  } else if (Mn == "jz") {
    OK = rt(Lamp::JZ);
  } else if (Mn == "rjz") {
    OK = rt(Lamp::RJZ);
  } else if (Mn == "jnz") {
    OK = rt(Lamp::JNZ);
  } else if (Mn == "rjnz") {
    OK = rt(Lamp::RJNZ);
  } else if (Mn == "rjg") {
    OK = t(Lamp::RJG);
  } else if (Mn == "rjge") {
    OK = t(Lamp::RJGE);
  } else if (Mn == "rjl") {
    OK = t(Lamp::RJL);
  } else if (Mn == "rjle") {
    OK = t(Lamp::RJLE);
  } else if (Mn == "rjc") {
    OK = t(Lamp::RJC);
  } else if (Mn == "rjnc") {
    OK = t(Lamp::RJNC);
  } else if (Mn == "jg") {
    OK = t(Lamp::JG);
  } else if (Mn == "jge") {
    OK = t(Lamp::JGE);
  } else if (Mn == "jl") {
    OK = t(Lamp::JL);
  } else if (Mn == "jle") {
    OK = t(Lamp::JLE);
  } else if (Mn == "jc") {
    OK = t(Lamp::JC);
  } else if (Mn == "jnc") {
    OK = t(Lamp::JNC);
  } else if (Mn == "movi") {
    OK = ri(Lamp::MOVI);
  } else if (Mn == "mov") {
    OK = rr(Lamp::MOV);
  } else if (Mn == "out") {
    OK = rr(Lamp::OUT);
  } else if (Mn == "in") {
    OK = rr(Lamp::IN);
  } else if (Mn == "push") {
    OK = r(Lamp::PUSH);
  } else if (Mn == "pop") {
    OK = r(Lamp::POP);
  } else if (Mn == "int") {
    OK = r(Lamp::INT);
  } else if (Mn == "inti") {
    OK = t(Lamp::INTI);
  } else if (Mn == "inc") {
    OK = r(Lamp::INC);
  } else if (Mn == "cpuid") {
    OK = r(Lamp::CPUID);
  } else if (Mn == "store32") {
    OK = rri(Lamp::STORE32);
  } else if (Mn == "store16") {
    OK = rri(Lamp::STORE16);
  } else if (Mn == "store") {
    OK = rri(Lamp::STORE);
  } else if (Mn == "storex32") {
    OK = rrri(Lamp::STOREX32);
  } else if (Mn == "load32") {
    OK = rri(Lamp::LOAD32);
  } else if (Mn == "load16") {
    OK = rri(Lamp::LOAD16);
  } else if (Mn == "loads8") {
    OK = rri(Lamp::LOADS8);
  } else if (Mn == "loads16") {
    OK = rri(Lamp::LOADS16);
  } else if (Mn == "load") {
    OK = rri(Lamp::LOAD);
  } else if (Mn == "loadx32") {
    OK = rrri(Lamp::LOADX32);
  } else if (Mn == "ldar") {
    OK = rri(Lamp::LDAR);
  } else if (Mn == "stlr") {
    OK = rri(Lamp::STLR);
  } else if (Mn == "xadd") {
    OK = rrri(Lamp::XADD);
  } else if (Mn == "xchg") {
    OK = rrri(Lamp::XCHG);
  } else if (Mn == "cas") {
    OK = rrrri(Lamp::CAS);
  } else if (Mn == "fload32") {
    OK = rri(Lamp::FLOAD32);
  } else if (Mn == "fstore32") {
    OK = rri(Lamp::FSTORE32);
  } else if (Mn == "memcpy") {
    OK = rri(Lamp::MEMCPY);
  } else if (Mn == "memset") {
    OK = rri(Lamp::MEMSET);
  } else if (Mn == "addi") {
    OK = rri(Lamp::ADDI);
  } else if (Mn == "subi") {
    OK = rri(Lamp::SUBI);
  } else if (Mn == "andi") {
    OK = rri(Lamp::ANDI);
  } else if (Mn == "ori") {
    OK = rri(Lamp::ORI);
  } else if (Mn == "xori") {
    OK = rri(Lamp::XORI);
  } else if (Mn == "shli") {
    OK = rri(Lamp::SHLI);
  } else if (Mn == "shri") {
    OK = rri(Lamp::SHRI);
  } else if (Mn == "roli") {
    OK = rri(Lamp::ROLI);
  } else if (Mn == "rori") {
    OK = rri(Lamp::RORI);
  } else if (Mn == "cmpi") {
    OK = ri(Lamp::CMPI);
  } else if (Mn == "cmp") {
    OK = rr(Lamp::CMP);
  } else if (Mn == "not") {
    OK = rr(Lamp::NOT);
  } else if (Mn == "or") {
    OK = rrr(Lamp::OR);
  } else if (Mn == "and") {
    OK = rrr(Lamp::AND);
  } else if (Mn == "xor") {
    OK = rrr(Lamp::XOR);
  } else if (Mn == "shl") {
    OK = rrr(Lamp::SHL);
  } else if (Mn == "shr") {
    OK = rrr(Lamp::SHR);
  } else if (Mn == "sar") {
    OK = rrr(Lamp::SAR);
  } else if (Mn == "rol") {
    OK = rrr(Lamp::ROL);
  } else if (Mn == "ror") {
    OK = rrr(Lamp::ROR);
  } else if (Mn == "sub") {
    OK = rrr(Lamp::SUB);
  } else if (Mn == "add") {
    OK = rrr(Lamp::ADD);
  } else if (Mn == "div") {
    OK = rrr(Lamp::DIV);
  } else if (Mn == "mod") {
    OK = rrr(Lamp::MOD);
  } else if (Mn == "mul") {
    OK = rrr(Lamp::MUL);
  } else if (Mn == "fadd") {
    OK = rrr(Lamp::FADD);
  } else if (Mn == "fsub") {
    OK = rrr(Lamp::FSUB);
  } else if (Mn == "fmul") {
    OK = rrr(Lamp::FMUL);
  } else if (Mn == "fdiv") {
    OK = rrr(Lamp::FDIV);
  } else if (Mn == "fneg") {
    OK = rr(Lamp::FNEG);
  } else if (Mn == "fabs") {
    OK = rr(Lamp::FABS);
  } else if (Mn == "fsqrt") {
    OK = rr(Lamp::FSQRT);
  } else if (Mn == "fcmp") {
    OK = rr(Lamp::FCMP);
  } else if (Mn == "itof") {
    OK = rr(Lamp::ITOF);
  } else if (Mn == "ftoi") {
    OK = rr(Lamp::FTOI);
  } else if (Mn == "startap") {
    OK = rri(Lamp::STARTAP);
  } else if (Mn == "ipi") {
    OK = rr(Lamp::IPI);
  } else {
    return Parser.Error(IDLoc, "unknown instruction '" + Mn + "'");
  }

  if (!OK)
    return Parser.Error(IDLoc, "invalid operand type");

  Out.emitInstruction(Inst, getSTI());
  return false;
}

} // namespace

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeLampAsmParser() {
  RegisterMCAsmParser<LampAsmParser> X(getTheLampTarget());
}

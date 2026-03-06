#include "Lamp.h"
#include "LampISelLowering.h"
#include "LampTargetMachine.h"
#include "MCTargetDesc/LampMCTargetDesc.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/InitializePasses.h"
#include "llvm/IR/DiagnosticInfo.h"
#include <functional>
#include <utility>

using namespace llvm;

#define DEBUG_TYPE "lamp-isel"
#define PASS_NAME "Lamp DAG->DAG Pattern Instruction Selection"

namespace {
struct BranchCondInfo {
  unsigned BrOpc = 0;
  bool SwapCompareOperands = false;
};

static ISD::CondCode getSwappedCondCode(ISD::CondCode CC) {
  switch (CC) {
  case ISD::SETEQ:
  case ISD::SETNE:
    return CC;
  case ISD::SETLT:
    return ISD::SETGT;
  case ISD::SETLE:
    return ISD::SETGE;
  case ISD::SETGT:
    return ISD::SETLT;
  case ISD::SETGE:
    return ISD::SETLE;
  case ISD::SETULT:
    return ISD::SETUGT;
  case ISD::SETULE:
    return ISD::SETUGE;
  case ISD::SETUGT:
    return ISD::SETULT;
  case ISD::SETUGE:
    return ISD::SETULE;
  default:
    return ISD::SETCC_INVALID;
  }
}

static bool isZeroBranchOpcode(unsigned Opc) {
  return Opc == Lamp::JZ || Opc == Lamp::JNZ ||
         Opc == Lamp::RJZ || Opc == Lamp::RJNZ;
}

static bool getBranchCondInfo(ISD::CondCode CC, BranchCondInfo &Info) {
  switch (CC) {
  case ISD::SETEQ:
    Info.BrOpc = Lamp::RJZ;
    return true;
  case ISD::SETNE:
    Info.BrOpc = Lamp::RJNZ;
    return true;
  case ISD::SETLT:
    Info.BrOpc = Lamp::RJL;
    return true;
  case ISD::SETLE:
    Info.BrOpc = Lamp::RJLE;
    return true;
  case ISD::SETGT:
    Info.BrOpc = Lamp::RJG;
    return true;
  case ISD::SETGE:
    Info.BrOpc = Lamp::RJGE;
    return true;
  case ISD::SETULT:
    Info.BrOpc = Lamp::RJC;
    return true;
  case ISD::SETUGE:
    Info.BrOpc = Lamp::RJNC;
    return true;
  case ISD::SETUGT:
    Info.BrOpc = Lamp::RJC;
    Info.SwapCompareOperands = true;
    return true;
  case ISD::SETULE:
    Info.BrOpc = Lamp::RJNC;
    Info.SwapCompareOperands = true;
    return true;
  default:
    return false;
  }
}

class LampDAGToDAGISel : public SelectionDAGISel {
public:
  LampDAGToDAGISel() = delete;

  LampDAGToDAGISel(LampTargetMachine &TM, CodeGenOptLevel OptLevel)
      : SelectionDAGISel(TM, OptLevel) {}

  void Select(SDNode *N) override;
  bool SelectAddr(SDValue Addr, SDValue &Base, SDValue &Offset);
  bool SelectInlineAsmMemoryOperand(const SDValue &Op,
                                    InlineAsm::ConstraintCode ConstraintID,
                                    std::vector<SDValue> &OutOps) override;

#include "LampGenDAGISel.inc"
};

class LampDAGToDAGISelLegacy : public SelectionDAGISelLegacy {
public:
  static char ID;

  explicit LampDAGToDAGISelLegacy(LampTargetMachine &TM,
                                  CodeGenOptLevel OptLevel)
      : SelectionDAGISelLegacy(ID,
                               std::make_unique<LampDAGToDAGISel>(TM, OptLevel)) {}
};

} // namespace

char LampDAGToDAGISelLegacy::ID = 0;

INITIALIZE_PASS(LampDAGToDAGISelLegacy, DEBUG_TYPE, PASS_NAME, false, false)

FunctionPass *llvm::createLampISelDag(LampTargetMachine &TM,
                                      CodeGenOptLevel OptLevel) {
  return new LampDAGToDAGISelLegacy(TM, OptLevel);
}

void LampDAGToDAGISel::Select(SDNode *N) {
  if (N->isMachineOpcode()) {
    N->setNodeId(-1);
    return;
  }

  if (N->getOpcode() == ISD::FrameIndex) {
    auto *FIN = cast<FrameIndexSDNode>(N);
    SDValue TFI = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i32);
    SDValue Zero = CurDAG->getTargetConstant(0, SDLoc(N), MVT::i32);
    ReplaceNode(N,
                CurDAG->getMachineNode(Lamp::ADDI, SDLoc(N), MVT::i32, TFI, Zero));
    return;
  }

  // These nodes are selection-time value-range hints and should not reach MI.
  // Treat them as transparent passthrough values.
  if (N->getOpcode() == ISD::AssertZext || N->getOpcode() == ISD::AssertSext ||
      N->getOpcode() == ISD::ZERO_EXTEND || N->getOpcode() == ISD::ANY_EXTEND) {
    SDValue Src = N->getOperand(0);
    if (Src.getValueType() == MVT::i32) {
      ReplaceNode(N, Src.getNode());
      return;
    }
  }

  std::function<SDValue(SDValue, SDLoc)> materializeGPROp =
      [&](SDValue V, SDLoc DL) -> SDValue {
    auto makeZero = [&]() -> SDValue {
      SDNode *MovZero = CurDAG->getMachineNode(
          Lamp::MOVI, DL, MVT::i32,
          CurDAG->getSignedTargetConstant(0, DL, MVT::i32));
      return SDValue(MovZero, 0);
    };

    if (V.getOpcode() == ISD::UNDEF)
      return makeZero();

    if (auto *RS = dyn_cast<RegisterSDNode>(V))
      if (!RS->getReg())
        return makeZero();

    if (V.getOpcode() == ISD::CopyFromReg) {
      if (auto *RS = dyn_cast<RegisterSDNode>(V.getOperand(1)))
        if (!RS->getReg())
          return makeZero();
    }

    if (V.getOpcode() == ISD::TRUNCATE &&
        V.getOperand(0).getValueType() == MVT::i32)
      V = V.getOperand(0);

    if (V.getOpcode() == ISD::ZERO_EXTEND || V.getOpcode() == ISD::ANY_EXTEND ||
        V.getOpcode() == ISD::AssertZext || V.getOpcode() == ISD::AssertSext) {
      if (V.getValueType() == MVT::i32)
        return V;
      V = V.getOperand(0);
    }

    if (V.getOpcode() == ISD::SIGN_EXTEND ||
        V.getOpcode() == ISD::SIGN_EXTEND_INREG) {
      SDValue Src = materializeGPROp(V.getOperand(0), DL);
      if (Src.getValueType() != MVT::i32)
        Src = CurDAG->getZExtOrTrunc(Src, DL, MVT::i32);

      EVT ExVT = V.getOpcode() == ISD::SIGN_EXTEND_INREG
                     ? cast<VTSDNode>(V.getOperand(1))->getVT()
                     : V.getOperand(0).getValueType();
      unsigned Bits = ExVT.getScalarSizeInBits();
      if (Bits == 0 || Bits >= 32)
        return Src;

      uint64_t Mask = (uint64_t(1) << Bits) - 1;
      uint64_t Bias = uint64_t(1) << (Bits - 1);
      SDValue MaskVal = CurDAG->getTargetConstant(int64_t(Mask), DL, MVT::i32);
      SDValue BiasVal = CurDAG->getTargetConstant(int64_t(Bias), DL, MVT::i32);
      SDNode *And = CurDAG->getMachineNode(Lamp::ANDI, DL, MVT::i32, Src, MaskVal);
      SDNode *Xor =
          CurDAG->getMachineNode(Lamp::XORI, DL, MVT::i32, SDValue(And, 0), BiasVal);
      SDNode *Sub =
          CurDAG->getMachineNode(Lamp::SUBI, DL, MVT::i32, SDValue(Xor, 0), BiasVal);
      return SDValue(Sub, 0);
    }

    if (auto *LD = dyn_cast<LoadSDNode>(V)) {
      if (!LD->isIndexed()) {
        SDValue Base, Offset;
        if (SelectAddr(LD->getBasePtr(), Base, Offset)) {
          Base = materializeGPROp(Base, DL);

          if (LD->getMemoryVT() == MVT::i32 &&
              LD->getValueType(0) == MVT::i32) {
            SDValue Ops[] = {Base, Offset, LD->getChain()};
            SDNode *Load32 = CurDAG->getMachineNode(
                Lamp::LOAD32, DL, {MVT::i32, MVT::Other}, Ops);
            return SDValue(Load32, 0);
          }

          if ((LD->getExtensionType() == ISD::NON_EXTLOAD ||
               LD->getExtensionType() == ISD::ZEXTLOAD ||
               LD->getExtensionType() == ISD::EXTLOAD ||
               LD->getExtensionType() == ISD::SEXTLOAD) &&
              LD->getMemoryVT() == MVT::i8 &&
              LD->getValueType(0) == MVT::i32) {
            SDValue Ops[] = {Base, Offset, LD->getChain()};
            SDNode *Load8 = CurDAG->getMachineNode(
                Lamp::LOAD, DL, {MVT::i32, MVT::Other}, Ops);
            if (LD->getExtensionType() == ISD::SEXTLOAD) {
              SDValue Bias = CurDAG->getTargetConstant(0x80, DL, MVT::i32);
              SDNode *Xor = CurDAG->getMachineNode(
                  Lamp::XORI, DL, MVT::i32, SDValue(Load8, 0), Bias);
              SDNode *Sub = CurDAG->getMachineNode(
                  Lamp::SUBI, DL, MVT::i32, SDValue(Xor, 0), Bias);
              return SDValue(Sub, 0);
            }
            // Keep byte loads deterministic in i32 regs.
            SDValue Mask = CurDAG->getTargetConstant(0xff, DL, MVT::i32);
            SDNode *Masked = CurDAG->getMachineNode(
                Lamp::ANDI, DL, MVT::i32, SDValue(Load8, 0), Mask);
            return SDValue(Masked, 0);
          }

          if ((LD->getExtensionType() == ISD::ZEXTLOAD ||
               LD->getExtensionType() == ISD::EXTLOAD ||
               LD->getExtensionType() == ISD::SEXTLOAD) &&
              LD->getMemoryVT() == MVT::i16 &&
              LD->getValueType(0) == MVT::i32) {
            int64_t Off = 0;
            if (auto *CN = dyn_cast<ConstantSDNode>(Offset))
              Off = CN->getSExtValue();

            SDValue LowOps[] = {Base, Offset, LD->getChain()};
            SDNode *LowLoad = CurDAG->getMachineNode(
                Lamp::LOAD, DL, {MVT::i32, MVT::Other}, LowOps);

            SDValue Offset1 =
                CurDAG->getSignedTargetConstant(Off + 1, DL, MVT::i32);
            SDValue HighOps[] = {Base, Offset1, SDValue(LowLoad, 1)};
            SDNode *HighLoad = CurDAG->getMachineNode(
                Lamp::LOAD, DL, {MVT::i32, MVT::Other}, HighOps);

            SDValue ShiftImm = CurDAG->getTargetConstant(8, DL, MVT::i32);
            SDNode *HighShift = CurDAG->getMachineNode(
                Lamp::SHLI, DL, MVT::i32, SDValue(HighLoad, 0), ShiftImm);
            SDNode *Merged = CurDAG->getMachineNode(
                Lamp::OR, DL, MVT::i32, SDValue(LowLoad, 0),
                SDValue(HighShift, 0));
            if (LD->getExtensionType() == ISD::SEXTLOAD) {
              SDValue Bias = CurDAG->getTargetConstant(0x8000, DL, MVT::i32);
              SDNode *Xor = CurDAG->getMachineNode(
                  Lamp::XORI, DL, MVT::i32, SDValue(Merged, 0), Bias);
              SDNode *Sub = CurDAG->getMachineNode(
                  Lamp::SUBI, DL, MVT::i32, SDValue(Xor, 0), Bias);
              return SDValue(Sub, 0);
            }
            return SDValue(Merged, 0);
          }
        }
      }
    }

    if (V.getOpcode() == ISD::ADD) {
      SDValue L = materializeGPROp(V.getOperand(0), DL);
      SDValue R = V.getOperand(1);
      if (auto *CN = dyn_cast<ConstantSDNode>(R)) {
        SDValue Imm = CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL,
                                                      MVT::i32);
        SDNode *AddI =
            CurDAG->getMachineNode(Lamp::ADDI, DL, MVT::i32, L, Imm);
        return SDValue(AddI, 0);
      }
      R = materializeGPROp(R, DL);
      SDNode *Add = CurDAG->getMachineNode(Lamp::ADD, DL, MVT::i32, L, R);
      return SDValue(Add, 0);
    }

    if (V.getOpcode() == ISD::SUB) {
      SDValue L = materializeGPROp(V.getOperand(0), DL);
      SDValue R = V.getOperand(1);
      if (auto *CN = dyn_cast<ConstantSDNode>(R)) {
        SDValue Imm = CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL,
                                                      MVT::i32);
        SDNode *SubI =
            CurDAG->getMachineNode(Lamp::SUBI, DL, MVT::i32, L, Imm);
        return SDValue(SubI, 0);
      }
      R = materializeGPROp(R, DL);
      SDNode *Sub = CurDAG->getMachineNode(Lamp::SUB, DL, MVT::i32, L, R);
      return SDValue(Sub, 0);
    }

    if (V.getOpcode() == ISD::MUL) {
      SDValue L = materializeGPROp(V.getOperand(0), DL);
      SDValue R = materializeGPROp(V.getOperand(1), DL);
      SDNode *Mul = CurDAG->getMachineNode(Lamp::MUL, DL, MVT::i32, L, R);
      return SDValue(Mul, 0);
    }

    if (V.getOpcode() == ISD::AND || V.getOpcode() == ISD::OR ||
        V.getOpcode() == ISD::XOR) {
      SDValue L = materializeGPROp(V.getOperand(0), DL);
      SDValue R = V.getOperand(1);
      if (auto *CN = dyn_cast<ConstantSDNode>(R)) {
        SDValue Imm = CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL,
                                                      MVT::i32);
        unsigned Opc = V.getOpcode() == ISD::AND
                           ? Lamp::ANDI
                           : (V.getOpcode() == ISD::OR ? Lamp::ORI : Lamp::XORI);
        SDNode *N = CurDAG->getMachineNode(Opc, DL, MVT::i32, L, Imm);
        return SDValue(N, 0);
      }
      R = materializeGPROp(R, DL);
      unsigned Opc = V.getOpcode() == ISD::AND
                         ? Lamp::AND
                         : (V.getOpcode() == ISD::OR ? Lamp::OR : Lamp::XOR);
      SDNode *N = CurDAG->getMachineNode(Opc, DL, MVT::i32, L, R);
      return SDValue(N, 0);
    }

    if ((V.getOpcode() == ISD::SHL || V.getOpcode() == ISD::SRL ||
         V.getOpcode() == ISD::SRA)) {
      SDValue L = materializeGPROp(V.getOperand(0), DL);
      SDValue R = V.getOperand(1);
      if (auto *CN = dyn_cast<ConstantSDNode>(R)) {
        if (V.getOpcode() == ISD::SRA) {
          SDValue Imm =
              CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL, MVT::i32);
          SDNode *ImmReg = CurDAG->getMachineNode(Lamp::MOVI, DL, MVT::i32, Imm);
          SDNode *N = CurDAG->getMachineNode(
              Lamp::SAR, DL, MVT::i32, L, SDValue(ImmReg, 0));
          return SDValue(N, 0);
        }

        SDValue Imm =
            CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL, MVT::i32);
        unsigned Opc = V.getOpcode() == ISD::SHL ? Lamp::SHLI : Lamp::SHRI;
        SDNode *N = CurDAG->getMachineNode(Opc, DL, MVT::i32, L, Imm);
        return SDValue(N, 0);
      }
      R = materializeGPROp(R, DL);
      unsigned Opc = V.getOpcode() == ISD::SHL
                         ? Lamp::SHL
                         : (V.getOpcode() == ISD::SRL ? Lamp::SHR : Lamp::SAR);
      SDNode *N = CurDAG->getMachineNode(Opc, DL, MVT::i32, L, R);
      return SDValue(N, 0);
    }

    if (auto *CN = dyn_cast<ConstantSDNode>(V)) {
      SDValue Imm =
          CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL, MVT::i32);
      SDNode *Mov = CurDAG->getMachineNode(Lamp::MOVI, DL, MVT::i32, Imm);
      return SDValue(Mov, 0);
    }

    if (isa<GlobalAddressSDNode>(V) || isa<ExternalSymbolSDNode>(V) ||
        V.getOpcode() == ISD::TargetGlobalAddress ||
        V.getOpcode() == ISD::TargetExternalSymbol) {
      SDValue Sym;
      if (auto *GA = dyn_cast<GlobalAddressSDNode>(V))
        Sym = CurDAG->getTargetGlobalAddress(GA->getGlobal(), DL, MVT::i32,
                                             GA->getOffset(),
                                             GA->getTargetFlags());
      else if (isa<ExternalSymbolSDNode>(V))
        Sym = CurDAG->getTargetExternalSymbol(
            cast<ExternalSymbolSDNode>(V)->getSymbol(), MVT::i32);
      else
        Sym = V;

      SDNode *Mov = CurDAG->getMachineNode(Lamp::MOVI, DL, MVT::i32, Sym);
      return SDValue(Mov, 0);
    }

    if (V.getValueType() != MVT::i32)
      return CurDAG->getNode(ISD::ZERO_EXTEND, DL, MVT::i32, V);

    return V;
  };

  if (N->getOpcode() == ISD::SIGN_EXTEND ||
      N->getOpcode() == ISD::SIGN_EXTEND_INREG) {
    SDLoc DL(N);
    SDValue Res = materializeGPROp(SDValue(N, 0), DL);
    ReplaceNode(N, Res.getNode());
    return;
  }

  if (N->getOpcode() == ISD::SHL) {
    SDLoc DL(N);
    SDValue LHS = materializeGPROp(N->getOperand(0), DL);
    SDValue RHS = N->getOperand(1);
    if (auto *CN = dyn_cast<ConstantSDNode>(RHS)) {
      SDValue Imm =
          CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL, MVT::i32);
      CurDAG->SelectNodeTo(N, Lamp::SHLI, MVT::i32, LHS, Imm);
    } else {
      RHS = materializeGPROp(RHS, DL);
      CurDAG->SelectNodeTo(N, Lamp::SHL, MVT::i32, LHS, RHS);
    }
    return;
  }

  if (N->getOpcode() == ISD::SRL) {
    SDLoc DL(N);
    SDValue LHS = materializeGPROp(N->getOperand(0), DL);
    SDValue RHS = N->getOperand(1);
    if (auto *CN = dyn_cast<ConstantSDNode>(RHS)) {
      SDValue Imm =
          CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL, MVT::i32);
      CurDAG->SelectNodeTo(N, Lamp::SHRI, MVT::i32, LHS, Imm);
    } else {
      RHS = materializeGPROp(RHS, DL);
      CurDAG->SelectNodeTo(N, Lamp::SHR, MVT::i32, LHS, RHS);
    }
    return;
  }

  if (N->getOpcode() == ISD::SRA) {
    SDLoc DL(N);
    SDValue LHS = materializeGPROp(N->getOperand(0), DL);
    SDValue RHS = materializeGPROp(N->getOperand(1), DL);
    CurDAG->SelectNodeTo(N, Lamp::SAR, MVT::i32, LHS, RHS);
    return;
  }

  if (N->getOpcode() == ISD::GlobalAddress) {
    auto *GA = cast<GlobalAddressSDNode>(N);
    if (TM.isPositionIndependent()) {
      CurDAG->getContext()->diagnose(DiagnosticInfoUnsupported(
          CurDAG->getMachineFunction().getFunction(),
          "PIC relocations are not supported on the Lamp target",
          SDLoc(N).getDebugLoc()));
    }
    if (GA->getGlobal()->isThreadLocal() && !TM.useEmulatedTLS()) {
      CurDAG->getContext()->diagnose(DiagnosticInfoUnsupported(
          CurDAG->getMachineFunction().getFunction(),
          "TLS is not supported on the Lamp target", SDLoc(N).getDebugLoc()));
    }
    SDValue TGA = CurDAG->getTargetGlobalAddress(GA->getGlobal(), SDLoc(N),
                                                 MVT::i32, GA->getOffset(),
                                                 GA->getTargetFlags());
    CurDAG->SelectNodeTo(N, Lamp::MOVI, MVT::i32, TGA);
    return;
  }

  if (N->getOpcode() == ISD::ExternalSymbol) {
    auto *ES = cast<ExternalSymbolSDNode>(N);
    if (TM.isPositionIndependent()) {
      CurDAG->getContext()->diagnose(DiagnosticInfoUnsupported(
          CurDAG->getMachineFunction().getFunction(),
          "PIC relocations are not supported on the Lamp target",
          SDLoc(N).getDebugLoc()));
    }
    SDValue TES =
        CurDAG->getTargetExternalSymbol(ES->getSymbol(), MVT::i32);
    CurDAG->SelectNodeTo(N, Lamp::MOVI, MVT::i32, TES);
    return;
  }

  if (N->getOpcode() == ISD::ADD) {
    SDLoc DL(N);
    SDValue LHS = materializeGPROp(N->getOperand(0), DL);
    SDValue RHS = N->getOperand(1);
    if (auto *CN = dyn_cast<ConstantSDNode>(RHS)) {
      SDValue Imm =
          CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL, MVT::i32);
      CurDAG->SelectNodeTo(N, Lamp::ADDI, MVT::i32, LHS, Imm);
    } else {
      RHS = materializeGPROp(RHS, DL);
      CurDAG->SelectNodeTo(N, Lamp::ADD, MVT::i32, LHS, RHS);
    }
    return;
  }

  if (N->getOpcode() == ISD::SUB) {
    SDLoc DL(N);
    SDValue LHS = materializeGPROp(N->getOperand(0), DL);
    SDValue RHS = N->getOperand(1);
    if (auto *CN = dyn_cast<ConstantSDNode>(RHS)) {
      SDValue Imm =
          CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL, MVT::i32);
      CurDAG->SelectNodeTo(N, Lamp::SUBI, MVT::i32, LHS, Imm);
    } else {
      RHS = materializeGPROp(RHS, DL);
      CurDAG->SelectNodeTo(N, Lamp::SUB, MVT::i32, LHS, RHS);
    }
    return;
  }

  if (N->getOpcode() == ISD::MUL) {
    SDLoc DL(N);
    SDValue LHS = materializeGPROp(N->getOperand(0), DL);
    SDValue RHS = materializeGPROp(N->getOperand(1), DL);
    CurDAG->SelectNodeTo(N, Lamp::MUL, MVT::i32, LHS, RHS);
    return;
  }

  if (N->getOpcode() == ISD::SETCC) {
    SDLoc DL(N);
    auto *CC = cast<CondCodeSDNode>(N->getOperand(2));
    ISD::CondCode CCCode = CC->get();

    SDValue LHS = materializeGPROp(N->getOperand(0), DL);
    SDValue RHS = materializeGPROp(N->getOperand(1), DL);
    if (LHS.getValueType() != MVT::i32)
      LHS = CurDAG->getZExtOrTrunc(LHS, DL, MVT::i32);
    if (RHS.getValueType() != MVT::i32)
      RHS = CurDAG->getZExtOrTrunc(RHS, DL, MVT::i32);

    auto emitSub = [&](SDValue A, SDValue B) -> SDValue {
      SDNode *Sub = CurDAG->getMachineNode(Lamp::SUB, DL, MVT::i32, A, B);
      return SDValue(Sub, 0);
    };
    auto emitXorImm = [&](SDValue V, int64_t Imm) -> SDValue {
      SDValue I = CurDAG->getSignedTargetConstant(Imm, DL, MVT::i32);
      SDNode *X = CurDAG->getMachineNode(Lamp::XORI, DL, MVT::i32, V, I);
      return SDValue(X, 0);
    };
    auto emitShr31 = [&](SDValue V) -> SDValue {
      SDValue ShiftImm = CurDAG->getTargetConstant(31, DL, MVT::i32);
      SDNode *S = CurDAG->getMachineNode(Lamp::SHRI, DL, MVT::i32, V, ShiftImm);
      return SDValue(S, 0);
    };
    auto emitInvert01 = [&](SDValue V) -> SDValue {
      return emitXorImm(V, 1);
    };

    std::function<SDValue(SDValue, SDValue)> emitUnsignedLT =
        [&](SDValue X, SDValue Y) -> SDValue {
      // ltu(x, y) = ((~x & y) | (~(x ^ y) & (x - y))) >> 31
      SDValue D = emitSub(X, Y);
      SDValue NX = emitXorImm(X, -1);
      SDNode *XYNode = CurDAG->getMachineNode(Lamp::XOR, DL, MVT::i32, X, Y);
      SDValue NXY = emitXorImm(SDValue(XYNode, 0), -1);
      SDNode *T1Node = CurDAG->getMachineNode(Lamp::AND, DL, MVT::i32, NX, Y);
      SDNode *T2Node = CurDAG->getMachineNode(Lamp::AND, DL, MVT::i32, NXY, D);
      SDNode *OrNode = CurDAG->getMachineNode(Lamp::OR, DL, MVT::i32,
                                              SDValue(T1Node, 0),
                                              SDValue(T2Node, 0));
      return emitShr31(SDValue(OrNode, 0));
    };

    std::function<SDValue(SDValue, SDValue)> emitSignedLT =
        [&](SDValue X, SDValue Y) -> SDValue {
      // slt(x, y) = ((d ^ ((x ^ y) & (x ^ d))) >> 31), d = x - y
      SDValue D = emitSub(X, Y);
      SDNode *XYNode = CurDAG->getMachineNode(Lamp::XOR, DL, MVT::i32, X, Y);
      SDNode *XDNode = CurDAG->getMachineNode(Lamp::XOR, DL, MVT::i32, X, D);
      SDNode *AndNode = CurDAG->getMachineNode(Lamp::AND, DL, MVT::i32,
                                               SDValue(XYNode, 0),
                                               SDValue(XDNode, 0));
      SDNode *XorNode = CurDAG->getMachineNode(Lamp::XOR, DL, MVT::i32, D,
                                               SDValue(AndNode, 0));
      return emitShr31(SDValue(XorNode, 0));
    };

    SDValue Res;
    switch (CCCode) {
    case ISD::SETEQ:
    case ISD::SETNE: {
      SDValue Diff = emitSub(LHS, RHS);
      SDValue ZeroImm = CurDAG->getTargetConstant(0, DL, MVT::i32);
      SDNode *Zero = CurDAG->getMachineNode(Lamp::MOVI, DL, MVT::i32, ZeroImm);
      SDNode *Neg = CurDAG->getMachineNode(Lamp::SUB, DL, MVT::i32,
                                           SDValue(Zero, 0), Diff);
      SDNode *Or = CurDAG->getMachineNode(Lamp::OR, DL, MVT::i32, Diff,
                                          SDValue(Neg, 0));
      Res = emitShr31(SDValue(Or, 0));
      if (CCCode == ISD::SETEQ)
        Res = emitInvert01(Res);
      break;
    }
    case ISD::SETULT:
      Res = emitUnsignedLT(LHS, RHS);
      break;
    case ISD::SETUGT:
      Res = emitUnsignedLT(RHS, LHS);
      break;
    case ISD::SETULE:
      Res = emitInvert01(emitUnsignedLT(RHS, LHS));
      break;
    case ISD::SETUGE:
      Res = emitInvert01(emitUnsignedLT(LHS, RHS));
      break;
    case ISD::SETLT:
      Res = emitSignedLT(LHS, RHS);
      break;
    case ISD::SETGT:
      Res = emitSignedLT(RHS, LHS);
      break;
    case ISD::SETLE:
      Res = emitInvert01(emitSignedLT(RHS, LHS));
      break;
    case ISD::SETGE:
      Res = emitInvert01(emitSignedLT(LHS, RHS));
      break;
    default:
      SelectCode(N);
      return;
    }
    ReplaceNode(N, Res.getNode());
    return;
  }

  if (N->getOpcode() == ISD::SELECT) {
    SDLoc DL(N);
    SDValue Cond = materializeGPROp(N->getOperand(0), DL);
    SDValue TrueV = materializeGPROp(N->getOperand(1), DL);
    SDValue FalseV = materializeGPROp(N->getOperand(2), DL);
    if (Cond.getValueType() != MVT::i32)
      Cond = CurDAG->getZExtOrTrunc(Cond, DL, MVT::i32);
    if (TrueV.getValueType() != MVT::i32)
      TrueV = CurDAG->getZExtOrTrunc(TrueV, DL, MVT::i32);
    if (FalseV.getValueType() != MVT::i32)
      FalseV = CurDAG->getZExtOrTrunc(FalseV, DL, MVT::i32);

    // cond is 0/1. Use: false ^ ((-cond) & (true ^ false))
    SDValue ZeroImm = CurDAG->getTargetConstant(0, DL, MVT::i32);
    SDNode *Zero = CurDAG->getMachineNode(Lamp::MOVI, DL, MVT::i32, ZeroImm);
    SDNode *NegCond =
        CurDAG->getMachineNode(Lamp::SUB, DL, MVT::i32, SDValue(Zero, 0), Cond);
    SDNode *XorTF =
        CurDAG->getMachineNode(Lamp::XOR, DL, MVT::i32, TrueV, FalseV);
    SDNode *Masked = CurDAG->getMachineNode(Lamp::AND, DL, MVT::i32,
                                            SDValue(NegCond, 0),
                                            SDValue(XorTF, 0));
    SDNode *Res = CurDAG->getMachineNode(Lamp::XOR, DL, MVT::i32, FalseV,
                                         SDValue(Masked, 0));
    ReplaceNode(N, Res);
    return;
  }

  if (N->getOpcode() == ISD::LOAD) {
    auto *LD = cast<LoadSDNode>(N);
    // Leave plain i16 loads to generic legalization/selection paths.
    // The custom byte-merge path here interacted badly with later zext/shl
    // uses and could introduce NOREG operands.
    if (!LD->isIndexed() &&
        LD->getMemoryVT() == MVT::i32 && LD->getValueType(0) == MVT::i32) {
      SDValue Base, Offset;
      if (SelectAddr(LD->getBasePtr(), Base, Offset)) {
        SDValue Ops[] = {Base, Offset, LD->getChain()};
        CurDAG->SelectNodeTo(N, Lamp::LOAD32, MVT::i32, MVT::Other, Ops);
        return;
      }
    }
    if (!LD->isIndexed() && LD->getExtensionType() == ISD::SEXTLOAD &&
        LD->getMemoryVT() == MVT::i8 && LD->getValueType(0) == MVT::i32) {
      SDValue Base, Offset;
      if (SelectAddr(LD->getBasePtr(), Base, Offset)) {
        SDValue Ops[] = {Base, Offset, LD->getChain()};
        CurDAG->SelectNodeTo(N, Lamp::LOADS8, MVT::i32, MVT::Other, Ops);
        return;
      }
    }
    if (!LD->isIndexed() &&
        (LD->getExtensionType() == ISD::NON_EXTLOAD ||
         LD->getExtensionType() == ISD::ZEXTLOAD ||
         LD->getExtensionType() == ISD::EXTLOAD) &&
        LD->getMemoryVT() == MVT::i8 && LD->getValueType(0) == MVT::i32) {
      SDValue Base, Offset;
      if (SelectAddr(LD->getBasePtr(), Base, Offset)) {
        SDValue Ops[] = {Base, Offset, LD->getChain()};
        CurDAG->SelectNodeTo(N, Lamp::LOAD, MVT::i32, MVT::Other, Ops);
        return;
      }
    }
    if (!LD->isIndexed() &&
        (LD->getExtensionType() == ISD::ZEXTLOAD ||
         LD->getExtensionType() == ISD::EXTLOAD ||
         LD->getExtensionType() == ISD::SEXTLOAD) &&
        LD->getMemoryVT() == MVT::i16 && LD->getValueType(0) == MVT::i32) {
      SDValue Base, Offset;
      if (SelectAddr(LD->getBasePtr(), Base, Offset)) {
        if (LD->getAlign().value() >= 2) {
          SDValue Ops[] = {Base, Offset, LD->getChain()};
          unsigned Opc = LD->getExtensionType() == ISD::SEXTLOAD
                             ? Lamp::LOADS16
                             : Lamp::LOAD16;
          CurDAG->SelectNodeTo(N, Opc, MVT::i32, MVT::Other, Ops);
          return;
        }

        SDLoc DL(N);
        int64_t Off = 0;
        if (auto *CN = dyn_cast<ConstantSDNode>(Offset))
          Off = CN->getSExtValue();

        SDValue LowOps[] = {Base, Offset, LD->getChain()};
        SDNode *LowLoad =
            CurDAG->getMachineNode(Lamp::LOAD, DL, {MVT::i32, MVT::Other},
                                   LowOps);

        SDValue Offset1 = CurDAG->getSignedTargetConstant(Off + 1, DL, MVT::i32);
        SDValue HighOps[] = {Base, Offset1, SDValue(LowLoad, 1)};
        SDNode *HighLoad =
            CurDAG->getMachineNode(Lamp::LOAD, DL, {MVT::i32, MVT::Other},
                                   HighOps);

        SDValue ShiftImm = CurDAG->getTargetConstant(8, DL, MVT::i32);
        SDNode *HighShift =
            CurDAG->getMachineNode(Lamp::SHLI, DL, MVT::i32, SDValue(HighLoad, 0),
                                   ShiftImm);
        SDNode *Merged = CurDAG->getMachineNode(Lamp::OR, DL, MVT::i32,
                                                SDValue(LowLoad, 0),
                                                SDValue(HighShift, 0));
        SDValue Val(SDValue(Merged, 0));
        if (LD->getExtensionType() == ISD::SEXTLOAD) {
          SDValue Bias = CurDAG->getTargetConstant(0x8000, DL, MVT::i32);
          SDNode *Xor = CurDAG->getMachineNode(
              Lamp::XORI, DL, MVT::i32, Val, Bias);
          SDNode *Sub = CurDAG->getMachineNode(
              Lamp::SUBI, DL, MVT::i32, SDValue(Xor, 0), Bias);
          Val = SDValue(Sub, 0);
        }
        CurDAG->ReplaceAllUsesOfValueWith(SDValue(N, 0), Val);
        CurDAG->ReplaceAllUsesOfValueWith(SDValue(N, 1), SDValue(HighLoad, 1));
        CurDAG->RemoveDeadNode(N);
        return;
      }
    }
  }

  if (N->getOpcode() == ISD::STORE) {
    auto *ST = cast<StoreSDNode>(N);
    auto materializeStoreVal = [&](SDValue V, SDLoc DL) {
      if (V.getValueType() != MVT::i32)
        V = CurDAG->getNode(ISD::ANY_EXTEND, DL, MVT::i32, V);
      if (isa<GlobalAddressSDNode>(V) || isa<ExternalSymbolSDNode>(V) ||
          V.getOpcode() == ISD::TargetGlobalAddress ||
          V.getOpcode() == ISD::TargetExternalSymbol) {
        SDValue Sym;
        if (auto *GA = dyn_cast<GlobalAddressSDNode>(V))
          Sym = CurDAG->getTargetGlobalAddress(GA->getGlobal(), DL, MVT::i32,
                                               GA->getOffset(),
                                               GA->getTargetFlags());
        else if (isa<ExternalSymbolSDNode>(V))
          Sym = CurDAG->getTargetExternalSymbol(
              cast<ExternalSymbolSDNode>(V)->getSymbol(), MVT::i32);
        else
          Sym = V;

        SDNode *Mov = CurDAG->getMachineNode(Lamp::MOVI, DL, MVT::i32, Sym);
        return SDValue(Mov, 0);
      }
      if (auto *CN = dyn_cast<ConstantSDNode>(V)) {
        SDValue Imm =
            CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL, MVT::i32);
        SDNode *Mov = CurDAG->getMachineNode(Lamp::MOVI, DL, MVT::i32, Imm);
        return SDValue(Mov, 0);
      }
      return V;
    };

    if (!ST->isIndexed() && ST->getMemoryVT() == MVT::i32 &&
        !ST->isTruncatingStore()) {
      SDValue Base, Offset;
      if (SelectAddr(ST->getBasePtr(), Base, Offset)) {
        SDLoc DL(N);
        Base = materializeGPROp(Base, DL);
        SDValue Val = materializeStoreVal(ST->getValue(), DL);
        SDValue Ops[] = {Val, Base, Offset, ST->getChain()};
        CurDAG->SelectNodeTo(N, Lamp::STORE32, MVT::Other, Ops);
        return;
      }
    }
    if (!ST->isIndexed() && ST->getMemoryVT() == MVT::i8) {
      SDValue Base, Offset;
      if (SelectAddr(ST->getBasePtr(), Base, Offset)) {
        SDLoc DL(N);
        Base = materializeGPROp(Base, DL);
        SDValue Val = materializeStoreVal(ST->getValue(), DL);
        SDValue Ops[] = {Val, Base, Offset, ST->getChain()};
        CurDAG->SelectNodeTo(N, Lamp::STORE, MVT::Other, Ops);
        return;
      }
    }
    if (!ST->isIndexed() && ST->getMemoryVT() == MVT::i16) {
      SDValue Base, Offset;
      if (SelectAddr(ST->getBasePtr(), Base, Offset)) {
        SDLoc DL(N);
        Base = materializeGPROp(Base, DL);
        SDValue Val = materializeStoreVal(ST->getValue(), DL);

        if (ST->getAlign().value() >= 2) {
          SDValue Ops[] = {Val, Base, Offset, ST->getChain()};
          CurDAG->SelectNodeTo(N, Lamp::STORE16, MVT::Other, Ops);
          return;
        }

        SDValue LowOps[] = {Val, Base, Offset, ST->getChain()};
        SDNode *LowStore = CurDAG->getMachineNode(Lamp::STORE, DL, MVT::Other,
                                                  LowOps);

        SDValue ShiftImm = CurDAG->getTargetConstant(8, DL, MVT::i32);
        SDNode *HighValNode =
            CurDAG->getMachineNode(Lamp::SHRI, DL, MVT::i32, Val, ShiftImm);
        SDValue HighVal(HighValNode, 0);

        int64_t Off = 0;
        if (auto *CN = dyn_cast<ConstantSDNode>(Offset))
          Off = CN->getSExtValue();
        SDValue Offset1 = CurDAG->getSignedTargetConstant(Off + 1, DL, MVT::i32);

        SDValue HighOps[] = {HighVal, Base, Offset1, SDValue(LowStore, 0)};
        SDNode *HighStore =
            CurDAG->getMachineNode(Lamp::STORE, DL, MVT::Other, HighOps);
        ReplaceNode(N, HighStore);
        return;
      }
    }
  }

  if (N->getOpcode() == ISD::BR) {
    SDValue Ops[] = {N->getOperand(1), N->getOperand(0)};
    CurDAG->SelectNodeTo(N, Lamp::RJMP, MVT::Other, Ops);
    return;
  }

  if (N->getOpcode() == ISD::BR_CC) {
    SDValue Chain = N->getOperand(0);
    auto *CC = cast<CondCodeSDNode>(N->getOperand(1));
    SDValue LHS = N->getOperand(2);
    SDValue RHS = N->getOperand(3);
    SDValue Dest = N->getOperand(4);

    ISD::CondCode CCCode = CC->get();
    if (isa<ConstantSDNode>(LHS) && !isa<ConstantSDNode>(RHS)) {
      ISD::CondCode Swapped = getSwappedCondCode(CCCode);
      if (Swapped != ISD::SETCC_INVALID) {
        CCCode = Swapped;
        std::swap(LHS, RHS);
      }
    }

    BranchCondInfo Info;
    if (getBranchCondInfo(CCCode, Info)) {
      SDLoc DL(N);

      if (Info.SwapCompareOperands)
        std::swap(LHS, RHS);

      if (auto *CL = dyn_cast<ConstantSDNode>(LHS)) {
        SDValue Imm =
            CurDAG->getSignedTargetConstant(CL->getSExtValue(), DL, MVT::i32);
        SDNode *Mov = CurDAG->getMachineNode(Lamp::MOVI, DL, MVT::i32, Imm);
        LHS = SDValue(Mov, 0);
      } else {
        LHS = materializeGPROp(LHS, DL);
      }

      if (isZeroBranchOpcode(Info.BrOpc)) {
        SDValue Diff;
        if (LHS.getValueType() != MVT::i32)
          LHS = CurDAG->getZExtOrTrunc(LHS, DL, MVT::i32);
        if (auto *CN = dyn_cast<ConstantSDNode>(RHS)) {
          SDValue Imm =
              CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL, MVT::i32);
          SDNode *SubI =
              CurDAG->getMachineNode(Lamp::SUBI, DL, MVT::i32, LHS, Imm);
          Diff = SDValue(SubI, 0);
        } else {
          RHS = materializeGPROp(RHS, DL);
          if (RHS.getValueType() != MVT::i32)
            RHS = CurDAG->getZExtOrTrunc(RHS, DL, MVT::i32);
          SDNode *Sub = CurDAG->getMachineNode(Lamp::SUB, DL, MVT::i32, LHS, RHS);
          Diff = SDValue(Sub, 0);
        }
        SDValue Ops[] = {Diff, Dest, Chain};
        CurDAG->SelectNodeTo(N, Info.BrOpc, MVT::Other, Ops);
      } else {
        SDValue CmpChain;
        SDValue CmpGlue;
        if (LHS.getValueType() != MVT::i32)
          LHS = CurDAG->getZExtOrTrunc(LHS, DL, MVT::i32);
        if (auto *CN = dyn_cast<ConstantSDNode>(RHS)) {
          SDValue Imm =
              CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL, MVT::i32);
          SDValue CmpOps[] = {LHS, Imm, Chain};
          SDNode *Cmp = CurDAG->getMachineNode(Lamp::CMPI, DL,
                                               {MVT::Other, MVT::Glue}, CmpOps);
          CmpChain = SDValue(Cmp, 0);
          CmpGlue = SDValue(Cmp, 1);
        } else {
          RHS = materializeGPROp(RHS, DL);
          if (RHS.getValueType() != MVT::i32)
            RHS = CurDAG->getZExtOrTrunc(RHS, DL, MVT::i32);
          SDValue CmpOps[] = {LHS, RHS, Chain};
          SDNode *Cmp = CurDAG->getMachineNode(Lamp::CMP, DL,
                                               {MVT::Other, MVT::Glue}, CmpOps);
          CmpChain = SDValue(Cmp, 0);
          CmpGlue = SDValue(Cmp, 1);
        }
        SDValue Ops[] = {Dest, CmpChain, CmpGlue};
        CurDAG->SelectNodeTo(N, Info.BrOpc, MVT::Other, Ops);
      }
      return;
    }
  }

  if (N->getOpcode() == ISD::BRCOND) {
    SDValue Cond = N->getOperand(1);
    unsigned BrOpc = Lamp::RJNZ;

    if (Cond.getOpcode() == ISD::SETCC) {
      SDValue LHS = Cond.getOperand(0);
      SDValue RHS = Cond.getOperand(1);
      auto *CC = cast<CondCodeSDNode>(Cond.getOperand(2));
      ISD::CondCode CCCode = CC->get();

      if (isa<ConstantSDNode>(LHS) && !isa<ConstantSDNode>(RHS)) {
        ISD::CondCode Swapped = getSwappedCondCode(CCCode);
        if (Swapped != ISD::SETCC_INVALID) {
          CCCode = Swapped;
          std::swap(LHS, RHS);
        }
      }

      BranchCondInfo Info;
      if (getBranchCondInfo(CCCode, Info)) {
        SDLoc DL(N);
        SDValue Dest = N->getOperand(2);
        SDValue Chain = N->getOperand(0);

        if (Info.SwapCompareOperands)
          std::swap(LHS, RHS);

        if (auto *CL = dyn_cast<ConstantSDNode>(LHS)) {
          SDValue Imm = CurDAG->getSignedTargetConstant(CL->getSExtValue(), DL,
                                                        MVT::i32);
          SDNode *Mov = CurDAG->getMachineNode(Lamp::MOVI, DL, MVT::i32, Imm);
          LHS = SDValue(Mov, 0);
        } else {
          LHS = materializeGPROp(LHS, DL);
        }

        if (isZeroBranchOpcode(Info.BrOpc)) {
          SDValue Diff;
          if (LHS.getValueType() != MVT::i32)
            LHS = CurDAG->getZExtOrTrunc(LHS, DL, MVT::i32);
          if (auto *CN = dyn_cast<ConstantSDNode>(RHS)) {
            SDValue Imm = CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL,
                                                          MVT::i32);
            SDNode *SubI =
                CurDAG->getMachineNode(Lamp::SUBI, DL, MVT::i32, LHS, Imm);
            Diff = SDValue(SubI, 0);
          } else {
            RHS = materializeGPROp(RHS, DL);
            if (RHS.getValueType() != MVT::i32)
              RHS = CurDAG->getZExtOrTrunc(RHS, DL, MVT::i32);
            SDNode *Sub =
                CurDAG->getMachineNode(Lamp::SUB, DL, MVT::i32, LHS, RHS);
            Diff = SDValue(Sub, 0);
          }
          SDValue Ops[] = {Diff, Dest, Chain};
          CurDAG->SelectNodeTo(N, Info.BrOpc, MVT::Other, Ops);
        } else {
          SDValue CmpChain;
          SDValue CmpGlue;
          if (LHS.getValueType() != MVT::i32)
            LHS = CurDAG->getZExtOrTrunc(LHS, DL, MVT::i32);
          if (auto *CN = dyn_cast<ConstantSDNode>(RHS)) {
            SDValue Imm = CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL,
                                                          MVT::i32);
            SDValue CmpOps[] = {LHS, Imm, Chain};
            SDNode *Cmp = CurDAG->getMachineNode(Lamp::CMPI, DL,
                                                 {MVT::Other, MVT::Glue}, CmpOps);
            CmpChain = SDValue(Cmp, 0);
            CmpGlue = SDValue(Cmp, 1);
          } else {
            RHS = materializeGPROp(RHS, DL);
            if (RHS.getValueType() != MVT::i32)
              RHS = CurDAG->getZExtOrTrunc(RHS, DL, MVT::i32);
            SDValue CmpOps[] = {LHS, RHS, Chain};
            SDNode *Cmp = CurDAG->getMachineNode(Lamp::CMP, DL,
                                                 {MVT::Other, MVT::Glue}, CmpOps);
            CmpChain = SDValue(Cmp, 0);
            CmpGlue = SDValue(Cmp, 1);
          }
          SDValue Ops[] = {Dest, CmpChain, CmpGlue};
          CurDAG->SelectNodeTo(N, Info.BrOpc, MVT::Other, Ops);
        }
        return;
      }
    }

    {
      SDLoc DL(N);
      SDValue C = materializeGPROp(Cond, DL);
      if (C.getValueType() != MVT::i32)
        C = CurDAG->getZExtOrTrunc(C, DL, MVT::i32);
      SDValue Zero = CurDAG->getSignedTargetConstant(0, DL, MVT::i32);
      SDNode *CmpZero = CurDAG->getMachineNode(Lamp::SUBI, DL, MVT::i32, C, Zero);
      SDValue Ops[] = {SDValue(CmpZero, 0), N->getOperand(2), N->getOperand(0)};
      CurDAG->SelectNodeTo(N, BrOpc, MVT::Other, Ops);
    }
    return;
  }

  if (N->getOpcode() == LampISD::CALL) {
    SmallVector<SDValue, 4> Ops;
    Ops.push_back(N->getOperand(1));
    Ops.push_back(N->getOperand(0));
    CurDAG->SelectNodeTo(N, Lamp::RCALL, MVT::Other, Ops);
    return;
  }

  if (N->getOpcode() == LampISD::CALLR) {
    SmallVector<SDValue, 4> Ops;
    Ops.push_back(N->getOperand(1));
    Ops.push_back(N->getOperand(0));
    CurDAG->SelectNodeTo(N, Lamp::CALLR, MVT::Other, Ops);
    return;
  }

  if (N->getOpcode() == LampISD::RET) {
    if (N->getNumOperands() == 2) {
      CurDAG->SelectNodeTo(N, Lamp::RET, MVT::Other, N->getOperand(0),
                           N->getOperand(1));
    } else {
      CurDAG->SelectNodeTo(N, Lamp::RET, MVT::Other, N->getOperand(0));
    }
    return;
  }

  if (N->getOpcode() == LampISD::FENCE) {
    CurDAG->SelectNodeTo(N, Lamp::FENCE, MVT::Other, N->getOperand(0));
    return;
  }

  if (N->getOpcode() == LampISD::XCHG || N->getOpcode() == LampISD::XADD) {
    SDValue Base, Offset;
    if (SelectAddr(N->getOperand(1), Base, Offset)) {
      unsigned Opc = N->getOpcode() == LampISD::XCHG ? Lamp::XCHG : Lamp::XADD;
      SDValue Ops[] = {Base, N->getOperand(2), Offset, N->getOperand(0)};
      CurDAG->SelectNodeTo(N, Opc, MVT::i32, MVT::Other, Ops);
      return;
    }
  }

  if (N->getOpcode() == LampISD::CAS) {
    SDValue Base, Offset;
    if (SelectAddr(N->getOperand(1), Base, Offset)) {
      SDValue Ops[] = {Base, N->getOperand(2), N->getOperand(3), Offset,
                       N->getOperand(0)};
      CurDAG->SelectNodeTo(N, Lamp::CAS, MVT::i32, MVT::Other, Ops);
      return;
    }
  }

  if (N->getOpcode() == LampISD::LDAR) {
    SDValue Base, Offset;
    if (SelectAddr(N->getOperand(1), Base, Offset)) {
      SDValue Ops[] = {Base, Offset, N->getOperand(0)};
      CurDAG->SelectNodeTo(N, Lamp::LDAR, MVT::i32, MVT::Other, Ops);
      return;
    }
  }

  if (N->getOpcode() == LampISD::STLR) {
    SDValue Base, Offset;
    if (SelectAddr(N->getOperand(2), Base, Offset)) {
      SDValue Ops[] = {N->getOperand(1), Base, Offset, N->getOperand(0)};
      CurDAG->SelectNodeTo(N, Lamp::STLR, MVT::Other, Ops);
      return;
    }
  }

  SelectCode(N);
}

bool LampDAGToDAGISel::SelectAddr(SDValue Addr, SDValue &Base,
                                  SDValue &Offset) {
  SDLoc DL(Addr);
  if (isa<GlobalAddressSDNode>(Addr) || isa<ExternalSymbolSDNode>(Addr) ||
      Addr.getOpcode() == ISD::TargetGlobalAddress ||
      Addr.getOpcode() == ISD::TargetExternalSymbol) {
    if (TM.isPositionIndependent()) {
      CurDAG->getContext()->diagnose(DiagnosticInfoUnsupported(
          CurDAG->getMachineFunction().getFunction(),
          "PIC relocations are not supported on the Lamp target",
          DL.getDebugLoc()));
    }

    if (auto *GA = dyn_cast<GlobalAddressSDNode>(Addr)) {
      if (GA->getGlobal()->isThreadLocal()) {
        if (!TM.useEmulatedTLS()) CurDAG->getContext()->diagnose(DiagnosticInfoUnsupported(
            CurDAG->getMachineFunction().getFunction(),
            "TLS is not supported on the Lamp target", DL.getDebugLoc()));
      }
    }

    SDValue Sym;
    if (auto *GA = dyn_cast<GlobalAddressSDNode>(Addr))
      Sym = CurDAG->getTargetGlobalAddress(GA->getGlobal(), DL, MVT::i32,
                                           GA->getOffset(),
                                           GA->getTargetFlags());
    else if (isa<ExternalSymbolSDNode>(Addr))
      Sym = CurDAG->getTargetExternalSymbol(
          cast<ExternalSymbolSDNode>(Addr)->getSymbol(), MVT::i32);
    else if (Addr.getOpcode() == ISD::TargetGlobalAddress)
      Sym = Addr;
    else
      Sym = Addr;

    SDNode *Mov = CurDAG->getMachineNode(Lamp::MOVI, DL, MVT::i32, Sym);
    Base = SDValue(Mov, 0);
    Offset = CurDAG->getTargetConstant(0, DL, MVT::i32);
    return true;
  }

  if (auto *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
    Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i32);
    Offset = CurDAG->getTargetConstant(0, DL, MVT::i32);
    return true;
  }

  if (Addr.getOpcode() == ISD::ADD) {
    if (auto *CN = dyn_cast<ConstantSDNode>(Addr.getOperand(1))) {
      Base = Addr.getOperand(0);
      Offset = CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL, MVT::i32);
      return true;
    }
    if (auto *CN = dyn_cast<ConstantSDNode>(Addr.getOperand(0))) {
      Base = Addr.getOperand(1);
      Offset = CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL, MVT::i32);
      return true;
    }
  }

  Base = Addr;
  Offset = CurDAG->getTargetConstant(0, DL, MVT::i32);
  return true;
}

bool LampDAGToDAGISel::SelectInlineAsmMemoryOperand(
    const SDValue &Op, InlineAsm::ConstraintCode ConstraintID,
    std::vector<SDValue> &OutOps) {
  if (ConstraintID != InlineAsm::ConstraintCode::m)
    return true;

  SDValue Base, Offset;
  if (!SelectAddr(Op, Base, Offset))
    return true;

  SDLoc DL(Op);
  SDNode *Addr = CurDAG->getMachineNode(Lamp::ADDI, DL, MVT::i32, Base, Offset);
  OutOps.push_back(SDValue(Addr, 0));
  return false;
}

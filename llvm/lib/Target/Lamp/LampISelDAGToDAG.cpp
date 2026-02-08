#include "Lamp.h"
#include "LampISelLowering.h"
#include "LampTargetMachine.h"
#include "MCTargetDesc/LampMCTargetDesc.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/InitializePasses.h"
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

static bool getBranchCondInfo(ISD::CondCode CC, BranchCondInfo &Info) {
  switch (CC) {
  case ISD::SETEQ:
    Info.BrOpc = Lamp::JZ;
    return true;
  case ISD::SETNE:
    Info.BrOpc = Lamp::JNZ;
    return true;
  case ISD::SETLT:
    Info.BrOpc = Lamp::JL;
    return true;
  case ISD::SETLE:
    Info.BrOpc = Lamp::JLE;
    return true;
  case ISD::SETGT:
    Info.BrOpc = Lamp::JG;
    return true;
  case ISD::SETGE:
    Info.BrOpc = Lamp::JGE;
    return true;
  case ISD::SETULT:
    Info.BrOpc = Lamp::JC;
    return true;
  case ISD::SETUGE:
    Info.BrOpc = Lamp::JNC;
    return true;
  case ISD::SETUGT:
    Info.BrOpc = Lamp::JC;
    Info.SwapCompareOperands = true;
    return true;
  case ISD::SETULE:
    Info.BrOpc = Lamp::JNC;
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
        V.getOpcode() == ISD::AssertZext || V.getOpcode() == ISD::AssertSext)
      return materializeGPROp(V.getOperand(0), DL);

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

    if ((V.getOpcode() == ISD::SHL || V.getOpcode() == ISD::SRL) &&
        isa<ConstantSDNode>(V.getOperand(1))) {
      SDValue L = materializeGPROp(V.getOperand(0), DL);
      auto *CN = cast<ConstantSDNode>(V.getOperand(1));
      SDValue Imm = CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL,
                                                    MVT::i32);
      unsigned Opc = V.getOpcode() == ISD::SHL ? Lamp::SHLI : Lamp::SHRI;
      SDNode *Sh = CurDAG->getMachineNode(Opc, DL, MVT::i32, L, Imm);
      return SDValue(Sh, 0);
    }

    if (V.getValueType() != MVT::i32)
      V = CurDAG->getNode(ISD::ZERO_EXTEND, DL, MVT::i32, V);

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
      if (isa<GlobalAddressSDNode>(V))
        Sym = CurDAG->getTargetGlobalAddress(
            cast<GlobalAddressSDNode>(V)->getGlobal(), DL, MVT::i32);
      else if (isa<ExternalSymbolSDNode>(V))
        Sym = CurDAG->getTargetExternalSymbol(
            cast<ExternalSymbolSDNode>(V)->getSymbol(), MVT::i32);
      else
        Sym = V;

      SDNode *Mov = CurDAG->getMachineNode(Lamp::MOVI, DL, MVT::i32, Sym);
      return SDValue(Mov, 0);
    }

    return V;
  };

  if ((N->getOpcode() == ISD::ZERO_EXTEND ||
       N->getOpcode() == ISD::ANY_EXTEND) &&
      N->getValueType(0) == MVT::i32) {
    SDLoc DL(N);
    SDValue Src = N->getOperand(0);
    MVT SrcVT = Src.getSimpleValueType();
    if (Src.getOpcode() == ISD::TRUNCATE &&
        Src.getOperand(0).getValueType() == MVT::i32) {
      SrcVT = Src.getValueType().getSimpleVT();
      Src = Src.getOperand(0);
    }

    if (SrcVT == MVT::i8 || SrcVT == MVT::i16) {
      SDValue Src32 = materializeGPROp(Src, DL);
      uint64_t Mask = SrcVT == MVT::i8 ? 0xFFu : 0xFFFFu;
      SDValue Imm = CurDAG->getSignedTargetConstant(Mask, DL, MVT::i32);
      CurDAG->SelectNodeTo(N, Lamp::ANDI, MVT::i32, Src32, Imm);
      return;
    }
  }

  if (N->getOpcode() == ISD::SHL) {
    if (auto *CN = dyn_cast<ConstantSDNode>(N->getOperand(1))) {
      SDLoc DL(N);
      EVT VT = N->getValueType(0);
      SDValue LHS = materializeGPROp(N->getOperand(0), DL);
      SDValue Imm =
          CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL, MVT::i32);
      SDNode *Sh = CurDAG->getMachineNode(Lamp::SHLI, DL, MVT::i32, LHS, Imm);
      if (VT == MVT::i32) {
        ReplaceNode(N, Sh);
      } else {
        SDValue Tr = CurDAG->getNode(ISD::TRUNCATE, DL, VT, SDValue(Sh, 0));
        ReplaceNode(N, Tr.getNode());
      }
      return;
    }
  }

  if (N->getOpcode() == ISD::SRL) {
    if (auto *CN = dyn_cast<ConstantSDNode>(N->getOperand(1))) {
      SDLoc DL(N);
      EVT VT = N->getValueType(0);
      SDValue LHS = materializeGPROp(N->getOperand(0), DL);
      SDValue Imm =
          CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL, MVT::i32);
      SDNode *Sh = CurDAG->getMachineNode(Lamp::SHRI, DL, MVT::i32, LHS, Imm);
      if (VT == MVT::i32) {
        ReplaceNode(N, Sh);
      } else {
        SDValue Tr = CurDAG->getNode(ISD::TRUNCATE, DL, VT, SDValue(Sh, 0));
        ReplaceNode(N, Tr.getNode());
      }
      return;
    }
  }

  if (N->getOpcode() == ISD::GlobalAddress) {
    auto *GA = cast<GlobalAddressSDNode>(N);
    SDValue TGA =
        CurDAG->getTargetGlobalAddress(GA->getGlobal(), SDLoc(N), MVT::i32);
    CurDAG->SelectNodeTo(N, Lamp::MOVI, MVT::i32, TGA);
    return;
  }

  if (N->getOpcode() == ISD::ExternalSymbol) {
    auto *ES = cast<ExternalSymbolSDNode>(N);
    SDValue TES =
        CurDAG->getTargetExternalSymbol(ES->getSymbol(), MVT::i32);
    CurDAG->SelectNodeTo(N, Lamp::MOVI, MVT::i32, TES);
    return;
  }

  if (N->getOpcode() == ISD::ADD) {
    SDValue LHS = N->getOperand(0);
    SDValue RHS = N->getOperand(1);
    SDLoc DL(N);
    EVT VT = N->getValueType(0);
    SDValue Sym;

    if (VT != MVT::i32) {
      SDValue L = materializeGPROp(LHS, DL);
      SDValue Sum;
      if (auto *CN = dyn_cast<ConstantSDNode>(RHS)) {
        SDValue Imm =
            CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL, MVT::i32);
        SDNode *AddI = CurDAG->getMachineNode(Lamp::ADDI, DL, MVT::i32, L, Imm);
        Sum = SDValue(AddI, 0);
      } else {
        SDValue R = materializeGPROp(RHS, DL);
        SDNode *Add = CurDAG->getMachineNode(Lamp::ADD, DL, MVT::i32, L, R);
        Sum = SDValue(Add, 0);
      }
      SDValue Tr = CurDAG->getNode(ISD::TRUNCATE, DL, VT, Sum);
      ReplaceNode(N, Tr.getNode());
      return;
    }

    if (isa<GlobalAddressSDNode>(RHS))
      Sym = CurDAG->getTargetGlobalAddress(
          cast<GlobalAddressSDNode>(RHS)->getGlobal(), DL, MVT::i32);
    else if (RHS.getOpcode() == ISD::TargetGlobalAddress)
      Sym = RHS;
    else if (isa<ExternalSymbolSDNode>(RHS))
      Sym = CurDAG->getTargetExternalSymbol(
          cast<ExternalSymbolSDNode>(RHS)->getSymbol(), MVT::i32);
    else if (RHS.getOpcode() == ISD::TargetExternalSymbol)
      Sym = RHS;
    else if (isa<GlobalAddressSDNode>(LHS))
      Sym = CurDAG->getTargetGlobalAddress(
          cast<GlobalAddressSDNode>(LHS)->getGlobal(), DL, MVT::i32);
    else if (LHS.getOpcode() == ISD::TargetGlobalAddress)
      Sym = LHS;
    else if (isa<ExternalSymbolSDNode>(LHS))
      Sym = CurDAG->getTargetExternalSymbol(
          cast<ExternalSymbolSDNode>(LHS)->getSymbol(), MVT::i32);
    else if (LHS.getOpcode() == ISD::TargetExternalSymbol)
      Sym = LHS;

    if (Sym.getNode()) {
      SDValue BaseReg = (Sym == RHS) ? LHS : RHS;
      SDNode *Mov = CurDAG->getMachineNode(Lamp::MOVI, DL, MVT::i32, Sym);
      CurDAG->SelectNodeTo(N, Lamp::ADD, MVT::i32, BaseReg, SDValue(Mov, 0));
      return;
    }

    if (auto *CN = dyn_cast<ConstantSDNode>(RHS)) {
      SDValue L = materializeGPROp(LHS, DL);
      SDValue Imm =
          CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL, MVT::i32);
      CurDAG->SelectNodeTo(N, Lamp::ADDI, MVT::i32, L, Imm);
      return;
    }
    if (auto *CN = dyn_cast<ConstantSDNode>(LHS)) {
      SDValue R = materializeGPROp(RHS, DL);
      SDValue Imm =
          CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL, MVT::i32);
      CurDAG->SelectNodeTo(N, Lamp::ADDI, MVT::i32, R, Imm);
      return;
    }

    SDValue L = materializeGPROp(LHS, DL);
    SDValue R = materializeGPROp(RHS, DL);
    CurDAG->SelectNodeTo(N, Lamp::ADD, MVT::i32, L, R);
    return;
  }

  if (N->getOpcode() == ISD::SUB) {
    SDLoc DL(N);
    EVT VT = N->getValueType(0);
    SDValue LHS = N->getOperand(0);
    SDValue RHS = N->getOperand(1);

    if (VT != MVT::i32) {
      SDValue L = materializeGPROp(LHS, DL);
      SDValue Diff;
      if (auto *CN = dyn_cast<ConstantSDNode>(RHS)) {
        SDValue Imm =
            CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL, MVT::i32);
        SDNode *SubI =
            CurDAG->getMachineNode(Lamp::SUBI, DL, MVT::i32, L, Imm);
        Diff = SDValue(SubI, 0);
      } else {
        SDValue R = materializeGPROp(RHS, DL);
        SDNode *Sub = CurDAG->getMachineNode(Lamp::SUB, DL, MVT::i32, L, R);
        Diff = SDValue(Sub, 0);
      }
      SDValue Tr = CurDAG->getNode(ISD::TRUNCATE, DL, VT, Diff);
      ReplaceNode(N, Tr.getNode());
      return;
    }

    if (auto *CN = dyn_cast<ConstantSDNode>(RHS)) {
      SDValue L = materializeGPROp(LHS, DL);
      SDValue Imm =
          CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL, MVT::i32);
      CurDAG->SelectNodeTo(N, Lamp::SUBI, MVT::i32, L, Imm);
      return;
    }

    SDValue L = materializeGPROp(LHS, DL);
    SDValue R = materializeGPROp(RHS, DL);
    CurDAG->SelectNodeTo(N, Lamp::SUB, MVT::i32, L, R);
    return;
  }

  if (N->getOpcode() == ISD::MUL) {
    SDLoc DL(N);
    EVT VT = N->getValueType(0);
    SDValue L = materializeGPROp(N->getOperand(0), DL);
    SDValue R = materializeGPROp(N->getOperand(1), DL);
    SDNode *Mul = CurDAG->getMachineNode(Lamp::MUL, DL, MVT::i32, L, R);
    if (VT == MVT::i32) {
      ReplaceNode(N, Mul);
    } else {
      SDValue Tr = CurDAG->getNode(ISD::TRUNCATE, DL, VT, SDValue(Mul, 0));
      ReplaceNode(N, Tr.getNode());
    }
    return;
  }

  if (N->getOpcode() == ISD::LOAD) {
    auto *LD = cast<LoadSDNode>(N);
    // Leave plain i16 loads to generic legalization/selection paths.
    // The custom byte-merge path here interacted badly with later zext/shl
    // uses and could introduce NOREG operands.
    if (!LD->isIndexed() && LD->getExtensionType() == ISD::NON_EXTLOAD &&
        LD->getMemoryVT() == MVT::i32 && LD->getValueType(0) == MVT::i32) {
      SDValue Base, Offset;
      if (SelectAddr(LD->getBasePtr(), Base, Offset)) {
        Base = materializeGPROp(Base, SDLoc(N));
        SDValue Ops[] = {Base, Offset, LD->getChain()};
        CurDAG->SelectNodeTo(N, Lamp::LOAD32, MVT::i32, MVT::Other, Ops);
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
        Base = materializeGPROp(Base, SDLoc(N));
        SDValue Ops[] = {Base, Offset, LD->getChain()};
        CurDAG->SelectNodeTo(N, Lamp::LOAD, MVT::i32, MVT::Other, Ops);
        return;
      }
    }
    if (!LD->isIndexed() &&
        (LD->getExtensionType() == ISD::ZEXTLOAD ||
         LD->getExtensionType() == ISD::EXTLOAD) &&
        LD->getMemoryVT() == MVT::i16 && LD->getValueType(0) == MVT::i32) {
      SDValue Base, Offset;
      if (SelectAddr(LD->getBasePtr(), Base, Offset)) {
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
        SDValue ResOps[] = {SDValue(Merged, 0), SDValue(HighLoad, 1)};
        ReplaceNode(N, CurDAG->getMergeValues(ResOps, DL).getNode());
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
        if (isa<GlobalAddressSDNode>(V))
          Sym = CurDAG->getTargetGlobalAddress(
              cast<GlobalAddressSDNode>(V)->getGlobal(), DL, MVT::i32);
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
    CurDAG->SelectNodeTo(N, Lamp::JMP, MVT::Other, Ops);
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

      if ((Info.BrOpc == Lamp::JZ || Info.BrOpc == Lamp::JNZ) &&
          isa<ConstantSDNode>(RHS) && cast<ConstantSDNode>(RHS)->isZero()) {
        SDValue Ops[] = {LHS, Dest, Chain};
        CurDAG->SelectNodeTo(N, Info.BrOpc, MVT::Other, Ops);
        return;
      }

      if (auto *CL = dyn_cast<ConstantSDNode>(LHS)) {
        SDValue Imm =
            CurDAG->getSignedTargetConstant(CL->getSExtValue(), DL, MVT::i32);
        SDNode *Mov = CurDAG->getMachineNode(Lamp::MOVI, DL, MVT::i32, Imm);
        LHS = SDValue(Mov, 0);
      }

      if (Info.BrOpc == Lamp::JZ || Info.BrOpc == Lamp::JNZ) {
        SDValue Diff;
        if (auto *CN = dyn_cast<ConstantSDNode>(RHS)) {
          SDValue Imm =
              CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL, MVT::i32);
          SDNode *SubI =
              CurDAG->getMachineNode(Lamp::SUBI, DL, MVT::i32, LHS, Imm);
          Diff = SDValue(SubI, 0);
        } else {
          SDNode *Sub = CurDAG->getMachineNode(Lamp::SUB, DL, MVT::i32, LHS, RHS);
          Diff = SDValue(Sub, 0);
        }
        SDValue Ops[] = {Diff, Dest, Chain};
        CurDAG->SelectNodeTo(N, Info.BrOpc, MVT::Other, Ops);
      } else {
        if (auto *CN = dyn_cast<ConstantSDNode>(RHS)) {
          SDValue Imm =
              CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL, MVT::i32);
          (void)CurDAG->getMachineNode(Lamp::CMPI, DL, MVT::i32, LHS, Imm);
        } else {
          (void)CurDAG->getMachineNode(Lamp::CMP, DL, MVT::i32, LHS, RHS);
        }
        SDValue Ops[] = {Dest, Chain};
        CurDAG->SelectNodeTo(N, Info.BrOpc, MVT::Other, Ops);
      }
      return;
    }
  }

  if (N->getOpcode() == ISD::BRCOND) {
    SDValue Cond = N->getOperand(1);
    unsigned BrOpc = Lamp::JNZ;

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

        if ((Info.BrOpc == Lamp::JZ || Info.BrOpc == Lamp::JNZ) &&
            isa<ConstantSDNode>(RHS) && cast<ConstantSDNode>(RHS)->isZero()) {
          SDValue Ops[] = {LHS, Dest, Chain};
          CurDAG->SelectNodeTo(N, Info.BrOpc, MVT::Other, Ops);
          return;
        }

        if (auto *CL = dyn_cast<ConstantSDNode>(LHS)) {
          SDValue Imm = CurDAG->getSignedTargetConstant(CL->getSExtValue(), DL,
                                                        MVT::i32);
          SDNode *Mov = CurDAG->getMachineNode(Lamp::MOVI, DL, MVT::i32, Imm);
          LHS = SDValue(Mov, 0);
        }

        if (Info.BrOpc == Lamp::JZ || Info.BrOpc == Lamp::JNZ) {
          SDValue Diff;
          if (auto *CN = dyn_cast<ConstantSDNode>(RHS)) {
            SDValue Imm = CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL,
                                                          MVT::i32);
            SDNode *SubI =
                CurDAG->getMachineNode(Lamp::SUBI, DL, MVT::i32, LHS, Imm);
            Diff = SDValue(SubI, 0);
          } else {
            SDNode *Sub =
                CurDAG->getMachineNode(Lamp::SUB, DL, MVT::i32, LHS, RHS);
            Diff = SDValue(Sub, 0);
          }
          SDValue Ops[] = {Diff, Dest, Chain};
          CurDAG->SelectNodeTo(N, Info.BrOpc, MVT::Other, Ops);
        } else {
          if (auto *CN = dyn_cast<ConstantSDNode>(RHS)) {
            SDValue Imm = CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL,
                                                          MVT::i32);
            (void)CurDAG->getMachineNode(Lamp::CMPI, DL, MVT::i32, LHS, Imm);
          } else {
            (void)CurDAG->getMachineNode(Lamp::CMP, DL, MVT::i32, LHS, RHS);
          }
          SDValue Ops[] = {Dest, Chain};
          CurDAG->SelectNodeTo(N, Info.BrOpc, MVT::Other, Ops);
        }
        return;
      }
    }

    SDValue Ops[] = {Cond, N->getOperand(2), N->getOperand(0)};
    CurDAG->SelectNodeTo(N, BrOpc, MVT::Other, Ops);
    return;
  }

  if (N->getOpcode() == LampISD::CALL) {
    SmallVector<SDValue, 4> Ops;
    Ops.push_back(N->getOperand(1));
    Ops.push_back(N->getOperand(0));
    CurDAG->SelectNodeTo(N, Lamp::CALL, MVT::Other, Ops);
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
    SDValue Sym;
    if (isa<GlobalAddressSDNode>(Addr))
      Sym = CurDAG->getTargetGlobalAddress(
          cast<GlobalAddressSDNode>(Addr)->getGlobal(), DL, MVT::i32);
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

#include "Lamp.h"
#include "LampISelLowering.h"
#include "LampTargetMachine.h"
#include "MCTargetDesc/LampMCTargetDesc.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/InitializePasses.h"
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
    SDValue Sym;

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
  }

  if (N->getOpcode() == ISD::LOAD) {
    auto *LD = cast<LoadSDNode>(N);
    if (LD->isSimple() && LD->getMemoryVT() == MVT::i32) {
      SDValue Base, Offset;
      if (SelectAddr(LD->getBasePtr(), Base, Offset)) {
        SDValue Ops[] = {Base, Offset, LD->getChain()};
        CurDAG->SelectNodeTo(N, Lamp::LOAD32, MVT::i32, MVT::Other, Ops);
        return;
      }
    }
    if (LD->isSimple() && LD->getMemoryVT() == MVT::i8 &&
        LD->getValueType(0) == MVT::i32) {
      SDValue Base, Offset;
      if (SelectAddr(LD->getBasePtr(), Base, Offset)) {
        SDValue Ops[] = {Base, Offset, LD->getChain()};
        CurDAG->SelectNodeTo(N, Lamp::LOAD, MVT::i32, MVT::Other, Ops);
        return;
      }
    }
  }

  if (N->getOpcode() == ISD::STORE) {
    auto *ST = cast<StoreSDNode>(N);
    if (ST->isSimple() && ST->getMemoryVT() == MVT::i32 &&
        !ST->isTruncatingStore()) {
      SDValue Base, Offset;
      if (SelectAddr(ST->getBasePtr(), Base, Offset)) {
        SDValue Ops[] = {ST->getValue(), Base, Offset, ST->getChain()};
        CurDAG->SelectNodeTo(N, Lamp::STORE32, MVT::Other, Ops);
        return;
      }
    }
    if (ST->isSimple() && ST->getMemoryVT() == MVT::i8) {
      SDValue Base, Offset;
      if (SelectAddr(ST->getBasePtr(), Base, Offset)) {
        SDValue Ops[] = {ST->getValue(), Base, Offset, ST->getChain()};
        CurDAG->SelectNodeTo(N, Lamp::STORE, MVT::Other, Ops);
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
        SDValue CmpChain = Chain;
        if (auto *CN = dyn_cast<ConstantSDNode>(RHS)) {
          SDValue Imm =
              CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL, MVT::i32);
          SDValue CmpOps[] = {LHS, Imm, Chain};
          SDNode *Cmp =
              CurDAG->getMachineNode(Lamp::CMPI, DL, {MVT::i32, MVT::Other},
                                     CmpOps);
          CmpChain = SDValue(Cmp, 1);
        } else {
          SDValue CmpOps[] = {LHS, RHS, Chain};
          SDNode *Cmp =
              CurDAG->getMachineNode(Lamp::CMP, DL, {MVT::i32, MVT::Other},
                                     CmpOps);
          CmpChain = SDValue(Cmp, 1);
        }
        SDValue Ops[] = {Dest, CmpChain};
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
          SDValue CmpChain = Chain;
          if (auto *CN = dyn_cast<ConstantSDNode>(RHS)) {
            SDValue Imm = CurDAG->getSignedTargetConstant(CN->getSExtValue(), DL,
                                                          MVT::i32);
            SDValue CmpOps[] = {LHS, Imm, Chain};
            SDNode *Cmp =
                CurDAG->getMachineNode(Lamp::CMPI, DL, {MVT::i32, MVT::Other},
                                       CmpOps);
            CmpChain = SDValue(Cmp, 1);
          } else {
            SDValue CmpOps[] = {LHS, RHS, Chain};
            SDNode *Cmp =
                CurDAG->getMachineNode(Lamp::CMP, DL, {MVT::i32, MVT::Other},
                                       CmpOps);
            CmpChain = SDValue(Cmp, 1);
          }
          SDValue Ops[] = {Dest, CmpChain};
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
    SmallVector<SDValue, 16> Ops;
    Ops.push_back(N->getOperand(1));
    for (unsigned I = 2, E = N->getNumOperands(); I != E; ++I)
      Ops.push_back(N->getOperand(I));
    Ops.push_back(N->getOperand(0));
    CurDAG->SelectNodeTo(N, Lamp::CALL, MVT::Other, Ops);
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

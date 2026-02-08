#include "LampISelLowering.h"
#include "LampSubtarget.h"
#include "MCTargetDesc/LampMCTargetDesc.h"
#include "LampTargetMachine.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"

using namespace llvm;

#define DEBUG_TYPE "lamp-lower"

#include "LampGenCallingConv.inc"

LampTargetLowering::LampTargetLowering(const LampTargetMachine &TM,
                                       const LampSubtarget &STI)
    : TargetLowering(TM, STI) {
  addRegisterClass(MVT::i32, &Lamp::GPRRegClass);
  MVT PtrVT = getPointerTy(TM.createDataLayout());
  (void)PtrVT;

  // Lamp only has a 32-bit integer register class. Keep narrow integer
  // arithmetic out of ISel by promoting it to i32 first.
  for (MVT VT : {MVT::i8, MVT::i16}) {
    for (unsigned Opc : {ISD::ADD, ISD::SUB, ISD::MUL, ISD::SHL, ISD::SRL,
                         ISD::SRA, ISD::AND, ISD::OR, ISD::XOR}) {
      setOperationAction(Opc, VT, Promote);
    }
    setOperationAction(ISD::SETCC, VT, Promote);
    setOperationAction(ISD::BR_CC, VT, Promote);
    setOperationAction(ISD::SELECT_CC, VT, Promote);
  }

  setOperationAction(ISD::ATOMIC_FENCE, MVT::Other, Custom);
  setOperationAction(ISD::ATOMIC_CMP_SWAP, MVT::i32, Custom);
  setOperationAction(ISD::ATOMIC_CMP_SWAP_WITH_SUCCESS, MVT::i32, Expand);
  setOperationAction(ISD::ATOMIC_SWAP, MVT::i32, Custom);
  setOperationAction(ISD::ATOMIC_LOAD_ADD, MVT::i32, Custom);
  setOperationAction(ISD::ATOMIC_LOAD, MVT::i32, Custom);
  setOperationAction(ISD::ATOMIC_STORE, MVT::i32, Custom);
  computeRegisterProperties(STI.getRegisterInfo());
  setStackPointerRegisterToSaveRestore(Lamp::R30);
  setBooleanContents(ZeroOrOneBooleanContent);
}

SDValue LampTargetLowering::LowerOperation(SDValue Op,
                                           SelectionDAG &DAG) const {
  SDLoc DL(Op);
  switch (Op.getOpcode()) {
  case ISD::ATOMIC_FENCE:
    return DAG.getNode(LampISD::FENCE, DL, MVT::Other, Op.getOperand(0));
  case ISD::ATOMIC_SWAP: {
    auto *AN = cast<AtomicSDNode>(Op.getNode());
    SDVTList VTs = DAG.getVTList(Op.getValueType(), MVT::Other);
    SDValue Ops[] = {AN->getOperand(0), AN->getOperand(1), AN->getOperand(2)};
    return DAG.getMemIntrinsicNode(LampISD::XCHG, DL, VTs, Ops,
                                   AN->getMemoryVT(), AN->getMemOperand());
  }
  case ISD::ATOMIC_LOAD_ADD: {
    auto *AN = cast<AtomicSDNode>(Op.getNode());
    SDVTList VTs = DAG.getVTList(Op.getValueType(), MVT::Other);
    SDValue Ops[] = {AN->getOperand(0), AN->getOperand(1), AN->getOperand(2)};
    return DAG.getMemIntrinsicNode(LampISD::XADD, DL, VTs, Ops,
                                   AN->getMemoryVT(), AN->getMemOperand());
  }
  case ISD::ATOMIC_LOAD: {
    auto *AN = cast<AtomicSDNode>(Op.getNode());
    SDVTList VTs = DAG.getVTList(Op.getValueType(), MVT::Other);
    SDValue Ops[] = {AN->getChain(), AN->getBasePtr()};
    return DAG.getMemIntrinsicNode(LampISD::LDAR, DL, VTs, Ops,
                                   AN->getMemoryVT(), AN->getMemOperand());
  }
  case ISD::ATOMIC_STORE: {
    auto *AN = cast<AtomicSDNode>(Op.getNode());
    SDVTList VTs = DAG.getVTList(MVT::Other);
    SDValue Ops[] = {AN->getChain(), AN->getVal(), AN->getBasePtr()};
    return DAG.getMemIntrinsicNode(LampISD::STLR, DL, VTs, Ops,
                                   AN->getMemoryVT(), AN->getMemOperand());
  }
  case ISD::ATOMIC_CMP_SWAP: {
    auto *AN = cast<AtomicSDNode>(Op.getNode());
    SDVTList VTs = DAG.getVTList(Op.getValueType(), MVT::Other);
    SDValue Ops[] = {AN->getOperand(0), AN->getOperand(1), AN->getOperand(2),
                     AN->getOperand(3)};
    return DAG.getMemIntrinsicNode(LampISD::CAS, DL, VTs, Ops,
                                   AN->getMemoryVT(), AN->getMemOperand());
  }
  default:
    break;
  }
  return SDValue();
}

const char *LampTargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (Opcode) {
  case LampISD::CALL:
    return "LampISD::CALL";
  case LampISD::CALLR:
    return "LampISD::CALLR";
  case LampISD::RET:
    return "LampISD::RET";
  case LampISD::FENCE:
    return "LampISD::FENCE";
  case LampISD::CAS:
    return "LampISD::CAS";
  case LampISD::XADD:
    return "LampISD::XADD";
  case LampISD::XCHG:
    return "LampISD::XCHG";
  case LampISD::LDAR:
    return "LampISD::LDAR";
  case LampISD::STLR:
    return "LampISD::STLR";
  default:
    return nullptr;
  }
}

TargetLowering::ConstraintType
LampTargetLowering::getConstraintType(StringRef Constraint) const {
  if (Constraint.size() == 1 && Constraint[0] == 'r')
    return C_RegisterClass;
  return TargetLowering::getConstraintType(Constraint);
}

std::pair<unsigned, const TargetRegisterClass *>
LampTargetLowering::getRegForInlineAsmConstraint(
    const TargetRegisterInfo *TRI, StringRef Constraint, MVT VT) const {
  if (Constraint.size() == 1 && Constraint[0] == 'r')
    return std::make_pair(0U, &Lamp::GPRRegClass);
  return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
}

SDValue LampTargetLowering::LowerCall(
    CallLoweringInfo &CLI, SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;
  SDLoc &DL = CLI.DL;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;
  SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  CallingConv::ID CallConv = CLI.CallConv;
  bool IsVarArg = CLI.IsVarArg;

  CLI.IsTailCall = false;
  if (IsVarArg)
    report_fatal_error("Lamp vararg call is not supported yet");

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeCallOperands(Outs, CC_Lamp);

  unsigned NumBytes = CCInfo.getStackSize();
  if (NumBytes != 0)
    report_fatal_error("Lamp stack call arguments are not supported yet");

  SmallVector<std::pair<Register, SDValue>, 8> RegsToPass;

  for (unsigned I = 0; I < ArgLocs.size(); ++I) {
    CCValAssign &VA = ArgLocs[I];
    SDValue Arg = OutVals[I];

    if (VA.getLocInfo() == CCValAssign::SExt)
      Arg = DAG.getNode(ISD::SIGN_EXTEND, DL, VA.getLocVT(), Arg);
    else if (VA.getLocInfo() == CCValAssign::ZExt)
      Arg = DAG.getNode(ISD::ZERO_EXTEND, DL, VA.getLocVT(), Arg);
    else if (VA.getLocInfo() == CCValAssign::AExt)
      Arg = DAG.getNode(ISD::ANY_EXTEND, DL, VA.getLocVT(), Arg);

    if (VA.isRegLoc()) {
      RegsToPass.push_back({VA.getLocReg(), Arg});
      continue;
    }

    report_fatal_error("Lamp stack-passed call args are not supported yet");
  }

  for (const auto &[Reg, Val] : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, Reg, Val);
  }

  unsigned CallOpc = LampISD::CALL;
  if (auto *G = dyn_cast<GlobalAddressSDNode>(Callee)) {
    Callee = DAG.getTargetGlobalAddress(G->getGlobal(), DL, MVT::i32);
  } else if (auto *E = dyn_cast<ExternalSymbolSDNode>(Callee)) {
    Callee = DAG.getTargetExternalSymbol(E->getSymbol(), MVT::i32);
  } else {
    if (Callee.getValueType() != MVT::i32)
      Callee = DAG.getZExtOrTrunc(Callee, DL, MVT::i32);
    CallOpc = LampISD::CALLR;
  }

  SDVTList NodeTys = DAG.getVTList(MVT::Other);
  SmallVector<SDValue, 16> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);

  Chain = DAG.getNode(CallOpc, DL, NodeTys, Ops);

  SmallVector<CCValAssign, 16> RVLocs;
  CCState RetCCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), RVLocs,
                    *DAG.getContext());
  RetCCInfo.AnalyzeCallResult(Ins, RetCC_Lamp);

  for (CCValAssign &VA : RVLocs) {
    SDValue Ret = DAG.getCopyFromReg(Chain, DL, VA.getLocReg(), VA.getValVT());
    Chain = Ret.getValue(1);
    InVals.push_back(Ret.getValue(0));
  }

  return Chain;
}

SDValue LampTargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineRegisterInfo &RegInfo = MF.getRegInfo();

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeFormalArguments(Ins, CC_Lamp);

  for (CCValAssign &VA : ArgLocs) {
    if (VA.isRegLoc()) {
      Register VReg = RegInfo.createVirtualRegister(&Lamp::GPRRegClass);
      RegInfo.addLiveIn(VA.getLocReg(), VReg);
      SDValue Arg = DAG.getCopyFromReg(Chain, DL, VReg, VA.getValVT());
      InVals.push_back(Arg);
      Chain = Arg.getValue(1);
      continue;
    }

    int FI = MF.getFrameInfo().CreateFixedObject(4, VA.getLocMemOffset(), true);
    SDValue FIN = DAG.getFrameIndex(FI, MVT::i32);
    SDValue Ld = DAG.getLoad(VA.getValVT(), DL, Chain, FIN, MachinePointerInfo());
    InVals.push_back(Ld);
    Chain = Ld.getValue(1);
  }

  return Chain;
}

SDValue LampTargetLowering::LowerReturn(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs,
    const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
    SelectionDAG &DAG) const {
  SmallVector<CCValAssign, 16> RVLocs;
  MachineFunction &MF = DAG.getMachineFunction();
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, *DAG.getContext());
  CCInfo.AnalyzeReturn(Outs, RetCC_Lamp);

  SDValue Glue;
  for (unsigned i = 0; i < RVLocs.size(); ++i) {
    Chain = DAG.getCopyToReg(Chain, DL, RVLocs[i].getLocReg(), OutVals[i], Glue);
    Glue = Chain.getValue(1);
  }

  SmallVector<SDValue, 4> RetOps;
  RetOps.push_back(Chain);
  if (Glue.getNode())
    RetOps.push_back(Glue);
  return DAG.getNode(LampISD::RET, DL, MVT::Other, RetOps);
}

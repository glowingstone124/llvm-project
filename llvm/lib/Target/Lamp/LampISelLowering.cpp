#include "LampISelLowering.h"
#include "LampMachineFunctionInfo.h"
#include "LampSubtarget.h"
#include "MCTargetDesc/LampMCTargetDesc.h"
#include "LampTargetMachine.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/Support/MathExtras.h"
#include <climits>

using namespace llvm;

#define DEBUG_TYPE "lamp-lower"

#include "LampGenCallingConv.inc"

static void diagnoseUnsupported(const SDLoc &DL, SelectionDAG &DAG,
                                const Twine &Msg) {
  MachineFunction &MF = DAG.getMachineFunction();
  DAG.getContext()->diagnose(
      DiagnosticInfoUnsupported(MF.getFunction(), Msg, DL.getDebugLoc()));
}

static SDValue lowerVASTART(SDValue Op, SelectionDAG &DAG) {
  MachineFunction &MF = DAG.getMachineFunction();
  auto *FuncInfo = MF.getInfo<LampMachineFunctionInfo>();
  if (!FuncInfo->hasVarArgsFrameIndex())
    return Op.getOperand(0);

  SDValue Ptr = Op.getOperand(1);
  EVT PtrVT = Ptr.getValueType();
  SDValue FrameIndex = DAG.getFrameIndex(FuncInfo->getVarArgsFrameIndex(), PtrVT);
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  return DAG.getStore(Op.getOperand(0), SDLoc(Op), FrameIndex, Ptr,
                      MachinePointerInfo(SV));
}

static SDValue lowerDYNAMIC_STACKALLOC(SDValue Op, SelectionDAG &DAG) {
  SDLoc DL(Op);
  EVT PtrVT = Op.getValueType();

  SDValue Chain = Op.getOperand(0);
  SDValue Size = Op.getOperand(1);
  SDValue AlignOp = Op.getOperand(2);

  if (Size.getValueType() != PtrVT)
    Size = DAG.getZExtOrTrunc(Size, DL, PtrVT);

  uint64_t Align = 4;
  if (const auto *CN = dyn_cast<ConstantSDNode>(AlignOp)) {
    const uint64_t Requested = CN->getZExtValue();
    if (Requested != 0 && isPowerOf2_64(Requested))
      Align = std::max<uint64_t>(Align, Requested);
  }

  if (Align > 1) {
    SDValue AlignMinus1 = DAG.getConstant(Align - 1, DL, PtrVT);
    SDValue AlignMask = DAG.getSignedConstant(-static_cast<int64_t>(Align), DL,
                                              PtrVT);
    Size = DAG.getNode(ISD::ADD, DL, PtrVT, Size, AlignMinus1);
    Size = DAG.getNode(ISD::AND, DL, PtrVT, Size, AlignMask);
  }

  SDValue SP = DAG.getCopyFromReg(Chain, DL, Lamp::R30, PtrVT);
  Chain = SP.getValue(1);
  SDValue NewSP = DAG.getNode(ISD::SUB, DL, PtrVT, SP, Size);
  Chain = DAG.getCopyToReg(Chain, DL, Lamp::R30, NewSP);

  SDValue Ops[2] = {NewSP, Chain};
  return DAG.getMergeValues(Ops, DL);
}

static SDValue lowerMULHU(SDValue Op, SelectionDAG &DAG) {
  SDLoc DL(Op);
  EVT VT = Op.getValueType();
  SDValue A = Op.getOperand(0);
  SDValue B = Op.getOperand(1);

  SDValue Mask16 = DAG.getConstant(0xFFFFu, DL, VT);
  SDValue Shift16 = DAG.getConstant(16, DL, VT);

  SDValue ALo = DAG.getNode(ISD::AND, DL, VT, A, Mask16);
  SDValue AHi = DAG.getNode(ISD::SRL, DL, VT, A, Shift16);
  SDValue BLo = DAG.getNode(ISD::AND, DL, VT, B, Mask16);
  SDValue BHi = DAG.getNode(ISD::SRL, DL, VT, B, Shift16);

  SDValue P0 = DAG.getNode(ISD::MUL, DL, VT, ALo, BLo);
  SDValue T = DAG.getNode(ISD::SRL, DL, VT, P0, Shift16);

  SDValue U = DAG.getNode(
      ISD::ADD, DL, VT, DAG.getNode(ISD::MUL, DL, VT, AHi, BLo), T);
  SDValue V = DAG.getNode(
      ISD::ADD, DL, VT, DAG.getNode(ISD::MUL, DL, VT, ALo, BHi),
      DAG.getNode(ISD::AND, DL, VT, U, Mask16));

  SDValue Hi = DAG.getNode(
      ISD::ADD, DL, VT, DAG.getNode(ISD::MUL, DL, VT, AHi, BHi),
      DAG.getNode(ISD::SRL, DL, VT, U, Shift16));
  return DAG.getNode(ISD::ADD, DL, VT, Hi,
                     DAG.getNode(ISD::SRL, DL, VT, V, Shift16));
}

static SDValue lowerMULHS(SDValue Op, SelectionDAG &DAG) {
  SDLoc DL(Op);
  EVT VT = Op.getValueType();
  SDValue A = Op.getOperand(0);
  SDValue B = Op.getOperand(1);

  SDValue UHi = lowerMULHU(Op, DAG);
  SDValue Shift31 = DAG.getConstant(31, DL, VT);
  SDValue SignA = DAG.getNode(ISD::SRA, DL, VT, A, Shift31);
  SDValue SignB = DAG.getNode(ISD::SRA, DL, VT, B, Shift31);
  SDValue CorrA = DAG.getNode(ISD::AND, DL, VT, SignA, B);
  SDValue CorrB = DAG.getNode(ISD::AND, DL, VT, SignB, A);
  SDValue Corr = DAG.getNode(ISD::ADD, DL, VT, CorrA, CorrB);
  return DAG.getNode(ISD::SUB, DL, VT, UHi, Corr);
}

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
                         ISD::SRA, ISD::ROTL, ISD::ROTR, ISD::AND, ISD::OR,
                         ISD::XOR}) {
      setOperationAction(Opc, VT, Promote);
    }
    setOperationAction(ISD::SETCC, VT, Promote);
    setOperationAction(ISD::BR_CC, VT, Promote);
    setOperationAction(ISD::SELECT_CC, VT, Promote);
  }
  // Force compare-and-branch expansion through SETCC/BRCOND. This avoids
  // depending on flag-based unsigned branch lowering for address-range tests.
  setOperationAction(ISD::BR_CC, MVT::i32, Expand);
  setOperationAction(ISD::SELECT_CC, MVT::i32, Expand);
  // We only have low-part integer multiply in hardware.
  setOperationAction(ISD::UMUL_LOHI, MVT::i32, Expand);
  setOperationAction(ISD::SMUL_LOHI, MVT::i32, Expand);
  setOperationAction(ISD::MULHU, MVT::i32, Custom);
  setOperationAction(ISD::MULHS, MVT::i32, Custom);
  setOperationAction(ISD::SHL, MVT::i64, LibCall);
  setOperationAction(ISD::SRL, MVT::i64, LibCall);
  setOperationAction(ISD::SRA, MVT::i64, LibCall);
  setOperationAction(ISD::SHL_PARTS, MVT::i32, Expand);
  setOperationAction(ISD::SRL_PARTS, MVT::i32, Expand);
  setOperationAction(ISD::SRA_PARTS, MVT::i32, Expand);
  setOperationAction(ISD::ROTL, MVT::i32, Legal);
  setOperationAction(ISD::ROTR, MVT::i32, Legal);
  // Lamp has no native funnel-shift instructions.
  // Force legalizer expansion so these never reach DAG isel.
  for (MVT VT : {MVT::i8, MVT::i16, MVT::i32}) {
    setOperationAction(ISD::FSHL, VT, Expand);
    setOperationAction(ISD::FSHR, VT, Expand);
    setOperationAction(ISD::BSWAP, VT, Expand);
    setOperationAction(ISD::CTPOP, VT, Expand);
    setOperationAction(ISD::CTTZ, VT, Expand);
    setOperationAction(ISD::CTLZ, VT, Expand);
  }

  setOperationAction(ISD::ATOMIC_FENCE, MVT::Other, Custom);
  setOperationAction(ISD::ATOMIC_CMP_SWAP, MVT::i32, Custom);
  setOperationAction(ISD::ATOMIC_CMP_SWAP_WITH_SUCCESS, MVT::i32, Expand);
  setOperationAction(ISD::ATOMIC_SWAP, MVT::i32, Custom);
  setOperationAction(ISD::ATOMIC_LOAD_ADD, MVT::i32, Custom);
  setOperationAction(ISD::ATOMIC_LOAD, MVT::i32, Custom);
  setOperationAction(ISD::ATOMIC_STORE, MVT::i32, Custom);
  setOperationAction(ISD::VASTART, MVT::Other, Custom);
  setOperationAction(ISD::VAARG, MVT::Other, Expand);
  setOperationAction(ISD::VAEND, MVT::Other, Expand);
  setOperationAction(ISD::VACOPY, MVT::Other, Expand);
  setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i32, Custom);
  setOperationAction(ISD::GlobalTLSAddress, MVT::i32, Custom);
  setOperationAction(ISD::BR_JT, MVT::Other, Expand);
  setLibcallImpl(RTLIB::FPEXT_F32_F64, RTLIB::impl___extendsfdf2);
  setLibcallImpl(RTLIB::SHL_I64, RTLIB::impl___ashldi3);
  setLibcallImpl(RTLIB::SRL_I64, RTLIB::impl___lshrdi3);
  setLibcallImpl(RTLIB::SRA_I64, RTLIB::impl___ashrdi3);
  MaxStoresPerMemcpy = MaxStoresPerMemcpyOptSize = 0;
  MaxStoresPerMemset = MaxStoresPerMemsetOptSize = 0;
  computeRegisterProperties(STI.getRegisterInfo());
  setSchedulingPreference(Sched::Source);
  setStackPointerRegisterToSaveRestore(Lamp::R30);
  setOperationAction(ISD::STACKSAVE, MVT::Other, Expand);
  setOperationAction(ISD::STACKRESTORE, MVT::Other, Expand);
  setBooleanContents(ZeroOrOneBooleanContent);
  // Lamp has no indirect branch instruction, so force switch lowering away
  // from jump tables.
  setMinimumJumpTableEntries(UINT_MAX);

  // `lamp-unknown-unknown` has no predefined runtime set; wire up the
  // integer div/rem libcalls to compiler-rt entry points.
  if (getLibcallImpl(RTLIB::MUL_I64) == RTLIB::Unsupported)
    setLibcallImpl(RTLIB::MUL_I64, RTLIB::impl___muldi3);
  if (getLibcallImpl(RTLIB::UDIV_I64) == RTLIB::Unsupported)
    setLibcallImpl(RTLIB::UDIV_I64, RTLIB::impl___udivdi3);
  if (getLibcallImpl(RTLIB::UREM_I64) == RTLIB::Unsupported)
    setLibcallImpl(RTLIB::UREM_I64, RTLIB::impl___umoddi3);
  if (getLibcallImpl(RTLIB::SDIV_I64) == RTLIB::Unsupported)
    setLibcallImpl(RTLIB::SDIV_I64, RTLIB::impl___divdi3);
  if (getLibcallImpl(RTLIB::SREM_I64) == RTLIB::Unsupported)
    setLibcallImpl(RTLIB::SREM_I64, RTLIB::impl___moddi3);
  if (getLibcallImpl(RTLIB::FPTOSINT_F64_I64) == RTLIB::Unsupported)
    setLibcallImpl(RTLIB::FPTOSINT_F64_I64, RTLIB::impl___fixdfdi);
  if (getLibcallImpl(RTLIB::FPTOUINT_F64_I64) == RTLIB::Unsupported)
    setLibcallImpl(RTLIB::FPTOUINT_F64_I64, RTLIB::impl___fixunsdfdi);
  if (getLibcallImpl(RTLIB::SINTTOFP_I64_F64) == RTLIB::Unsupported)
    setLibcallImpl(RTLIB::SINTTOFP_I64_F64, RTLIB::impl___floatdidf);
  if (getLibcallImpl(RTLIB::UINTTOFP_I64_F64) == RTLIB::Unsupported)
    setLibcallImpl(RTLIB::UINTTOFP_I64_F64, RTLIB::impl___floatundidf);
  if (getLibcallImpl(RTLIB::FPEXT_F64_F128) == RTLIB::Unsupported)
    setLibcallImpl(RTLIB::FPEXT_F64_F128, RTLIB::Unsupported);
  if (getLibcallImpl(RTLIB::FPROUND_F64_F32) == RTLIB::Unsupported)
    setLibcallImpl(RTLIB::FPROUND_F64_F32, RTLIB::impl___truncdfsf2);
  if (getLibcallImpl(RTLIB::FPTOSINT_F64_I32) == RTLIB::Unsupported)
    setLibcallImpl(RTLIB::FPTOSINT_F64_I32, RTLIB::impl___fixdfsi);
  if (getLibcallImpl(RTLIB::FPTOUINT_F64_I32) == RTLIB::Unsupported)
    setLibcallImpl(RTLIB::FPTOUINT_F64_I32, RTLIB::impl___fixunsdfsi);
  if (getLibcallImpl(RTLIB::SINTTOFP_I32_F64) == RTLIB::Unsupported)
    setLibcallImpl(RTLIB::SINTTOFP_I32_F64, RTLIB::impl___floatsidf);
  if (getLibcallImpl(RTLIB::UINTTOFP_I32_F64) == RTLIB::Unsupported)
    setLibcallImpl(RTLIB::UINTTOFP_I32_F64, RTLIB::impl___floatunsidf);
  if (getLibcallImpl(RTLIB::ADD_F64) == RTLIB::Unsupported)
    setLibcallImpl(RTLIB::ADD_F64, RTLIB::impl___adddf3);
  if (getLibcallImpl(RTLIB::SUB_F64) == RTLIB::Unsupported)
    setLibcallImpl(RTLIB::SUB_F64, RTLIB::impl___subdf3);
  if (getLibcallImpl(RTLIB::MUL_F64) == RTLIB::Unsupported)
    setLibcallImpl(RTLIB::MUL_F64, RTLIB::impl___muldf3);
  if (getLibcallImpl(RTLIB::DIV_F64) == RTLIB::Unsupported)
    setLibcallImpl(RTLIB::DIV_F64, RTLIB::impl___divdf3);
}

SDValue LampTargetLowering::LowerOperation(SDValue Op,
                                           SelectionDAG &DAG) const {
  SDLoc DL(Op);
  switch (Op.getOpcode()) {
  case ISD::DYNAMIC_STACKALLOC:
    return lowerDYNAMIC_STACKALLOC(Op, DAG);
  case ISD::VASTART:
    return lowerVASTART(Op, DAG);
  case ISD::MULHU:
    return lowerMULHU(Op, DAG);
  case ISD::MULHS:
    return lowerMULHS(Op, DAG);
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
  case ISD::GlobalTLSAddress:
    if (DAG.getTarget().useEmulatedTLS())
      return LowerToTLSEmulatedModel(cast<GlobalAddressSDNode>(Op), DAG);

    diagnoseUnsupported(DL, DAG, "TLS is not supported on the Lamp target");
    return DAG.getUNDEF(Op.getValueType());
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
  if (Constraint.size() == 1 && Constraint[0] == 'I')
    return C_Immediate;
  return TargetLowering::getConstraintType(Constraint);
}

TargetLowering::ConstraintWeight
LampTargetLowering::getSingleConstraintMatchWeight(
    AsmOperandInfo &Info, const char *Constraint) const {
  if (!Info.CallOperandVal)
    return CW_Default;

  if (*Constraint == 'I') {
    if (auto *CI = dyn_cast<ConstantInt>(Info.CallOperandVal))
      return isInt<32>(CI->getSExtValue()) ? CW_Constant : CW_Invalid;
    return CW_Invalid;
  }

  return TargetLowering::getSingleConstraintMatchWeight(Info, Constraint);
}

std::pair<unsigned, const TargetRegisterClass *>
LampTargetLowering::getRegForInlineAsmConstraint(
    const TargetRegisterInfo *TRI, StringRef Constraint, MVT VT) const {
  if (Constraint.size() == 1 && Constraint[0] == 'r')
    return std::make_pair(0U, &Lamp::GPRRegClass);
  return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
}

void LampTargetLowering::LowerAsmOperandForConstraint(
    SDValue Op, StringRef Constraint, std::vector<SDValue> &Ops,
    SelectionDAG &DAG) const {
  if (Constraint.size() == 1 && Constraint[0] == 'I') {
    if (auto *C = dyn_cast<ConstantSDNode>(Op)) {
      int64_t Val = C->getSExtValue();
      if (isInt<32>(Val)) {
        Ops.push_back(DAG.getTargetConstant(Val, SDLoc(Op), MVT::i32));
      }
    }
    return;
  }

  TargetLowering::LowerAsmOperandForConstraint(Op, Constraint, Ops, DAG);
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

  if (isPositionIndependent()) {
    if (isa<GlobalAddressSDNode>(Callee) || isa<ExternalSymbolSDNode>(Callee) ||
        Callee.getOpcode() == ISD::TargetGlobalAddress ||
        Callee.getOpcode() == ISD::TargetExternalSymbol) {
      diagnoseUnsupported(DL, DAG,
                          "PIC relocations are not supported on the Lamp target");
    }
  }

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeCallOperands(Outs, CC_Lamp);

  unsigned NumBytes = CCInfo.getStackSize();
  SmallVector<std::pair<Register, SDValue>, 8> RegsToPass;
  SDValue StackPtr = DAG.getRegister(Lamp::R30, MVT::i32);
  SDValue StackAdj;
  SDValue AdjustedSP;
  SDValue Glue;
  if (NumBytes != 0) {
    SDValue SP = DAG.getCopyFromReg(Chain, DL, Lamp::R30, MVT::i32);
    Chain = SP.getValue(1);
    StackAdj = DAG.getIntPtrConstant(NumBytes, DL);
    StackPtr = DAG.getNode(ISD::SUB, DL, MVT::i32, SP, StackAdj);
    AdjustedSP = StackPtr;
  }

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

    SDValue PtrOff = DAG.getIntPtrConstant(VA.getLocMemOffset(), DL);
    SDValue Ptr = DAG.getNode(ISD::ADD, DL, MVT::i32, StackPtr, PtrOff);
    Chain = DAG.getStore(Chain, DL, Arg, Ptr, MachinePointerInfo());
  }

  if (NumBytes != 0) {
    Chain = DAG.getCopyToReg(Chain, DL, Lamp::R30, AdjustedSP, Glue);
    Glue = Chain.getValue(1);
  }

  for (const auto &[Reg, Val] : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, Reg, Val, Glue);
    Glue = Chain.getValue(1);
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

  SmallVector<SDValue, 16> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);
  for (const auto &[Reg, Val] : RegsToPass) {
    (void)Val;
    Ops.push_back(DAG.getRegister(Reg, MVT::i32));
  }
  if (Glue.getNode())
    Ops.push_back(Glue);

  SDVTList CallVTs =
      Glue.getNode() ? DAG.getVTList(MVT::Other, MVT::Glue)
                     : DAG.getVTList(MVT::Other);
  SDValue Call = DAG.getNode(CallOpc, DL, CallVTs, Ops);
  Chain = SDValue(Call.getNode(), 0);
  if (Glue.getNode())
    Glue = SDValue(Call.getNode(), 1);
  else
    Glue = SDValue();

  if (NumBytes != 0) {
    SDValue RestoredSP =
        DAG.getNode(ISD::ADD, DL, MVT::i32, AdjustedSP, StackAdj);
    Chain = DAG.getCopyToReg(Chain, DL, Lamp::R30, RestoredSP, Glue);
    Glue = Chain.getValue(1);
  }

  SmallVector<CCValAssign, 16> RVLocs;
  CCState RetCCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), RVLocs,
                    *DAG.getContext());
  RetCCInfo.AnalyzeCallResult(Ins, RetCC_Lamp);

  for (CCValAssign &VA : RVLocs) {
    SDValue Ret =
        DAG.getCopyFromReg(Chain, DL, VA.getLocReg(), VA.getValVT(), Glue);
    Chain = Ret.getValue(1);
    Glue = Ret.getValue(2);
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

  if (IsVarArg) {
    unsigned Offset = CCInfo.getStackSize();
    int FI = MF.getFrameInfo().CreateFixedObject(4, Offset, true);
    MF.getInfo<LampMachineFunctionInfo>()->setVarArgsFrameIndex(FI);
  }

  for (CCValAssign &VA : ArgLocs) {
    if (VA.isRegLoc()) {
      Register VReg = RegInfo.createVirtualRegister(&Lamp::GPRRegClass);
      RegInfo.addLiveIn(VA.getLocReg(), VReg);
      SDValue Arg = DAG.getCopyFromReg(Chain, DL, VReg, VA.getValVT());
      InVals.push_back(Arg);
      Chain = Arg.getValue(1);
      continue;
    }

    int FI = MF.getFrameInfo().CreateFixedObject(VA.getLocVT().getStoreSize(),
                                                 VA.getLocMemOffset(), true);
    SDValue FIN = DAG.getFrameIndex(FI, MVT::i32);
    SDValue Ld = DAG.getLoad(VA.getValVT(), DL, Chain, FIN, MachinePointerInfo());
    InVals.push_back(Ld);
    Chain = Ld.getValue(1);
  }

  return Chain;
}

bool LampTargetLowering::CanLowerReturn(
    CallingConv::ID CallConv, MachineFunction &MF, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &Context,
    const Type *RetTy) const {
  (void)RetTy;
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, Context);
  return CCInfo.CheckReturn(Outs, RetCC_Lamp);
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

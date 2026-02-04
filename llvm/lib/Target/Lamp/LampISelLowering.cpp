#include "LampISelLowering.h"
#include "LampSubtarget.h"
#include "MCTargetDesc/LampMCTargetDesc.h"
#include "LampTargetMachine.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFrameInfo.h"

using namespace llvm;

#define DEBUG_TYPE "lamp-lower"

#include "LampGenCallingConv.inc"

LampTargetLowering::LampTargetLowering(const LampTargetMachine &TM,
                                       const LampSubtarget &STI)
    : TargetLowering(TM, STI) {
  addRegisterClass(MVT::i32, &Lamp::GPRRegClass);
  computeRegisterProperties(STI.getRegisterInfo());
  setStackPointerRegisterToSaveRestore(Lamp::R30);
  setBooleanContents(ZeroOrOneBooleanContent);
}

SDValue LampTargetLowering::LowerOperation(SDValue Op,
                                           SelectionDAG &DAG) const {
  return SDValue();
}

const char *LampTargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (Opcode) {
  case LampISD::RET:
    return "LampISD::RET";
  default:
    return nullptr;
  }
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

  for (unsigned i = 0; i < RVLocs.size(); ++i) {
    Chain = DAG.getCopyToReg(Chain, DL, RVLocs[i].getLocReg(), OutVals[i]);
  }

  SmallVector<SDValue, 4> RetOps;
  RetOps.push_back(Chain);
  return DAG.getNode(LampISD::RET, DL, MVT::Other, RetOps);
}

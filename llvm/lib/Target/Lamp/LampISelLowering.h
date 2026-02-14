#ifndef LLVM_LIB_TARGET_LAMP_LAMPISELLOWERING_H
#define LLVM_LIB_TARGET_LAMP_LAMPISELLOWERING_H

#include "llvm/CodeGen/TargetLowering.h"
#include <vector>

namespace llvm {

class LampSubtarget;
class LampTargetMachine;

namespace LampISD {
enum NodeType : unsigned {
  FIRST_NUMBER = ISD::BUILTIN_OP_END,
  CALL,
  CALLR,
  RET,
  FENCE,
  CAS,
  XADD,
  XCHG,
  LDAR,
  STLR,
};
} // namespace LampISD

class LampTargetLowering : public TargetLowering {
public:
  explicit LampTargetLowering(const LampTargetMachine &TM,
                              const LampSubtarget &STI);

  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;

  const char *getTargetNodeName(unsigned Opcode) const override;

  TargetLowering::ConstraintType
  getConstraintType(StringRef Constraint) const override;

  ConstraintWeight getSingleConstraintMatchWeight(
      AsmOperandInfo &Info, const char *Constraint) const override;

  std::pair<unsigned, const TargetRegisterClass *>
  getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                               StringRef Constraint, MVT VT) const override;

  void LowerAsmOperandForConstraint(SDValue Op, StringRef Constraint,
                                    std::vector<SDValue> &Ops,
                                    SelectionDAG &DAG) const override;

  SDValue LowerCall(CallLoweringInfo &CLI,
                    SmallVectorImpl<SDValue> &InVals) const override;

  SDValue LowerFormalArguments(SDValue Chain,
                               CallingConv::ID CallConv,
                               bool IsVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &DL,
                               SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;

  bool CanLowerReturn(CallingConv::ID CallConv, MachineFunction &MF,
                      bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      LLVMContext &Context,
                      const Type *RetTy) const override;

  SDValue LowerReturn(SDValue Chain,
                      CallingConv::ID CallConv,
                      bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals,
                      const SDLoc &DL,
                      SelectionDAG &DAG) const override;
};

} // namespace llvm

#endif

#ifndef LLVM_LIB_TARGET_LAMP_LAMPISELLOWERING_H
#define LLVM_LIB_TARGET_LAMP_LAMPISELLOWERING_H

#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {

class LampSubtarget;
class LampTargetMachine;

namespace LampISD {
enum NodeType : unsigned {
  FIRST_NUMBER = ISD::BUILTIN_OP_END,
  RET,
};
} // namespace LampISD

class LampTargetLowering : public TargetLowering {
public:
  explicit LampTargetLowering(const LampTargetMachine &TM,
                              const LampSubtarget &STI);

  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;

  const char *getTargetNodeName(unsigned Opcode) const override;

  SDValue LowerFormalArguments(SDValue Chain,
                               CallingConv::ID CallConv,
                               bool IsVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &DL,
                               SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;

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

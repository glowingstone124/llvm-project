#include "LampSelectionDAGInfo.h"
#include "LampISelLowering.h"
#include "MCTargetDesc/LampMCTargetDesc.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/SelectionDAG.h"

using namespace llvm;

static std::optional<uint32_t> getLampConstSize(SDValue Size) {
  auto *CSize = dyn_cast<ConstantSDNode>(Size);
  if (!CSize)
    return std::nullopt;
  uint64_t CopyLen = CSize->getZExtValue();
  if (CopyLen > UINT32_MAX)
    return std::nullopt;
  return static_cast<uint32_t>(CopyLen);
}

SDValue LampSelectionDAGInfo::EmitTargetCodeForMemcpy(
    SelectionDAG &DAG, const SDLoc &DL, SDValue Chain, SDValue Dst, SDValue Src,
    SDValue Size, Align Alignment, bool IsVolatile, bool AlwaysInline,
    MachinePointerInfo DstPtrInfo, MachinePointerInfo SrcPtrInfo) const {
  if (IsVolatile)
    return SDValue();

  std::optional<uint32_t> CopyLen = getLampConstSize(Size);
  if (!CopyLen)
    return SDValue();
  if (*CopyLen == 0)
    return Chain;

  MachineFunction &MF = DAG.getMachineFunction();
  auto Vol =
      IsVolatile ? MachineMemOperand::MOVolatile : MachineMemOperand::MONone;
  auto *DstMMO = MF.getMachineMemOperand(
      DstPtrInfo, MachineMemOperand::MOStore | Vol, *CopyLen, Alignment);
  auto *SrcMMO = MF.getMachineMemOperand(
      SrcPtrInfo, MachineMemOperand::MOLoad | Vol, *CopyLen, Alignment);

  SDValue Len = DAG.getTargetConstant(*CopyLen, DL, MVT::i32);
  SDValue Ops[] = {Dst, Src, Len, Chain};
  MachineSDNode *Node =
      DAG.getMachineNode(Lamp::MEMCPY, DL, DAG.getVTList(MVT::Other), Ops);
  DAG.setNodeMemRefs(Node, {DstMMO, SrcMMO});
  return SDValue(Node, 0);
}

SDValue LampSelectionDAGInfo::EmitTargetCodeForMemset(
    SelectionDAG &DAG, const SDLoc &DL, SDValue Chain, SDValue Dst,
    SDValue Byte, SDValue Size, Align Alignment, bool IsVolatile,
    bool AlwaysInline, MachinePointerInfo DstPtrInfo) const {
  if (IsVolatile)
    return SDValue();

  std::optional<uint32_t> SetLen = getLampConstSize(Size);
  if (!SetLen)
    return SDValue();
  if (*SetLen == 0)
    return Chain;

  MachineFunction &MF = DAG.getMachineFunction();
  auto Vol =
      IsVolatile ? MachineMemOperand::MOVolatile : MachineMemOperand::MONone;
  auto *DstMMO = MF.getMachineMemOperand(
      DstPtrInfo, MachineMemOperand::MOStore | Vol, *SetLen, Alignment);

  if (Byte.getValueType() != MVT::i32)
    Byte = DAG.getZExtOrTrunc(Byte, DL, MVT::i32);

  SDValue Len = DAG.getTargetConstant(*SetLen, DL, MVT::i32);
  SDValue Ops[] = {Dst, Byte, Len, Chain};
  MachineSDNode *Node =
      DAG.getMachineNode(Lamp::MEMSET, DL, DAG.getVTList(MVT::Other), Ops);
  DAG.setNodeMemRefs(Node, {DstMMO});
  return SDValue(Node, 0);
}

bool LampSelectionDAGInfo::isTargetMemoryOpcode(unsigned Opcode) const {
  switch (Opcode) {
  case Lamp::MEMSET:
  case Lamp::MEMCPY:
  case LampISD::CAS:
  case LampISD::XADD:
  case LampISD::XCHG:
  case LampISD::LDAR:
  case LampISD::STLR:
    return true;
  default:
    return false;
  }
}

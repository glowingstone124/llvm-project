#ifndef LLVM_LIB_TARGET_LAMP_LAMPSELECTIONDAGINFO_H
#define LLVM_LIB_TARGET_LAMP_LAMPSELECTIONDAGINFO_H

#include "llvm/CodeGen/SelectionDAGTargetInfo.h"

namespace llvm {

class LampSelectionDAGInfo : public SelectionDAGTargetInfo {
public:
  LampSelectionDAGInfo() = default;

  bool isTargetMemoryOpcode(unsigned Opcode) const override;
};

} // namespace llvm

#endif

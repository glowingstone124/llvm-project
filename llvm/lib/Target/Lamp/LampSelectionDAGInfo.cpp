#include "LampSelectionDAGInfo.h"
#include "LampISelLowering.h"

using namespace llvm;

bool LampSelectionDAGInfo::isTargetMemoryOpcode(unsigned Opcode) const {
  switch (Opcode) {
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

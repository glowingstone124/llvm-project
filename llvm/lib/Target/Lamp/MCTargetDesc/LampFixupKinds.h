#ifndef LLVM_LIB_TARGET_LAMP_MCTARGETDESC_LAMPFIXUPKINDS_H
#define LLVM_LIB_TARGET_LAMP_MCTARGETDESC_LAMPFIXUPKINDS_H

#include "llvm/MC/MCFixup.h"

namespace llvm {
namespace Lamp {
enum Fixups {
  fixup_lamp_32 = FirstTargetFixupKind,
  fixup_lamp_pc32,

  LastTargetFixupKind,
  NumTargetFixupKinds = LastTargetFixupKind - FirstTargetFixupKind
};
} // namespace Lamp
} // namespace llvm

#endif

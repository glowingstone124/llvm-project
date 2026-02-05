#ifndef LLVM_LIB_TARGET_LAMP_MCTARGETDESC_LAMPMCTARGETDESC_H
#define LLVM_LIB_TARGET_LAMP_MCTARGETDESC_LAMPMCTARGETDESC_H

#include "llvm/Support/DataTypes.h"
#include <memory>

namespace llvm {
class MCAsmBackend;
class MCCodeEmitter;
class MCContext;
class MCInstrInfo;
class MCObjectTargetWriter;
class MCRegisterInfo;
class MCSubtargetInfo;
class MCTargetOptions;
class Target;
class Triple;

MCCodeEmitter *createLampMCCodeEmitter(const MCInstrInfo &MCII, MCContext &Ctx);
MCAsmBackend *createLampMCAsmBackend(const Target &T, const MCSubtargetInfo &STI,
                                     const MCRegisterInfo &MRI,
                                     const llvm::MCTargetOptions &Options);
std::unique_ptr<MCObjectTargetWriter> createLampELFObjectWriter(uint8_t OSABI);

} // namespace llvm

#define GET_REGINFO_ENUM
#include "LampGenRegisterInfo.inc"

#define GET_INSTRINFO_ENUM
#include "LampGenInstrInfo.inc"

#endif

#include "MCTargetDesc/LampMCTargetDesc.h"
#include "MCTargetDesc/LampFixupKinds.h"

#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

namespace {
class LampELFObjectWriter : public MCELFObjectTargetWriter {
public:
  explicit LampELFObjectWriter(uint8_t OSABI)
      : MCELFObjectTargetWriter(/*Is64Bit=*/false,
                                OSABI,
                                ELF::EM_LAMP,
                                /*HasRelocationAddend=*/true) {}

  ~LampELFObjectWriter() override = default;

protected:
  unsigned getRelocType(const MCFixup &Fixup, const MCValue &Target,
                        bool IsPCRel) const override {
    (void)Target;
    (void)IsPCRel;

    switch (static_cast<unsigned>(Fixup.getKind())) {
    case Lamp::fixup_lamp_32:
    case FK_Data_4:
      return ELF::R_LAMP_32;
    case Lamp::fixup_lamp_pc32:
      return ELF::R_LAMP_PC32;
    default:
      llvm_unreachable("LampELFObjectWriter: unsupported fixup kind");
    }
  }
};
} // namespace

std::unique_ptr<MCObjectTargetWriter>
llvm::createLampELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<LampELFObjectWriter>(OSABI);
}

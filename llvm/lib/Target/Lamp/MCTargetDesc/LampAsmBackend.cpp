#include "MCTargetDesc/LampMCTargetDesc.h"
#include "MCTargetDesc/LampFixupKinds.h"

#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
class LampAsmBackend : public MCAsmBackend {
  uint8_t OSABI;

public:
  explicit LampAsmBackend(uint8_t OSABI)
      : MCAsmBackend(llvm::endianness::little), OSABI(OSABI) {}

  ~LampAsmBackend() override = default;

  MCFixupKindInfo getFixupKindInfo(MCFixupKind Kind) const override {
    static const MCFixupKindInfo Infos[Lamp::NumTargetFixupKinds] = {
        // name              offset bits flags
        {"fixup_lamp_32",    0, 32, 0},
        {"fixup_lamp_pc32",  0, 32, 0},
    };
    static_assert(std::size(Infos) == Lamp::NumTargetFixupKinds,
                  "Lamp fixup info table is incomplete");

    if (Kind < FirstTargetFixupKind)
      return MCAsmBackend::getFixupKindInfo(Kind);

    assert(unsigned(Kind - FirstTargetFixupKind) < Lamp::NumTargetFixupKinds &&
           "Invalid fixup kind");
    return Infos[Kind - FirstTargetFixupKind];
  }

  void applyFixup(const MCFragment &F, const MCFixup &Fixup,
                  const MCValue &Target, uint8_t *Data, uint64_t Value,
                  bool IsResolved) override {
    maybeAddReloc(F, Fixup, Target, Value, IsResolved);

    MCFixupKindInfo Info = getFixupKindInfo(Fixup.getKind());
    if (!Value)
      return;

    Value <<= Info.TargetOffset;
    const unsigned NumBytes = alignTo(Info.TargetSize + Info.TargetOffset, 8) / 8;

    for (unsigned I = 0; I != NumBytes; ++I)
      Data[I] |= static_cast<uint8_t>((Value >> (I * 8)) & 0xff);
  }

  std::unique_ptr<MCObjectTargetWriter> createObjectTargetWriter() const override {
    return createLampELFObjectWriter(OSABI);
  }

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override {
    (void)STI;
    if ((Count % 8) != 0)
      return false;

    while (Count) {
      const char HaltLE[8] = {0, 0, 0, 0, 0, 0, 0, 0x05};
      OS.write(HaltLE, 8);
      Count -= 8;
    }
    return true;
  }
};
} // namespace

MCAsmBackend *llvm::createLampMCAsmBackend(const Target &T,
                                           const MCSubtargetInfo &STI,
                                           const MCRegisterInfo &MRI,
                                           const MCTargetOptions &Options) {
  (void)T;
  (void)MRI;
  (void)Options;
  const uint8_t OSABI =
      MCELFObjectTargetWriter::getOSABI(STI.getTargetTriple().getOS());
  return new LampAsmBackend(OSABI);
}

//===- Lamp.cpp -----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Symbols.h"
#include "Target.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::ELF;
using namespace llvm::support::endian;
using namespace lld;
using namespace lld::elf;

namespace {
class Lamp final : public TargetInfo {
public:
  Lamp(Ctx &);
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  void relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const override;
};
} // namespace

Lamp::Lamp(Ctx &ctx) : TargetInfo(ctx) {
  // HALT
  trapInstr = {0x00, 0x00, 0x00, 0x00};
}

RelExpr Lamp::getRelExpr(RelType type, const Symbol &s,
                         const uint8_t *loc) const {
  (void)s;
  (void)loc;

  switch (type) {
  case R_LAMP_PC32:
    return R_PC;
  case R_LAMP_NONE:
    return R_NONE;
  case R_LAMP_32:
  default:
    return R_ABS;
  }
}

void Lamp::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  switch (rel.type) {
  case R_LAMP_32:
  case R_LAMP_PC32:
    checkIntUInt(ctx, loc, val, 32, rel);
    write32le(loc, val);
    break;
  case R_LAMP_NONE:
    break;
  default:
    Err(ctx) << getErrorLoc(ctx, loc) << "unrecognized relocation " << rel.type;
  }
}

void elf::setLampTargetInfo(Ctx &ctx) { ctx.target.reset(new Lamp(ctx)); }

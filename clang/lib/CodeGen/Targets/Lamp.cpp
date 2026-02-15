//===- Lamp.cpp -----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABIInfoImpl.h"
#include "TargetInfo.h"

using namespace clang;
using namespace clang::CodeGen;

namespace {

class LampABIInfo : public DefaultABIInfo {
  llvm::Type *I32Ty() const { return llvm::Type::getInt32Ty(getVMContext()); }
  llvm::Type *I64Ty() const { return llvm::Type::getInt64Ty(getVMContext()); }

public:
  LampABIInfo(CodeGenTypes &CGT) : DefaultABIInfo(CGT) {}

  ABIArgInfo classifyArgumentType(QualType Ty) const {
    Ty = useFirstFieldIfTransparentUnion(Ty);

    if (isAggregateTypeForABI(Ty)) {
      CGCXXABI::RecordArgABI RAA = getRecordArgABI(Ty, getCXXABI());
      return getNaturalAlignIndirect(Ty, getDataLayout().getAllocaAddrSpace(),
                                     RAA == CGCXXABI::RAA_DirectInMemory);
    }

    if (const auto *ED = Ty->getAsEnumDecl())
      Ty = ED->getIntegerType();

    if (Ty->isSpecificBuiltinType(BuiltinType::Float))
      return ABIArgInfo::getDirect(I32Ty());
    if (Ty->isSpecificBuiltinType(BuiltinType::Double))
      return ABIArgInfo::getDirect(I64Ty());

    if (const auto *EIT = Ty->getAs<BitIntType>())
      if (EIT->getNumBits() > 64)
        return getNaturalAlignIndirect(Ty, getDataLayout().getAllocaAddrSpace());

    if (getContext().getTypeSize(Ty) > 64)
      return getNaturalAlignIndirect(Ty, getDataLayout().getAllocaAddrSpace());

    return isPromotableIntegerTypeForABI(Ty)
               ? ABIArgInfo::getExtend(Ty, CGT.ConvertType(Ty))
               : ABIArgInfo::getDirect();
  }

  ABIArgInfo classifyReturnType(QualType RetTy) const {
    if (RetTy->isVoidType())
      return ABIArgInfo::getIgnore();

    if (isAggregateTypeForABI(RetTy))
      return getNaturalAlignIndirect(RetTy, getDataLayout().getAllocaAddrSpace());

    if (const auto *ED = RetTy->getAsEnumDecl())
      RetTy = ED->getIntegerType();

    if (RetTy->isSpecificBuiltinType(BuiltinType::Float))
      return ABIArgInfo::getDirect(I32Ty());

    if (RetTy->isSpecificBuiltinType(BuiltinType::Double))
      return getNaturalAlignIndirect(RetTy,
                                     getDataLayout().getAllocaAddrSpace());

    if (const auto *EIT = RetTy->getAs<BitIntType>())
      if (EIT->getNumBits() > 32)
        return getNaturalAlignIndirect(RetTy,
                                       getDataLayout().getAllocaAddrSpace());

    if (getContext().getTypeSize(RetTy) > 32)
      return getNaturalAlignIndirect(RetTy,
                                     getDataLayout().getAllocaAddrSpace());

    return isPromotableIntegerTypeForABI(RetTy) ? ABIArgInfo::getExtend(RetTy)
                                                 : ABIArgInfo::getDirect();
  }

  void computeInfo(CGFunctionInfo &FI) const override {
    if (!getCXXABI().classifyReturnType(FI))
      FI.getReturnInfo() = classifyReturnType(FI.getReturnType());
    for (auto &I : FI.arguments())
      I.info = classifyArgumentType(I.type);
  }

  RValue EmitVAArg(CodeGenFunction &CGF, Address VAListAddr, QualType Ty,
                   AggValueSlot Slot) const override {
    return CGF.EmitLoadOfAnyValue(
        CGF.MakeAddrLValue(EmitVAArgInstr(CGF, VAListAddr, Ty,
                                          classifyArgumentType(Ty)),
                           Ty),
        Slot);
  }
};

class LampTargetCodeGenInfo : public TargetCodeGenInfo {
public:
  LampTargetCodeGenInfo(CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<LampABIInfo>(CGT)) {}
};

} // namespace

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createLampTargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<LampTargetCodeGenInfo>(CGM.getTypes());
}


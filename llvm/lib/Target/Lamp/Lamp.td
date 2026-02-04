include "llvm/Target/Target.td"

include "LampRegisterInfo.td"
include "LampInstrInfo.td"
defm : RemapAllTargetPseudoPointerOperands<PtrRC>;
def Lamp : Target {
  let InstructionSet = LampInstrInfo;
}

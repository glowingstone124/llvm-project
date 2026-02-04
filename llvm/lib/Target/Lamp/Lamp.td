include "llvm/Target/Target.td"

include "LampRegisterInfo.td"
include "LampCallingConv.td"
include "LampInstrInfo.td"

class Proc<string Name, list<SubtargetFeature> Features>
    : Processor<Name, NoItineraries, Features>;

def : Proc<"generic", []>;

defm : RemapAllTargetPseudoPointerOperands<PtrRC>;

def LampInstrInfo : InstrInfo;

def LampAsmWriter : AsmWriter {
  string AsmWriterClassName = "InstPrinter";
}

def Lamp : Target {
  let InstructionSet = LampInstrInfo;
}

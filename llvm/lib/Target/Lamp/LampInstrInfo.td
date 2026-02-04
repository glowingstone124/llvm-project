def LampInstrInfo : InstrInfo;

def simm32 : Operand<i32>;

class LampInst : Instruction {
  let Namespace = "Lamp";

  let OutOperandList = (outs);
  let InOperandList  = (ins);
  let Pattern = [];
}

def RET : LampInst {
  let AsmString = "ret";
  let isTerminator = 1;
  let isReturn = 1;
  let hasSideEffects = 1;
}
def MOVI : LampInst {
  let OutOperandList = (outs GPR:$dst);
  let InOperandList  = (ins simm32:$imm);
  let AsmString = "movi $dst, $imm";
}

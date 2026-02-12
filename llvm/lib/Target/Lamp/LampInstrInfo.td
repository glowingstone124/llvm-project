def Lamp_i32imm : Operand<i32> {
  let PrintMethod = "printImm";
}

def brtarget : Operand<OtherVT> {
  let PrintMethod = "printOperand";
}

def simm32 : PatLeaf<(imm), [{ return isInt<32>(N->getSExtValue()); }]>;

class LampInstBase : Instruction {
  bits<64> Inst;
  let Size = 8;
  let Namespace = "Lamp";
}

class LampInstNone<string Mn, bits<8> Op> : LampInstBase {
  let OutOperandList = (outs);
  let InOperandList  = (ins);
  let AsmString = Mn;
  let Inst{63-56} = Op;
}

class LampInstI<string Mn, bits<8> Op> : LampInstBase {
  let OutOperandList = (outs);
  let InOperandList  = (ins Lamp_i32imm:$imm);
  let AsmString = !strconcat(Mn, " $imm");
  let Inst{63-56} = Op;
}

class LampInstTarget<string Mn, bits<8> Op> : LampInstBase {
  let OutOperandList = (outs);
  let InOperandList  = (ins brtarget:$target);
  let AsmString = !strconcat(Mn, " $target");
  let Inst{63-56} = Op;
}

class LampInstRsTarget<string Mn, bits<8> Op> : LampInstBase {
  let OutOperandList = (outs);
  let InOperandList  = (ins GPR:$rs, brtarget:$target);
  let AsmString = !strconcat(Mn, " $rs, $target");
  let Inst{63-56} = Op;
}

class LampCallInst<string Mn, bits<8> Op> : LampInstBase {
  let OutOperandList = (outs);
  let InOperandList = (ins brtarget:$target);
  let AsmString = !strconcat(Mn, " $target");
  let Inst{63-56} = Op;
}

class LampCallRegInst<string Mn, bits<8> Op> : LampInstBase {
  let OutOperandList = (outs);
  let InOperandList = (ins GPR:$rs);
  let AsmString = !strconcat(Mn, " $rs");
  let Inst{63-56} = Op;
}

class LampInstRd<string Mn, bits<8> Op> : LampInstBase {
  let OutOperandList = (outs GPR:$rd);
  let InOperandList  = (ins);
  let AsmString = !strconcat(Mn, " $rd");
  let Inst{63-56} = Op;
}

class LampInstRdImm<string Mn, bits<8> Op> : LampInstBase {
  let OutOperandList = (outs GPR:$rd);
  let InOperandList  = (ins Lamp_i32imm:$imm);
  let AsmString = !strconcat(Mn, " $rd, $imm");
  let Inst{63-56} = Op;
}

class LampInstRdRs<string Mn, bits<8> Op> : LampInstBase {
  let OutOperandList = (outs GPR:$rd);
  let InOperandList  = (ins GPR:$rs1);
  let AsmString = !strconcat(Mn, " $rd, $rs1");
  let Inst{63-56} = Op;
}

class LampInstRdRsRs<string Mn, bits<8> Op> : LampInstBase {
  let OutOperandList = (outs GPR:$rd);
  let InOperandList  = (ins GPR:$rs1, GPR:$rs2);
  let AsmString = !strconcat(Mn, " $rd, $rs1, $rs2");
  let Inst{63-56} = Op;
}

class LampInstRsRs<string Mn, bits<8> Op> : LampInstBase {
  let OutOperandList = (outs);
  let InOperandList  = (ins GPR:$rs, GPR:$rs1);
  let AsmString = !strconcat(Mn, " $rs, $rs1");
  let Inst{63-56} = Op;
}

class LampInstRsImm<string Mn, bits<8> Op> : LampInstBase {
  let OutOperandList = (outs);
  let InOperandList  = (ins GPR:$rs, Lamp_i32imm:$imm);
  let AsmString = !strconcat(Mn, " $rs, $imm");
  let Inst{63-56} = Op;
}

class LampInstRdRsImm<string Mn, bits<8> Op> : LampInstBase {
  let OutOperandList = (outs GPR:$rd);
  let InOperandList  = (ins GPR:$rs1, Lamp_i32imm:$imm);
  let AsmString = !strconcat(Mn, " $rd, $rs1, $imm");
  let Inst{63-56} = Op;
}

class LampInstRsRsImm<string Mn, bits<8> Op> : LampInstBase {
  let OutOperandList = (outs);
  let InOperandList  = (ins GPR:$rs, GPR:$rs1, Lamp_i32imm:$imm);
  let AsmString = !strconcat(Mn, " $rs, $rs1, $imm");
  let Inst{63-56} = Op;
}

class LampInstRdRsRsImm<string Mn, bits<8> Op> : LampInstBase {
  let OutOperandList = (outs GPR:$rd);
  let InOperandList  = (ins GPR:$rs1, GPR:$rs2, Lamp_i32imm:$imm);
  let AsmString = !strconcat(Mn, " $rd, $rs1, $rs2, $imm");
  let Inst{63-56} = Op;
}

class LampInstRdRsRsRsImm<string Mn, bits<8> Op> : LampInstBase {
  let OutOperandList = (outs GPR:$rd);
  let InOperandList  = (ins GPR:$rs1, GPR:$rs2, GPR:$rs3, Lamp_i32imm:$imm);
  let AsmString = !strconcat(Mn, " $rd, $rs1, $rs2, $rs3, $imm");
  let Inst{63-56} = Op;
}

let isAsCheapAsAMove = 1 in
def MOVI : LampInstRdImm<"movi", 0x15>;

def MOV : LampInstRdRs<"mov", 0x14>;

def ADD : LampInstRdRsRs<"add", 0x01>;
def SUB : LampInstRdRsRs<"sub", 0x02> {
  let Defs = [FLAGS];
}
def MUL : LampInstRdRsRs<"mul", 0x03>;
def DIV : LampInstRdRsRs<"div", 0x04>;
def MOD : LampInstRdRsRs<"mod", 0x1C>;
def AND : LampInstRdRsRs<"and", 0x1D>;
def OR  : LampInstRdRsRs<"or",  0x1E>;
def XOR : LampInstRdRsRs<"xor", 0x1F>;
def SHL : LampInstRdRsRs<"shl", 0x21>;
def SHR : LampInstRdRsRs<"shr", 0x22>;
def SAR : LampInstRdRsRs<"sar", 0x23>;

def CMP  : LampInstRsRs<"cmp", 0x12> {
  let Defs = [FLAGS];
  let hasSideEffects = 1;
}
def NOT  : LampInstRdRs<"not", 0x20>;
def IN   : LampInstRdRs<"in",  0x18>;
def OUT  : LampInstRdRs<"out", 0x19>;
def FNEG : LampInstRdRs<"fneg", 0x2F>;
def FABS : LampInstRdRs<"fabs", 0x30>;
def FSQRT: LampInstRdRs<"fsqrt", 0x31>;
def FCMP : LampInstRdRs<"fcmp", 0x32>;
def ITOF : LampInstRdRs<"itof", 0x33>;
def FTOI : LampInstRdRs<"ftoi", 0x34>;

let mayLoad = 1 in {
  def LOAD    : LampInstRdRsImm<"load",    0x0C>;
  def LOAD32  : LampInstRdRsImm<"load32",  0x0D>;
}
let mayStore = 1 in {
  def STORE   : LampInstRsRsImm<"store",   0x0F>;
  def STORE32 : LampInstRsRsImm<"store32", 0x10>;
}
def MEMSET  : LampInstRdRsImm<"memset",  0x16>;
def MEMCPY  : LampInstRdRsImm<"memcpy",  0x17>;
def FLOAD32 : LampInstRdRsImm<"fload32", 0x35>;
def FSTORE32: LampInstRdRsImm<"fstore32",0x36>;
def ADDI    : LampInstRdRsImm<"addi",    0x38>;
def SUBI    : LampInstRdRsImm<"subi",    0x39> {
  let Defs = [FLAGS];
}
def ANDI    : LampInstRdRsImm<"andi",    0x3A>;
def ORI     : LampInstRdRsImm<"ori",     0x3B>;
def XORI    : LampInstRdRsImm<"xori",    0x3C>;
def SHLI    : LampInstRdRsImm<"shli",    0x3D>;
def SHRI    : LampInstRdRsImm<"shri",    0x3E>;

def LOADX32  : LampInstRdRsRsImm<"loadx32",  0x0E>;
def STOREX32 : LampInstRdRsRsImm<"storex32", 0x11>;

def CMPI : LampInstRsImm<"cmpi", 0x13> {
  let Defs = [FLAGS];
  let hasSideEffects = 1;
}

def FADD : LampInstRdRsRs<"fadd", 0x2B>;
def FSUB : LampInstRdRsRs<"fsub", 0x2C>;
def FMUL : LampInstRdRsRs<"fmul", 0x2D>;
def FDIV : LampInstRdRsRs<"fdiv", 0x2E>;

def INC  : LampInstRd<"inc", 0x37>;
def PUSH : LampInstRd<"push", 0x08>;
def POP  : LampInstRd<"pop", 0x09>;
def INT  : LampInstRd<"int", 0x1A>;
def IRET : LampInstNone<"iret", 0x1B>;
def CPUID : LampInstRd<"cpuid", 0x48> { let hasSideEffects = 1; }
def PAUSE : LampInstNone<"pause", 0x45> { let hasSideEffects = 1; }
def FENCE : LampInstNone<"fence", 0x44> { let hasSideEffects = 1; }
def STARTAP : LampInstRsRsImm<"startap", 0x46> { let hasSideEffects = 1; }
def IPI : LampInstRsRs<"ipi", 0x47> { let hasSideEffects = 1; }

let mayLoad = 1, mayStore = 1 in {
  def CAS  : LampInstRdRsRsRsImm<"cas",  0x3F>;
  def XADD : LampInstRdRsRsImm<"xadd", 0x40>;
  def XCHG : LampInstRdRsRsImm<"xchg", 0x41>;
}
let mayLoad = 1 in
def LDAR : LampInstRdRsImm<"ldar", 0x42>;
let mayStore = 1 in
def STLR : LampInstRsRsImm<"stlr", 0x43>;

let isTerminator = 1, isBarrier = 1 in {
  def HALT : LampInstNone<"halt", 0x05>;
  let isReturn = 1 in
  def RET  : LampInstNone<"ret", 0x0B> {
    let Uses = [R0];
  }
}

let isTerminator = 1, isBranch = 1 in {
  def JMP  : LampInstTarget<"jmp", 0x06>;
  def RJMP : LampInstTarget<"rjmp", 0x4A>;
  def JZ   : LampInstRsTarget<"jz", 0x07>;
  def RJZ  : LampInstRsTarget<"rjz", 0x4C>;
  def JNZ  : LampInstRsTarget<"jnz", 0x24>;
  def RJNZ : LampInstRsTarget<"rjnz", 0x4D>;
  def JG   : LampInstTarget<"jg", 0x25> { let Uses = [FLAGS]; }
  def JGE  : LampInstTarget<"jge", 0x26> { let Uses = [FLAGS]; }
  def JL   : LampInstTarget<"jl", 0x27> { let Uses = [FLAGS]; }
  def JLE  : LampInstTarget<"jle", 0x28> { let Uses = [FLAGS]; }
  def JC   : LampInstTarget<"jc", 0x29> { let Uses = [FLAGS]; }
  def JNC  : LampInstTarget<"jnc", 0x2A> { let Uses = [FLAGS]; }
}

let isCall = 1 in
def CALL : LampCallInst<"call", 0x0A> {
  let Uses = [R0, R1, R2, R3, R4, R5, R6, R7];
  // No callee-saved regs; model call as clobbering all general regs
  // except SP/FLAGS so RA doesn't keep values live across calls.
  let Defs = [R0, R1, R2, R3, R4, R5, R6, R7,
              R8, R9, R10, R11, R12, R13, R14, R15,
              R16, R17, R18, R19, R20, R21, R22, R23,
              R24, R25, R26, R27, R28, R29];
}

let isCall = 1 in
def RCALL : LampCallInst<"rcall", 0x4B> {
  let Uses = [R0, R1, R2, R3, R4, R5, R6, R7];
  let Defs = [R0, R1, R2, R3, R4, R5, R6, R7,
              R8, R9, R10, R11, R12, R13, R14, R15,
              R16, R17, R18, R19, R20, R21, R22, R23,
              R24, R25, R26, R27, R28, R29];
}

let isCall = 1 in
def CALLR : LampCallRegInst<"callr", 0x49> {
  let Uses = [R0, R1, R2, R3, R4, R5, R6, R7];
  let Defs = [R0, R1, R2, R3, R4, R5, R6, R7,
              R8, R9, R10, R11, R12, R13, R14, R15,
              R16, R17, R18, R19, R20, R21, R22, R23,
              R24, R25, R26, R27, R28, R29];
}

def : Pat<(add i32:$lhs, i32:$rhs), (ADD GPR:$lhs, GPR:$rhs)>;
def : Pat<(sub i32:$lhs, i32:$rhs), (SUB GPR:$lhs, GPR:$rhs)>;
def : Pat<(mul i32:$lhs, i32:$rhs), (MUL GPR:$lhs, GPR:$rhs)>;
def : Pat<(sdiv i32:$lhs, i32:$rhs), (DIV GPR:$lhs, GPR:$rhs)>;
def : Pat<(udiv i32:$lhs, i32:$rhs), (DIV GPR:$lhs, GPR:$rhs)>;
def : Pat<(srem i32:$lhs, i32:$rhs), (MOD GPR:$lhs, GPR:$rhs)>;
def : Pat<(urem i32:$lhs, i32:$rhs), (MOD GPR:$lhs, GPR:$rhs)>;
def : Pat<(add i32:$lhs, simm32:$imm), (ADDI GPR:$lhs, simm32:$imm)>;
def : Pat<(sub i32:$lhs, simm32:$imm), (SUBI GPR:$lhs, simm32:$imm)>;
def : Pat<(and i32:$lhs, i32:$rhs), (AND GPR:$lhs, GPR:$rhs)>;
def : Pat<(or  i32:$lhs, i32:$rhs), (OR  GPR:$lhs, GPR:$rhs)>;
def : Pat<(xor i32:$lhs, i32:$rhs), (XOR GPR:$lhs, GPR:$rhs)>;
def : Pat<(and i32:$lhs, simm32:$imm), (ANDI GPR:$lhs, simm32:$imm)>;
def : Pat<(or  i32:$lhs, simm32:$imm), (ORI  GPR:$lhs, simm32:$imm)>;
def : Pat<(xor i32:$lhs, simm32:$imm), (XORI GPR:$lhs, simm32:$imm)>;
def : Pat<(mul i32:$lhs, simm32:$imm), (MUL GPR:$lhs, (MOVI simm32:$imm))>;
def : Pat<(sdiv i32:$lhs, simm32:$imm), (DIV GPR:$lhs, (MOVI simm32:$imm))>;
def : Pat<(udiv i32:$lhs, simm32:$imm), (DIV GPR:$lhs, (MOVI simm32:$imm))>;
def : Pat<(srem i32:$lhs, simm32:$imm), (MOD GPR:$lhs, (MOVI simm32:$imm))>;
def : Pat<(urem i32:$lhs, simm32:$imm), (MOD GPR:$lhs, (MOVI simm32:$imm))>;
def : Pat<(shl i32:$lhs, i32:$rhs), (SHL GPR:$lhs, GPR:$rhs)>;
def : Pat<(srl i32:$lhs, i32:$rhs), (SHR GPR:$lhs, GPR:$rhs)>;
def : Pat<(sra i32:$lhs, i32:$rhs), (SAR GPR:$lhs, GPR:$rhs)>;
def : Pat<(shl i32:$lhs, simm32:$imm), (SHLI GPR:$lhs, simm32:$imm)>;
def : Pat<(srl i32:$lhs, simm32:$imm), (SHRI GPR:$lhs, simm32:$imm)>;
def : Pat<(i32 simm32:$imm), (MOVI simm32:$imm)>;

def : Pat<(seteq i32:$lhs, i32:$rhs),
          (XORI
            (SHRI
              (OR
                (SUB GPR:$lhs, GPR:$rhs),
                (SUB (MOVI 0), (SUB GPR:$lhs, GPR:$rhs))),
              31),
            1)>;

def : Pat<(setne i32:$lhs, i32:$rhs),
          (SHRI
            (OR
              (SUB GPR:$lhs, GPR:$rhs),
              (SUB (MOVI 0), (SUB GPR:$lhs, GPR:$rhs))),
            31)>;

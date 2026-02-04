def Lamp_i32imm : Operand<i32> {
  let PrintMethod = "printImm";
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

class LampInstRdRsImm<string Mn, bits<8> Op> : LampInstBase {
  let OutOperandList = (outs GPR:$rd);
  let InOperandList  = (ins GPR:$rs1, Lamp_i32imm:$imm);
  let AsmString = !strconcat(Mn, " $rd, $rs1, $imm");
  let Inst{63-56} = Op;
}

class LampInstRdRsRsImm<string Mn, bits<8> Op> : LampInstBase {
  let OutOperandList = (outs GPR:$rd);
  let InOperandList  = (ins GPR:$rs1, GPR:$rs2, Lamp_i32imm:$imm);
  let AsmString = !strconcat(Mn, " $rd, $rs1, $rs2, $imm");
  let Inst{63-56} = Op;
}

let isAsCheapAsAMove = 1 in
def MOVI : LampInstRdImm<"movi", 0x15>;

def MOV : LampInstRdRs<"mov", 0x14>;

def ADD : LampInstRdRsRs<"add", 0x01>;
def SUB : LampInstRdRsRs<"sub", 0x02>;
def MUL : LampInstRdRsRs<"mul", 0x03>;
def DIV : LampInstRdRsRs<"div", 0x04>;
def MOD : LampInstRdRsRs<"mod", 0x1C>;
def AND : LampInstRdRsRs<"and", 0x1D>;
def OR  : LampInstRdRsRs<"or",  0x1E>;
def XOR : LampInstRdRsRs<"xor", 0x1F>;
def SHL : LampInstRdRsRs<"shl", 0x21>;
def SHR : LampInstRdRsRs<"shr", 0x22>;
def SAR : LampInstRdRsRs<"sar", 0x23>;

def CMP  : LampInstRdRs<"cmp", 0x12>;
def NOT  : LampInstRdRs<"not", 0x20>;
def IN   : LampInstRdRs<"in",  0x18>;
def OUT  : LampInstRdRs<"out", 0x19>;
def FNEG : LampInstRdRs<"fneg", 0x2F>;
def FABS : LampInstRdRs<"fabs", 0x30>;
def FSQRT: LampInstRdRs<"fsqrt", 0x31>;
def FCMP : LampInstRdRs<"fcmp", 0x32>;
def ITOF : LampInstRdRs<"itof", 0x33>;
def FTOI : LampInstRdRs<"ftoi", 0x34>;

def LOAD    : LampInstRdRsImm<"load",    0x0C>;
def LOAD32  : LampInstRdRsImm<"load32",  0x0D>;
def STORE   : LampInstRdRsImm<"store",   0x0F>;
def STORE32 : LampInstRdRsImm<"store32", 0x10>;
def MEMSET  : LampInstRdRsImm<"memset",  0x16>;
def MEMCPY  : LampInstRdRsImm<"memcpy",  0x17>;
def FLOAD32 : LampInstRdRsImm<"fload32", 0x35>;
def FSTORE32: LampInstRdRsImm<"fstore32",0x36>;
def ADDI    : LampInstRdRsImm<"addi",    0x38>;
def SUBI    : LampInstRdRsImm<"subi",    0x39>;
def ANDI    : LampInstRdRsImm<"andi",    0x3A>;
def ORI     : LampInstRdRsImm<"ori",     0x3B>;
def XORI    : LampInstRdRsImm<"xori",    0x3C>;
def SHLI    : LampInstRdRsImm<"shli",    0x3D>;
def SHRI    : LampInstRdRsImm<"shri",    0x3E>;

def LOADX32  : LampInstRdRsRsImm<"loadx32",  0x0E>;
def STOREX32 : LampInstRdRsRsImm<"storex32", 0x11>;

def CMPI : LampInstRdImm<"cmpi", 0x13>;

def FADD : LampInstRdRsRs<"fadd", 0x2B>;
def FSUB : LampInstRdRsRs<"fsub", 0x2C>;
def FMUL : LampInstRdRsRs<"fmul", 0x2D>;
def FDIV : LampInstRdRsRs<"fdiv", 0x2E>;

def INC  : LampInstRd<"inc", 0x37>;
def PUSH : LampInstRd<"push", 0x08>;
def POP  : LampInstRd<"pop", 0x09>;
def INT  : LampInstRd<"int", 0x1A>;

let isTerminator = 1, isBarrier = 1 in {
  def HALT : LampInstNone<"halt", 0x05>;
  let isReturn = 1 in
  def RET  : LampInstNone<"ret", 0x0B>;
}

let isTerminator = 1, isBranch = 1 in {
  def JMP  : LampInstI<"jmp", 0x06>;
  def JZ   : LampInstI<"jz", 0x07>;
  def CALL : LampInstI<"call", 0x0A> {
    let isCall = 1;
  }
  def JNZ  : LampInstI<"jnz", 0x24>;
  def JG   : LampInstI<"jg", 0x25>;
  def JGE  : LampInstI<"jge", 0x26>;
  def JL   : LampInstI<"jl", 0x27>;
  def JLE  : LampInstI<"jle", 0x28>;
  def JC   : LampInstI<"jc", 0x29>;
  def JNC  : LampInstI<"jnc", 0x2A>;
}

def : Pat<(add i32:$lhs, i32:$rhs), (ADD GPR:$lhs, GPR:$rhs)>;
def : Pat<(i32 simm32:$imm), (MOVI simm32:$imm)>;

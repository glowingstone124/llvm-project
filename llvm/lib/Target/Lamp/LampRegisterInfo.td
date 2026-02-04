class LampReg<string n, int enc> : Register<n> {
    let HWEncoding = enc;
}

def R0  : LampReg<"r0",  0>;
def R1  : LampReg<"r1",  1>;
def R2  : LampReg<"r2",  2>;
def R3  : LampReg<"r3",  3>;
def R4  : LampReg<"r4",  4>;
def R5  : LampReg<"r5",  5>;
def R6  : LampReg<"r6",  6>;
def R7  : LampReg<"r7",  7>;
def R8  : LampReg<"r8",  8>;
def R9  : LampReg<"r9",  9>;
def R10 : LampReg<"r10", 10>;
def R11 : LampReg<"r11", 11>;
def R12 : LampReg<"r12", 12>;
def R13 : LampReg<"r13", 13>;
def R14 : LampReg<"r14", 14>;
def R15 : LampReg<"r15", 15>;
def R16 : LampReg<"r16", 16>;
def R17 : LampReg<"r17", 17>;
def R18 : LampReg<"r18", 18>;
def R19 : LampReg<"r19", 19>;
def R20 : LampReg<"r20", 20>;
def R21 : LampReg<"r21", 21>;
def R22 : LampReg<"r22", 22>;
def R23 : LampReg<"r23", 23>;
def R24 : LampReg<"r24", 24>;
def R25 : LampReg<"r25", 25>;
def R26 : LampReg<"r26", 26>;
def R27 : LampReg<"r27", 27>;
def R28 : LampReg<"r28", 28>;
def R29 : LampReg<"r29", 29>;
def R30 : LampReg<"r30", 30>; // SP (reserved in C++ side)
def R31 : LampReg<"r31", 31>; // reserved

def GPR : RegisterClass<"Lamp", [i32], 32,
  (add
    R0, R1, R2, R3, R4, R5, R6, R7,
    R8, R9, R10, R11, R12, R13, R14, R15,
    R16, R17, R18, R19, R20, R21, R22, R23,
    R24, R25, R26, R27, R28, R29, R30, R31
  )
>;
def PtrRC : RegisterClass<"Lamp", [i32], 32, (add
  R0, R1, R2, R3, R4, R5, R6, R7,
  R8, R9, R10, R11, R12, R13, R14, R15,
  R16, R17, R18, R19, R20, R21, R22, R23,
  R24, R25, R26, R27, R28, R29, R30, R31
)>;

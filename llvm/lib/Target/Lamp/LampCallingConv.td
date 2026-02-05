def CC_Lamp : CallingConv<[
  CCIfType<[i32], CCAssignToReg<[R0, R1, R2, R3, R4, R5, R6, R7]>>,
  CCIfType<[i32], CCAssignToStack<4, 4>>
]>;

def RetCC_Lamp : CallingConv<[
  CCIfType<[i32], CCAssignToReg<[R0]>>
]>;

def CC_Lamp : CallingConv<[
  // For variadic calls/functions keep arguments stack-based to make va_list
  // traversal straightforward in the initial ABI implementation.
  CCIfNotVarArg<CCIfType<[i32], CCAssignToReg<[R0, R1, R2, R3, R4, R5, R6, R7]>>>,
  CCIfType<[i32], CCAssignToStack<4, 4>>,
  // 64-bit scalars are stack-passed in the baseline ABI.
  CCIfType<[i64, f64], CCAssignToStack<8, 8>>
]>;

def RetCC_Lamp : CallingConv<[
  CCIfType<[i32], CCAssignToReg<[R0]>>
]>;

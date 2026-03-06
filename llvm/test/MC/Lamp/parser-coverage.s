// RUN: llvm-mc -triple=lamp -filetype=obj %s -o /dev/null

.text
.globl parser_cov
parser_cov:
  andi r1, r2, 1
  ori r3, r4, 2
  xori r5, r6, 3
  shli r7, r8, 4
  shri r9, r10, 5
  roli r11, r12, 6
  rori r13, r14, 7
  addi r11, r12, 6
  subi r13, r14, 7

  div r1, r2, r3
  mod r4, r5, r6
  shl r7, r8, r9
  shr r10, r11, r12
  sar r13, r14, r15
  rol r16, r17, r18
  ror r19, r20, r21

  xadd r1, r2, r3, 0
  xchg r4, r5, r6, 4
  cas r7, r8, r9, r10, 8
  ldar r11, r12, 12
  stlr r13, r14, 16

  fadd r1, r2, r3
  fsub r4, r5, r6
  fmul r7, r8, r9
  fdiv r10, r11, r12
  fneg r13, r14
  fabs r15, r16
  fsqrt r17, r18
  fcmp r19, r20
  itof r21, r22
  ftoi r23, r24

  loadx32 r1, r2, r3, 20
  storex32 r4, r5, r6, 24
  startap r7, r8, 28
  ipi r9, r10
  cpuid r11
  pause
  fence
  ret

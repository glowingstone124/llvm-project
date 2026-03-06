; RUN: llc -march=lamp -filetype=asm %s -o - | FileCheck %s

target triple = "lamp-unknown-unknown-elf"

declare i32 @llvm.fshl.i32(i32, i32, i32)
declare i32 @llvm.fshr.i32(i32, i32, i32)

define i32 @rot_var(i32 %x, i32 %y) {
entry:
  %a = call i32 @llvm.fshl.i32(i32 %x, i32 %x, i32 %y)
  %b = call i32 @llvm.fshr.i32(i32 %x, i32 %x, i32 %y)
  %c = xor i32 %a, %b
  ret i32 %c
}

define i32 @rot_imm(i32 %x) {
entry:
  %a = call i32 @llvm.fshl.i32(i32 %x, i32 %x, i32 7)
  %b = call i32 @llvm.fshr.i32(i32 %x, i32 %x, i32 9)
  %c = add i32 %a, %b
  ret i32 %c
}

; CHECK-LABEL: rot_var:
; CHECK-DAG: rol
; CHECK-DAG: ror
; CHECK: ret

; CHECK-LABEL: rot_imm:
; CHECK-DAG: roli
; CHECK-DAG: rori
; CHECK: ret

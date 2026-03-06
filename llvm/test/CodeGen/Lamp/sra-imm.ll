; RUN: llc -march=lamp -filetype=asm %s -o - | FileCheck %s

target triple = "lamp-unknown-unknown-elf"

define i32 @ashr_imm(i32 %x) {
entry:
  %y = ashr i32 %x, 7
  ret i32 %y
}

; CHECK-LABEL: ashr_imm:
; CHECK: movi r1, 7
; CHECK-NEXT: sar r0, r0, r1
; CHECK-NEXT: ret

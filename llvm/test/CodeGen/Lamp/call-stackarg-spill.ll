; RUN: llc -march=lamp -O2 -filetype=asm %s -o - | FileCheck %s

target triple = "lamp-unknown-unknown-elf"

declare i32 @callee9(i32, i32, i32, i32, i32, i32, i32, i32, i32)

define i32 @stress(i32 %a, i32 %b, i32 %c, i32 %d, i32 %e, i32 %f) {
entry:
  %x0 = add i32 %a, %b
  %x1 = add i32 %x0, %c
  %x2 = add i32 %x1, %d
  %x3 = add i32 %x2, %e
  %x4 = add i32 %x3, %f
  %x5 = mul i32 %x4, 3
  %x6 = add i32 %x5, %a
  %x7 = add i32 %x6, %b
  %x8 = add i32 %x7, %c
  %x9 = add i32 %x8, %d
  %x10 = add i32 %x9, %e
  %x11 = add i32 %x10, %f
  %r = call i32 @callee9(i32 %x0, i32 %x1, i32 %x2, i32 %x3, i32 %x4, i32 %x5, i32 %x6, i32 %x7, i32 %x8)
  %y0 = add i32 %r, %x9
  %y1 = add i32 %y0, %x10
  %y2 = add i32 %y1, %x11
  ret i32 %y2
}

; CHECK-LABEL: stress:
; CHECK: subi r30, r30, 12
; CHECK: subi r30, r30, 4
; CHECK: store32 r31, r30, 0
; CHECK: addi r31, r30, 4
; CHECK: store32 {{r[0-9]+}}, r31, 0
; CHECK: rcall callee9
; CHECK: load32 {{r[0-9]+}}, r31, 0
; CHECK: mov r30, r31
; CHECK: load32 r31, r30, -4
; CHECK: ret

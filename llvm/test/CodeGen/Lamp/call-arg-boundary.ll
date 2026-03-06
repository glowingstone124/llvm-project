; RUN: llc -march=lamp -O0 -filetype=asm %s -o - | FileCheck %s

target triple = "lamp-unknown-unknown-elf"

define i32 @callee7(i32 %a0, i32 %a1, i32 %a2, i32 %a3, i32 %a4, i32 %a5, i32 %a6) {
entry:
  ret i32 %a6
}

define i32 @callee8(i32 %a0, i32 %a1, i32 %a2, i32 %a3, i32 %a4, i32 %a5, i32 %a6, i32 %a7) {
entry:
  ret i32 %a7
}

define i32 @callee9(i32 %a0, i32 %a1, i32 %a2, i32 %a3, i32 %a4, i32 %a5, i32 %a6, i32 %a7, i32 %a8) {
entry:
  ret i32 %a8
}

define i32 @caller7() {
entry:
  %r = call i32 @callee7(i32 10, i32 11, i32 12, i32 13, i32 14, i32 15, i32 16)
  ret i32 %r
}

define i32 @caller8() {
entry:
  %r = call i32 @callee8(i32 20, i32 21, i32 22, i32 23, i32 24, i32 25, i32 26, i32 27)
  ret i32 %r
}

define i32 @caller9() {
entry:
  %r = call i32 @callee9(i32 30, i32 31, i32 32, i32 33, i32 34, i32 35, i32 36, i32 37, i32 38)
  ret i32 %r
}

; CHECK-LABEL: callee7:
; CHECK: mov r0, r6
; CHECK: ret

; CHECK-LABEL: callee8:
; CHECK: mov r0, r7
; CHECK: ret

; CHECK-LABEL: callee9:
; CHECK: load32 r0, r30, 0
; CHECK: ret

; CHECK-LABEL: caller7:
; CHECK: movi r6, 16
; CHECK: rcall callee7

; CHECK-LABEL: caller8:
; CHECK: movi r7, 27
; CHECK: rcall callee8

; CHECK-LABEL: caller9:
; CHECK: subi r30, r30, 4
; CHECK: movi r0, 38
; CHECK: store32 r0, {{r[0-9]+}}, -4
; CHECK: rcall callee9
; CHECK: addi r{{[0-9]+}}, r{{[0-9]+}}, 4

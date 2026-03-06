; RUN: llc -march=lamp -filetype=asm %s -o - | FileCheck %s

define i32 @f2_stack(i32 %a) {
entry:
  %slot = alloca i32, align 4
  store i32 %a, ptr %slot, align 4
  %v = load i32, ptr %slot, align 4
  %r = add i32 %v, 1
  ret i32 %r
}

; CHECK-LABEL: f2_stack:
; CHECK: subi r30, r30, 4
; CHECK: store32 r0, r30, 0
; CHECK: addi r0, r0, 1
; CHECK: addi r30, r30, 4
; CHECK: ret

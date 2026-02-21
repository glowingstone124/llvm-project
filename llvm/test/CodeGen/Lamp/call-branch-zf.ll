; RUN: llc -march=lamp -O0 -filetype=asm %s -o - | FileCheck %s

target triple = "lamp-unknown-unknown-elf"

declare i32 @pred32(i32)

define i32 @call_branch_i32(i32 %x) {
entry:
  %v = call i32 @pred32(i32 %x)
  %c = icmp ne i32 %v, 0
  br i1 %c, label %t, label %f

t:
  ret i32 1
f:
  ret i32 0
}

; CHECK-LABEL: call_branch_i32:
; CHECK: rcall pred32
; CHECK: subi r0, r0, 0
; CHECK: rjz r0,

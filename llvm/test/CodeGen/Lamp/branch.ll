; RUN: llc -march=lamp -filetype=asm %s -o - | FileCheck %s

define i32 @f3_branch(i32 %x) {
entry:
  %cmp = icmp eq i32 %x, 0
  br i1 %cmp, label %iszero, label %notzero

iszero:
  ret i32 0

notzero:
  %y = add i32 %x, 1
  ret i32 %y
}

; CHECK-LABEL: f3_branch:
; CHECK: rjnz {{r[0-9]+}}, [[NOTZERO:\.LBB[0-9_]+]]
; CHECK: rjmp [[ISZERO:\.LBB[0-9_]+]]
; CHECK: [[ISZERO]]:
; CHECK: movi r0, 0
; CHECK: ret
; CHECK: [[NOTZERO]]:
; CHECK: addi r0, r0, 1
; CHECK: ret

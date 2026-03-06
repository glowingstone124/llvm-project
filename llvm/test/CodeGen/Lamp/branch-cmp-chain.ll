; RUN: llc -march=lamp -filetype=asm %s -o - | FileCheck %s

define i32 @cmp_sge(i32 %a, i32 %b) {
entry:
  %c = icmp sge i32 %a, %b
  br i1 %c, label %t, label %f

t:
  ret i32 1

f:
  ret i32 0
}

; CHECK-LABEL: cmp_sge:
; CHECK: cmp
; CHECK: jl [[FALSE:\.LBB[0-9_]+]]
; CHECK: rjmp
; CHECK: [[FALSE]]:

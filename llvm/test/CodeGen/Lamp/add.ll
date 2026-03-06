; RUN: llc -march=lamp -filetype=asm %s -o - | FileCheck %s

define i32 @add2(i32 %a, i32 %b) {
entry:
  %sum = add i32 %a, %b
  ret i32 %sum
}

; CHECK-LABEL: add2:
; CHECK: add r0, r0, r1
; CHECK-NEXT: ret

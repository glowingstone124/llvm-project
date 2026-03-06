; RUN: llc -march=lamp -filetype=asm %s -o - | FileCheck %s

define void @ia() {
entry:
  call void asm sideeffect "movi r1, 1\0Aout r0, [r1]\0A", ""()
  ret void
}

; CHECK-LABEL: ia:
; CHECK: movi r1, 1
; CHECK: out r0, r1
; CHECK: ret

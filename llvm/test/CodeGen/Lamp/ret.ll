; RUN: llc -march=lamp -filetype=asm %s -o - | FileCheck %s

define void @ret_void() {
entry:
  ret void
}

define i32 @ret_i32() {
entry:
  ret i32 42
}

; CHECK-LABEL: ret_void:
; CHECK: ret

; CHECK-LABEL: ret_i32:
; CHECK: movi r0, 42
; CHECK-NEXT: ret

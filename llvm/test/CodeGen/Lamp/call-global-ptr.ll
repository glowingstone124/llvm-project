; RUN: llc -march=lamp -filetype=asm %s -o - | FileCheck %s

@g = global i32 7

define i32 @callee(i32 %a, i32 %b) {
entry:
  %c = add i32 %a, %b
  ret i32 %c
}

define i32 @caller(i32 %x) {
entry:
  %y = call i32 @callee(i32 %x, i32 3)
  ret i32 %y
}

define i32 @getg() {
entry:
  %v = load i32, ptr @g
  ret i32 %v
}

define i32 @rd64(i64 %a) {
entry:
  %p = inttoptr i64 %a to ptr
  %v = load i32, ptr %p
  ret i32 %v
}

; CHECK-LABEL: caller:
; CHECK: rcall callee
; CHECK: ret

; CHECK-LABEL: getg:
; CHECK: movi r0, g
; CHECK-NEXT: load32 r0, r0, 0
; CHECK-NEXT: ret

; CHECK-LABEL: rd64:
; CHECK: load32 r0, r0, 0
; CHECK-NEXT: ret

; RUN: llc -march=lamp -filetype=asm %s -o - | FileCheck %s

target triple = "lamp-unknown-unknown-elf"

declare i32 @vprint(i32, ...)

define i32 @call_vararg(i32 %x) {
entry:
  %r = call i32 (i32, ...) @vprint(i32 1, i32 %x, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10)
  ret i32 %r
}

; CHECK-LABEL: call_vararg:
; CHECK: store32
; CHECK: store32
; CHECK: rcall vprint
; CHECK: addi r30, r30, 40
; CHECK: ret

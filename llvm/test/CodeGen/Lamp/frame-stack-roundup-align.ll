; RUN: llc -march=lamp -O0 -filetype=asm %s -o - | FileCheck %s

target triple = "lamp-unknown-unknown-elf"

declare void @sink(ptr)

define void @roundup_call_frame() {
entry:
  %buf = alloca [11 x i32], align 4
  call void @sink(ptr %buf)
  ret void
}

; CHECK-LABEL: roundup_call_frame:
; CHECK: subi r30, r30, 48
; CHECK: subi r30, r30, 8
; CHECK: store32 r31, r30, 4
; CHECK: addi r31, r30, 8
; CHECK: addi r0, r31, 4

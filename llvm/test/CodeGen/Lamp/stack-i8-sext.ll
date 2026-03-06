; RUN: llc -march=lamp -filetype=asm %s -o - | FileCheck %s

target datalayout = "e-m:e-p:32:32-i32:32-n32-S32"
target triple = "lamp-unknown-unknown-elf"

define i32 @stack_i8_sext_forced(i32 %x) {
entry:
  %p = alloca i8, align 1
  %t = trunc i32 %x to i8
  store volatile i8 %t, ptr %p, align 1
  %v = load volatile i8, ptr %p, align 1
  %s = sext i8 %v to i32
  ret i32 %s
}

; CHECK-LABEL: stack_i8_sext_forced:
; CHECK: store
; CHECK: load
; CHECK: xori r0, r0, 128
; CHECK-NEXT: subi r0, r0, 128
; CHECK: ret

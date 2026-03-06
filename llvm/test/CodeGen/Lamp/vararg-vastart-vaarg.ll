; RUN: llc -march=lamp -filetype=asm %s -o - | FileCheck %s

target triple = "lamp-unknown-unknown-elf"

declare void @llvm.va_start(ptr)
declare void @llvm.va_end(ptr)

define i32 @first_vararg(i32 %n, ...) {
entry:
  %ap = alloca ptr, align 4
  call void @llvm.va_start(ptr %ap)
  %x = va_arg ptr %ap, i32
  call void @llvm.va_end(ptr %ap)
  ret i32 %x
}

; CHECK-LABEL: first_vararg:
; CHECK: subi r30, r30, 4
; CHECK: addi r0, r30, 8
; CHECK: addi r0, r0, 4
; CHECK: store32 r0, r30, 0
; CHECK: load32 r0, r30, 8
; CHECK: addi r30, r30, 4
; CHECK: ret

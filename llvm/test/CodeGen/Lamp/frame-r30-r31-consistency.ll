; RUN: llc -march=lamp -O0 -filetype=asm %s -o - | FileCheck %s

target triple = "lamp-unknown-unknown-elf"

define i32 @nofp(i32 %x) {
entry:
  %s = alloca i32, align 4
  store i32 %x, ptr %s, align 4
  %v = load i32, ptr %s, align 4
  ret i32 %v
}

define i32 @withfp(i32 %n) {
entry:
  %p = alloca i8, i32 %n, align 4
  store volatile i8 0, ptr %p, align 1
  %r = ptrtoint ptr %p to i32
  ret i32 %r
}

; CHECK-LABEL: nofp:
; CHECK: subi r30, r30, 8
; CHECK-NOT: mov r31, r30
; CHECK: addi r30, r30, 8
; CHECK: ret

; CHECK-LABEL: withfp:
; CHECK: subi r30, r30, 8
; CHECK: store32 r31, r30, 4
; CHECK: addi r31, r30, 8
; CHECK: mov r30, r31
; CHECK: load32 r31, r30, -4
; CHECK: ret

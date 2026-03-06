; RUN: llc -march=lamp -filetype=asm %s -o - | FileCheck %s

target triple = "lamp-unknown-unknown-elf"

define i32 @vla(i32 %n) {
entry:
  %p = alloca i8, i32 %n, align 4
  store i8 1, ptr %p, align 1
  %v = load i8, ptr %p, align 1
  %ext = sext i8 %v to i32
  ret i32 %ext
}

; CHECK-LABEL: vla:
; CHECK: store32 r31, r30, 0
; CHECK: addi r31, r30, 4
; CHECK: andi {{r[0-9]+}}, {{r[0-9]+}}, -4
; CHECK: sub {{r[0-9]+}}, r30, {{r[0-9]+}}
; CHECK: mov r30, {{r[0-9]+}}
; CHECK: mov r30, r31
; CHECK: load32 r31, r30, -4
; CHECK: ret

; RUN: llc -march=lamp -O0 -filetype=asm %s -o - | FileCheck %s

target triple = "lamp-unknown-unknown-elf"

define i64 @ret_const() {
entry:
  ret i64 1234605616436508552
}

define i32 @use_hi() {
entry:
  %v = call i64 @ret_const()
  %hi64 = lshr i64 %v, 32
  %hi = trunc i64 %hi64 to i32
  ret i32 %hi
}

; CHECK-LABEL: use_hi:
; CHECK: subi r30, r30, 8
; CHECK: store32 r31, r30, 4
; CHECK: addi r31, r30, 8

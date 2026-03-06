; RUN: llc -march=lamp -O0 -filetype=asm %s -o - | FileCheck %s

target triple = "lamp-unknown-unknown-elf"

@g = global i32 0

define i32 @step_eq() {
entry:
  %v = load i32, ptr @g
  %inc = add i32 %v, 1
  store i32 %inc, ptr @g
  %c = icmp eq i32 %inc, 80
  br i1 %c, label %t, label %f

t:
  ret i32 1
f:
  ret i32 0
}

define i32 @step_uge() {
entry:
  %v = load i32, ptr @g
  %inc = add i32 %v, 1
  store i32 %inc, ptr @g
  %c = icmp uge i32 %inc, 80
  br i1 %c, label %t, label %f

t:
  ret i32 1
f:
  ret i32 0
}

; CHECK-LABEL: step_eq:
; CHECK: movi {{r[0-9]+}}, g
; CHECK: load32 {{r[0-9]+}}, {{r[0-9]+}}, 0
; CHECK: addi {{r[0-9]+}}, {{r[0-9]+}}, 1
; CHECK: store32 {{r[0-9]+}}, {{r[0-9]+}}, 0
; CHECK: subi {{r[0-9]+}}, {{r[0-9]+}}, 80
; CHECK: rjnz {{r[0-9]+}},

; CHECK-LABEL: step_uge:
; CHECK: cmpi {{r[0-9]+}}, 80
; CHECK: jc

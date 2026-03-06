; RUN: llc -march=lamp -O0 -filetype=asm %s -o - | FileCheck %s

target triple = "lamp-unknown-unknown-elf"

define i32 @step_if(i32 %v) {
entry:
  %c = icmp uge i32 %v, 80
  br i1 %c, label %t, label %f

t:
  ret i32 1
f:
  ret i32 0
}

define i32 @step_switch(i32 %v) {
entry:
  switch i32 %v, label %def [
    i32 80, label %t
  ]

t:
  ret i32 1

def:
  ret i32 0
}

; CHECK-LABEL: step_if:
; CHECK: cmpi {{r[0-9]+}}, 80
; CHECK: jc

; CHECK-LABEL: step_switch:
; CHECK: subi {{r[0-9]+}}, {{r[0-9]+}}, 80
; CHECK: rjnz

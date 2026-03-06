; RUN: llc -march=lamp -O0 -filetype=asm %s -o - | FileCheck %s

target triple = "lamp-unknown-unknown-elf"

define i32 @sum(ptr %buf, i32 %n) {
entry:
  br label %loop

loop:
  %i = phi i32 [ 0, %entry ], [ %i.next, %body ]
  %acc = phi i32 [ 0, %entry ], [ %acc.next, %body ]
  %cond = icmp ult i32 %i, %n
  br i1 %cond, label %body, label %done

body:
  %p = getelementptr i8, ptr %buf, i32 %i
  %v = load i8, ptr %p, align 1
  %vz = zext i8 %v to i32
  %acc.next = add i32 %acc, %vz
  %i.next = add i32 %i, 1
  br label %loop

done:
  ret i32 %acc
}

; CHECK-LABEL: sum:
; CHECK: cmp
; CHECK: jnc
; CHECK: load {{r[0-9]+}}, {{r[0-9]+}}, 0
; CHECK: andi {{r[0-9]+}}, {{r[0-9]+}}, 255
; CHECK: addi {{r[0-9]+}}, {{r[0-9]+}}, 1

; RUN: llc -march=lamp -filetype=asm %s -o - | FileCheck %s

target triple = "lamp-unknown-unknown-elf"

define i32 @count(ptr %s) {
entry:
  br label %loop

loop:
  %p = phi ptr [ %s, %entry ], [ %next, %cont ]
  %n = phi i32 [ 0, %entry ], [ %n1, %cont ]
  %ch = load i8, ptr %p, align 1
  %end = icmp eq i8 %ch, 0
  br i1 %end, label %done, label %cont

cont:
  %next = getelementptr i8, ptr %p, i32 1
  %n1 = add i32 %n, 1
  br label %loop

done:
  ret i32 %n
}

; CHECK-LABEL: count:
; CHECK: load {{r[0-9]+}}, {{r[0-9]+}}, 0
; CHECK: andi {{r[0-9]+}}, {{r[0-9]+}}, 255
; CHECK: rjz {{r[0-9]+}},
; CHECK: addi {{r[0-9]+}}, {{r[0-9]+}}, 1

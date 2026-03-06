; RUN: llc -march=lamp -O2 -stop-after=prologepilog %s -o - | FileCheck %s

target triple = "lamp-unknown-unknown-elf"

declare i32 @foo(i32)

define i32 @test(i32 %x) {
entry:
  %cmp = icmp eq i32 %x, 0
  %r = call i32 @foo(i32 %x)
  %v = select i1 %cmp, i32 %r, i32 %x
  ret i32 %v
}

; CHECK-LABEL: name: test
; CHECK: RCALL @foo,{{.*}}implicit-def{{.*}}$flags

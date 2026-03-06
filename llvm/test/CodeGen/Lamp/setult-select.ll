; RUN: llc -march=lamp -filetype=asm %s -o - | FileCheck %s

target triple = "lamp-unknown-unknown-elf"

define i32 @setult_to_i32(i32 %a, i32 %b) {
entry:
  %cmp = icmp ult i32 %a, %b
  %z = zext i1 %cmp to i32
  ret i32 %z
}

define i32 @select_ult(i32 %a, i32 %b, i32 %x, i32 %y) {
entry:
  %cmp = icmp ult i32 %a, %b
  %sel = select i1 %cmp, i32 %x, i32 %y
  ret i32 %sel
}

; CHECK-LABEL: setult_to_i32:
; CHECK: shri r0, r0, 31
; CHECK-NEXT: ret

; CHECK-LABEL: select_ult:
; CHECK: shri r0, r0, 31
; CHECK: sub r0, r1, r0
; CHECK: xor r0, r3, r0
; CHECK-NEXT: ret

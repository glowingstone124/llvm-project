; RUN: llc -march=lamp -filetype=asm %s -o - | FileCheck %s

define void @store_i16_from_i32(ptr %p, i32 %x) {
entry:
  %t = trunc i32 %x to i16
  store i16 %t, ptr %p, align 2
  ret void
}

; CHECK-LABEL: store_i16_from_i32:
; CHECK-COUNT-2: store
; CHECK: shri
; CHECK: ret

define i32 @load_i16_zext(ptr %p) {
entry:
  %v = load i16, ptr %p, align 2
  %z = zext i16 %v to i32
  ret i32 %z
}

; CHECK-LABEL: load_i16_zext:
; CHECK-COUNT-2: load
; CHECK: shli
; CHECK: or
; CHECK: ret

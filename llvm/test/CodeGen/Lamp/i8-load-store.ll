; RUN: llc -march=lamp -filetype=asm %s -o - | FileCheck %s

@b = global i8 0

define i32 @load_zext() {
entry:
  %v = load i8, ptr @b, align 1
  %z = zext i8 %v to i32
  ret i32 %z
}

define i32 @load_sext() {
entry:
  %v = load i8, ptr @b, align 1
  %s = sext i8 %v to i32
  ret i32 %s
}

define void @store_i8(i8 %x) {
entry:
  store i8 %x, ptr @b, align 1
  ret void
}

define void @copy_i8(ptr %dst, ptr %src) {
entry:
  %v = load i8, ptr %src, align 1
  store i8 %v, ptr %dst, align 1
  ret void
}

; CHECK-LABEL: load_zext:
; CHECK: movi r0, b
; CHECK: load r0, r0, 0
; CHECK: andi r0, r0, 255
; CHECK: ret

; CHECK-LABEL: load_sext:
; CHECK: movi r0, b
; CHECK: load r0, r0, 0
; CHECK: ret

; CHECK-LABEL: store_i8:
; CHECK: movi r1, b
; CHECK: store r0, r1, 0
; CHECK: ret

; CHECK-LABEL: copy_i8:
; CHECK: load
; CHECK: andi
; CHECK: store
; CHECK: ret

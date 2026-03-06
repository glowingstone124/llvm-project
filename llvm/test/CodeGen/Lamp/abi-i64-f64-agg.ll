; RUN: llc -march=lamp -O0 -filetype=asm %s -o - | FileCheck %s

target triple = "lamp-unknown-unknown-elf"

@g = global i64 0
@gd = global i64 0

define i64 @ret_i64(i64 %x) {
entry:
  %y = add i64 %x, 1
  ret i64 %y
}

define double @ret_f64(double %x) {
entry:
  ret double %x
}

define { i32, i32 } @ret_agg(i32 %a, i32 %b) {
entry:
  %r0 = insertvalue { i32, i32 } undef, i32 %a, 0
  %r1 = insertvalue { i32, i32 } %r0, i32 %b, 1
  ret { i32, i32 } %r1
}

define void @take_i64(i64 %x) {
entry:
  store volatile i64 %x, ptr @g, align 8
  ret void
}

define void @take_f64(double %x) {
entry:
  %b = bitcast double %x to i64
  store volatile i64 %b, ptr @gd, align 8
  ret void
}

; CHECK-LABEL: ret_i64:
; CHECK: store32 {{r[0-9]+}}, {{r[0-9]+}}, 0
; CHECK: store32 {{r[0-9]+}}, {{r[0-9]+}}, 4

; CHECK-LABEL: ret_f64:
; CHECK: store32 {{r[0-9]+}}, {{r[0-9]+}}, 4
; CHECK: store32 {{r[0-9]+}}, {{r[0-9]+}}, 0

; CHECK-LABEL: ret_agg:
; CHECK: store32 {{r[0-9]+}}, {{r[0-9]+}}, 4
; CHECK: store32 {{r[0-9]+}}, {{r[0-9]+}}, 0

; CHECK-LABEL: take_i64:
; CHECK: movi {{r[0-9]+}}, g
; CHECK: store32 {{r[0-9]+}}, {{r[0-9]+}}, 4
; CHECK: store32 {{r[0-9]+}}, {{r[0-9]+}}, 0

; CHECK-LABEL: take_f64:
; CHECK: movi {{r[0-9]+}}, gd
; CHECK: store32 {{r[0-9]+}}, {{r[0-9]+}}, 4
; CHECK: store32 {{r[0-9]+}}, {{r[0-9]+}}, 0

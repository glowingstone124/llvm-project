; RUN: llc -march=lamp -emulated-tls -filetype=asm %s -o - | FileCheck %s

target triple = "lamp-unknown-unknown-elf"
@t = thread_local global i32 1

define i32 @f() {
entry:
  %v = load i32, ptr @t
  ret i32 %v
}

; CHECK-LABEL: f:
; CHECK: __emutls_v.t
; CHECK: __emutls_get_address

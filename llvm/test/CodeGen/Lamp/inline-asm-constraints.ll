; RUN: llc -march=lamp -filetype=asm %s -o - | FileCheck %s

target triple = "lamp-unknown-unknown-elf"

define void @asm_imm() {
entry:
  call void asm sideeffect "movi r0, $0", "I"(i32 7)
  ret void
}

define void @asm_mem(ptr %p) {
entry:
  call void asm sideeffect "load32 r0, $0, 0", "m"(ptr %p)
  ret void
}

; CHECK-LABEL: asm_imm:
; CHECK: #APP
; CHECK: movi r0, 7
; CHECK: #NO_APP

; CHECK-LABEL: asm_mem:
; CHECK: #APP
; CHECK: load32 r0, {{r[0-9]+}}, 0
; CHECK: #NO_APP

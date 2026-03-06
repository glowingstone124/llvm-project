; RUN: not llc -march=lamp -relocation-model=pic -filetype=asm %s -o - 2>&1 | FileCheck %s

target triple = "lamp-unknown-unknown-elf"
@g = global i32 1

define i32 @f() {
entry:
  %v = load i32, ptr @g
  ret i32 %v
}

; CHECK: PIC relocations are not supported on the Lamp target

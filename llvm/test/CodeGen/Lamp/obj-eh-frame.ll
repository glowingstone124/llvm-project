; RUN: llc -march=lamp -filetype=obj %s -o %t
; RUN: llvm-readobj -S -r %t | FileCheck %s

target triple = "lamp-unknown-unknown-elf"

define i32 @f(i32 %x) #0 {
entry:
  %y = add i32 %x, 1
  ret i32 %y
}

attributes #0 = { nounwind uwtable }

; CHECK: Name: .eh_frame
; CHECK: Name: .rela.eh_frame
; CHECK: R_LAMP_32 .text 0x0

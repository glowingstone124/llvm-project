// RUN: llvm-mc -triple=lamp -filetype=obj %s -o - | llvm-readobj -r - | FileCheck %s

  .text
  .globl foo
foo:
  // PC-relative control-flow encodings.
  rjmp ext
  rcall ext
  rjz r0, ext
  rjnz r1, ext

  // Absolute control-flow encodings.
  jmp ext
  call ext
  jz r0, ext
  jnz r1, ext
  ret

// CHECK: Relocations [
// CHECK: Section (3) .rela.text {
// CHECK: 0x0 R_LAMP_PC32 ext 0x0
// CHECK: 0x8 R_LAMP_PC32 ext 0x0
// CHECK: 0x10 R_LAMP_PC32 ext 0x0
// CHECK: 0x18 R_LAMP_PC32 ext 0x0
// CHECK: 0x20 R_LAMP_32 ext 0x0
// CHECK: 0x28 R_LAMP_32 ext 0x0
// CHECK: 0x30 R_LAMP_32 ext 0x0
// CHECK: 0x38 R_LAMP_32 ext 0x0

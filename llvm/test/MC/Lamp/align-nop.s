// RUN: llvm-mc -triple=lamp -filetype=obj %s -o - | llvm-readobj --hex-dump=.text - | FileCheck %s

.text
.globl foo
foo:
  ret
  .p2align 4
  ret

// CHECK: Hex dump of section '.text':
// CHECK: 0x00000000 00000000 0000000b 00000000 00000014
// CHECK: 0x00000010 00000000 0000000b
// CHECK-NOT: 00000005

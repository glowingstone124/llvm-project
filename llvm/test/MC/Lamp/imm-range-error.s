// RUN: not llvm-mc -triple=lamp -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

  movi r0, 4294967296

// CHECK: error: LampMCCodeEmitter: immediate out of 32-bit range

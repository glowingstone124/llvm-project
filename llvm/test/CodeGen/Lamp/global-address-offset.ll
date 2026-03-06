; RUN: llc -march=lamp -filetype=asm %s -o - | FileCheck %s --check-prefix=ASM
; RUN: llc -march=lamp -filetype=obj %s -o - | llvm-readobj -r - | FileCheck %s --check-prefix=RELOC

target datalayout = "e-m:e-p:32:32-i32:32-n32-S32"
target triple = "lamp"

%Task = type { i32, i32, i32, i32, i32, i32 }

@g_tasks = external dso_local global [0 x %Task], align 4

define dso_local void @f() {
entry:
  store i32 1, ptr getelementptr inbounds nuw (%Task, ptr @g_tasks, i32 0, i32 4), align 4
  store i32 2, ptr getelementptr inbounds nuw (%Task, ptr @g_tasks, i32 0, i32 1), align 4
  store i32 3, ptr getelementptr inbounds nuw (%Task, ptr @g_tasks, i32 0, i32 5), align 4
  ret void
}

; ASM-LABEL: f:
; ASM-DAG: movi {{r[0-9]+}}, g_tasks+16
; ASM-DAG: movi {{r[0-9]+}}, g_tasks+4
; ASM-DAG: movi {{r[0-9]+}}, g_tasks+20

; RELOC-DAG: R_LAMP_32 g_tasks 0x10
; RELOC-DAG: R_LAMP_32 g_tasks 0x4
; RELOC-DAG: R_LAMP_32 g_tasks 0x14

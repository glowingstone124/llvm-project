; RUN: llc -march=lamp -filetype=asm %s -o - | FileCheck %s

define i32 @atomic_xadd(ptr %p, i32 %v) {
entry:
  %old = atomicrmw add ptr %p, i32 %v seq_cst
  ret i32 %old
}

; CHECK-LABEL: atomic_xadd:
; CHECK: xadd
; CHECK: ret

define i32 @atomic_xchg(ptr %p, i32 %v) {
entry:
  %old = atomicrmw xchg ptr %p, i32 %v seq_cst
  ret i32 %old
}

; CHECK-LABEL: atomic_xchg:
; CHECK: xchg
; CHECK: ret

define i32 @atomic_cmpxchg_old(ptr %p, i32 %expected, i32 %desired) {
entry:
  %pair = cmpxchg ptr %p, i32 %expected, i32 %desired seq_cst seq_cst
  %old = extractvalue { i32, i1 } %pair, 0
  ret i32 %old
}

; CHECK-LABEL: atomic_cmpxchg_old:
; CHECK: cas
; CHECK: ret

define i1 @atomic_cmpxchg_success(ptr %p, i32 %expected, i32 %desired) {
entry:
  %pair = cmpxchg ptr %p, i32 %expected, i32 %desired seq_cst seq_cst
  %ok = extractvalue { i32, i1 } %pair, 1
  ret i1 %ok
}

; CHECK-LABEL: atomic_cmpxchg_success:
; CHECK: cas
; CHECK: ret

define void @atomic_fence() {
entry:
  fence seq_cst
  ret void
}

; CHECK-LABEL: atomic_fence:
; CHECK: fence
; CHECK: ret

define i32 @atomic_load_acquire(ptr %p) {
entry:
  %v = load atomic i32, ptr %p acquire, align 4
  ret i32 %v
}

; CHECK-LABEL: atomic_load_acquire:
; CHECK: ldar
; CHECK: ret

define void @atomic_store_release(ptr %p, i32 %v) {
entry:
  store atomic i32 %v, ptr %p release, align 4
  ret void
}

; CHECK-LABEL: atomic_store_release:
; CHECK: stlr
; CHECK: ret

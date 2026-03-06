; RUN: llc -march=lamp -O0 -stop-after=greedy %s -o - | FileCheck %s --check-prefix=MIR

%struct.S = type { i32, i32, i32, i32, i32, i32, i32, i32 }

define i32 @idx_scaled(ptr %base, i16 %idx16) {
entry:
  %slot = alloca i16, align 2
  store i16 %idx16, ptr %slot, align 2
  %idx16r = load i16, ptr %slot, align 2
  %idx32 = zext i16 %idx16r to i32
  %elt = getelementptr inbounds %struct.S, ptr %base, i32 %idx32
  %field = getelementptr inbounds %struct.S, ptr %elt, i32 0, i32 0
  %v = load i32, ptr %field, align 4
  ret i32 %v
}

; MIR-LABEL: name: idx_scaled
; MIR: SHLI
; MIR-NOT: SHLI killed $noreg

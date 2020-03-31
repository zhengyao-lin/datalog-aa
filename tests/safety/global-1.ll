; RUN: %opt -S < %s 2>&1 | FileCheck %s

@global = external constant i32*

; CHECK-DAG: @global -> @global::aff(1)
; CHECK-DAG: @global::aff(1) -> @global::aff(1)
; CHECK-DAG: @global::aff(1) -> @main::aff(1)

define i32 @main() {
entry:
    ret i32 0
}

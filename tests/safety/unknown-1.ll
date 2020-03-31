; RUN: %opt -S < %s 2>&1 | FileCheck %s

; allocations are assigned correctly
; CHECK-DAG: @main -> @main::aff(1)
; CHECK-DAG: @main::%a -> @main::%a::aff(1)
; CHECK-DAG: @main::%b -> @main::%b::aff(1)

; correct analysis for the simple pointer manipulations
; CHECK-DAG: @main::%b::aff(1) -> @main::%a::aff(1)
; CHECK-DAG: @main::%c -> @main::%a::aff(1)

; external functions can behave arbitrarily
; CHECK-DAG: @main::%result -> @main::aff(1)
; CHECK-DAG: @main::%result -> @main::%a::aff(1)
; CHECK-DAG: @main::%result -> @main::%b::aff(1)

declare i32* @0()

define i32 @main() {
entry:
    %a = alloca i32
    %b = alloca i32*

    store i32* %a, i32** %b
    %c = load i32*, i32** %b

    %result = call i32* @0()

    ret i32 0
}

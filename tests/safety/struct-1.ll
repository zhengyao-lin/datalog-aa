; RUN: %opt -S -datalog-aa -datalog-aa-print-pts-to < %s 2>&1 | FileCheck %s

%t1 = type { i32, i32*, [4 x i32*] }

define i32 @main() {
entry:
    %a = alloca [4 x i32*]
    %b = alloca %t1
    %c = alloca i32

    ; allocations
    ; CHECK-DAG: @main::%b -> @main::%b::aff(1)
    ; CHECK-DAG: @main::%c -> @main::%c::aff(1)
    ; CHECK-DAG: @main::%a -> @main::%a::aff(1)

    %d = getelementptr [4 x i32*], [4 x i32*]* %a, i32 0, i32 2 ; %a[2]
    store i32* %c, i32** %d

    %e = getelementptr %t1, %t1* %b, i32 0, i32 2, i32 2 ; %b.2[2]
    %f = getelementptr [4 x i32*], [4 x i32*]* %a, i32 0, i32 2 ; %a[2]

    ; getelementptr works correctly
    ; CHECK-DAG: @main::%e -> @main::%b::aff(1)
    ; CHECK-DAG: @main::%d -> @main::%a::aff(1)
    ; CHECK-DAG: @main::%f -> @main::%a::aff(1)

    %g = load i32*, i32** %f
    store i32* %g, i32** %e

    ; goal is to let b potentially pointing to c
    ; CHECK-DAG: @main::%a::aff(1) -> @main::%c::aff(1)
    ; CHECK-DAG: @main::%g -> @main::%c::aff(1)
    ; CHECK-DAG: @main::%b::aff(1) -> @main::%c::aff(1)

    ret i32 0
}

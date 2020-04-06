; RUN: %opt -S < %s 2>&1 | FileCheck %s

declare i8* @malloc(i32)
declare void @llvm.memcpy.p0i8.p0i8.i32(i8*, i8*, i32, i1)

define i32 @main() {
entry:
    %a = call i8* @malloc(i32 8)
    %b = call i8* @malloc(i32 8)
    %c = call i8* @malloc(i32 8)

    ; CHECK-DAG: @main::%a -> @main::%a::aff(1)
    ; CHECK-DAG: @main::%b -> @main::%b::aff(1)
    ; CHECK-DAG: @main::%c -> @main::%c::aff(1)
    ; CHECK-DAG: @main::%b::aff(1) -> @main::%a::aff(1)
    ; CHECK-DAG: @main::%c::aff(1) -> @main::%a::aff(1)
    ; CHECK-DAG: @main::%a.p0i32 -> @main::%a::aff(1)
    ; CHECK-DAG: @main::%b.p1i32 -> @main::%b::aff(1)

    %a.p0i32 = bitcast i8* %a to i32*
    %b.p1i32 = bitcast i8* %b to i32**

    store i32 42, i32* %a.p0i32
    store i32* %a.p0i32, i32** %b.p1i32

    call void @llvm.memcpy.p0i8.p0i8.i32(i8* %c, i8* %b, i32 8, i1 0)

    ret i32 0
}

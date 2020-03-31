; RUN: %opt -S < %s 2>&1 | FileCheck %s

; declare void @llvm.memcpy.p0i8.p0i8.i32(i8*, i8*, i32, i1)
declare void @unknown(i8*, i8*)

@could.be.here = global i32 0

; the pointee of mem %a and %c would blow up
; since we don't know what @unknown would do
; CHECK-DAG: @main::%a::aff(1) -> @could.be.here::aff(1)
; CHECK-DAG: @main::%c -> @could.be.here::aff(1)
; CHECK-DAG: @main::%d -> @could.be.here::aff(1)
define i32 @main() {
entry:
    %a = alloca i32*
    %b = alloca i64

    store i64 1234, i64* %b
    store i32* inttoptr (i32 1234 to i32*), i32** %a

    %c = load i32*, i32** %a

    %d = bitcast i32* %c to i8*
    %e = bitcast i64* %b to i8*
    call void @unknown(i8* %d, i8* %e)

    ret i32 0
}

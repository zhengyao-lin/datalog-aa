; RUN: %opt -S < %s 2>&1 | FileCheck %s

@not.me = global i32 0

; CHECK-DAG: @main::%c -> @allocate::%a::aff(1)
; CHECK-DAG: @allocate::%v -> @main::%b::aff(1)
; CHECK-DAG: @allocate::%r -> @allocate::%a::aff(1)

define i32* @allocate(i32* %v) {
entry:
    %a = alloca i32
    br label %end

branch:
    %d = getelementptr i32, i32* %v, i32 0
    br label %end

end:
    %r = phi i32* [%d, %branch], [%a, %entry]
    ret i32* %r
}

define i32 @main() {
entry:
    %b = alloca i32
    %c = call i32* @allocate(i32* %b)
    ret i32 0
}

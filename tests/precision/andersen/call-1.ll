; RUN: %opt -S -datalog-aa -datalog-aa-print-pts-to < %s 2>&1 | FileCheck %s
; same program as safety/call-1.ll

@not.me = global i32 0

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

; should not be too conservative in this case
; CHECK-NOT: @main::%c -> @not.me::aff(1)
; CHECK-NOT: @allocate::%r -> @not.me::aff(1)

define i32 @main() {
entry:
    %b = alloca i32
    %c = call i32* @allocate(i32* %b)
    ret i32 0
}

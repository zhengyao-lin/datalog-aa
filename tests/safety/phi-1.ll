; RUN: %opt -S -datalog-aa -datalog-aa-print-pts-to < %s 2>&1 | FileCheck %s

define i32 @main() {
entry:
    %a = alloca i32
    br label %end

second:
    %b = alloca i32
    br label %end

end:
    ; CHECK-DAG: @main::%b -> @main::%b::aff(1)
    ; CHECK-DAG: @main::%a -> @main::%a::aff(1)
    ; CHECK-DAG: @main::%c -> @main::%b::aff(1)
    ; CHECK-DAG: @main::%c -> @main::%a::aff(1)

    %c = phi i32* [%a, %entry], [%b, %second]
    ret i32 0
}

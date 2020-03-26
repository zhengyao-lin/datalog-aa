; RUN: %opt -S -datalog-aa -datalog-aa-print-pts-to < %s 2>&1 | FileCheck %s

@str.1 = constant [14 x i8] c"string object\00"
@str.1.p = global i8* getelementptr inbounds ([14 x i8], [14 x i8]* @str.1, i32 0, i32 0)

; CHECK-DAG: @str.1 -> @str.1::aff(1)
; CHECK-DAG: @str.1.p -> @str.1.p::aff(1)
; CHECK-DAG: @str.1.p::aff(1) -> @str.1::aff(1)

@str.2 = global [14 x i8] c"string object\00"
; @str.2 should not be an alias of @str.1
; CHECK-DAG: @str.2 -> @str.2::aff(1)

%t1 = type { i8* }
%t2 = type { %t1*, i8** }

@struct.1 = global %t1 { i8* getelementptr inbounds ([14 x i8], [14 x i8]* @str.1, i32 0, i32 0) }
; CHECK-DAG: @struct.1 -> @struct.1::aff(1)
; CHECK-DAG: @struct.1::aff(1) -> @str.1::aff(1)

@struct.2 = global %t2 { %t1* @struct.1, i8** @str.1.p }
; CHECK-DAG: @struct.2 -> @struct.2::aff(1)
; CHECK-DAG: @struct.2::aff(1) -> @struct.1::aff(1)
; CHECK-DAG: @struct.2::aff(1) -> @str.1.p::aff(1)

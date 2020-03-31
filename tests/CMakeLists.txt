add_test(
    NAME test-safety-precision
    COMMAND
        env "OPT=opt -load $<TARGET_FILE:DatalogAA> -datalog-aa -datalog-aa-print-points-to"
        llvm-lit -v ${CMAKE_CURRENT_SOURCE_DIR}
)

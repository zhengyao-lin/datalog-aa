add_test(
    NAME test-safety-precision
    COMMAND
        env "OPT=opt -load $<TARGET_FILE:DatalogAA>"
        llvm-lit -v ${CMAKE_CURRENT_SOURCE_DIR}
)

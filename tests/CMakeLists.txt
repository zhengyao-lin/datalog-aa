add_test(
    NAME test-safety
    COMMAND
        env "OPT=opt -load $<TARGET_FILE:DatalogAA>"
        llvm-lit -v ${CMAKE_CURRENT_SOURCE_DIR}
)

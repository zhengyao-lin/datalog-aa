file(GLOB src "*.cpp" "Analysis/*.cpp")

add_llvm_library(DatalogAA MODULE ${src})

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 14)

set_target_properties(DatalogAA PROPERTIES PREFIX "")
target_include_directories(DatalogAA PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# TODO: fix this
# target_link_libraries(DatalogAA PRIVATE LLVMSupport)

# link z3
find_package(Z3 REQUIRED CONFIG)
target_include_directories(DatalogAA PRIVATE ${Z3_CXX_INCLUDE_DIRS})
target_link_libraries(DatalogAA PRIVATE ${Z3_LIBRARIES})

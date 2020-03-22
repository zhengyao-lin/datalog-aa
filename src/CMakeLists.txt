find_package(Z3 REQUIRED CONFIG)

file(GLOB src "*.cpp")

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 14)

add_library(DatalogAA MODULE ${src})

target_include_directories(DatalogAA PRIVATE ${Z3_CXX_INCLUDE_DIRS})
target_link_libraries(DatalogAA PRIVATE ${Z3_LIBRARIES})

target_compile_features(DatalogAA PRIVATE cxx_range_for cxx_auto_type)
set_target_properties(DatalogAA PROPERTIES COMPILE_FLAGS "-fno-rtti")
set_target_properties(DatalogAA PROPERTIES PREFIX "")

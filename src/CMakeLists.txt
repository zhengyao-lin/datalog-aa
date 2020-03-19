add_library(DatalogAA MODULE
  DatalogAAPass.cpp
)

target_compile_features(DatalogAA PRIVATE cxx_range_for cxx_auto_type)
set_target_properties(DatalogAA PROPERTIES COMPILE_FLAGS "-fno-rtti")
set_target_properties(DatalogAA PROPERTIES PREFIX "")

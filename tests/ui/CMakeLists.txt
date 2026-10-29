add_executable(rb_ui_test ui_test.cpp)
target_link_libraries(rb_ui_test PRIVATE rb_ui)
add_test(NAME rb_ui_roundtrip COMMAND rb_ui_test)

add_executable(rb_audio_test test.cpp)
target_link_libraries(rb_audio_test PRIVATE rb_audio)
add_test(NAME rb_audio COMMAND rb_audio_test ${CMAKE_CURRENT_BINARY_DIR}/mix.wav)
set_tests_properties(rb_audio PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")

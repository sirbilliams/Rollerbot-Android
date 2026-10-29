# rb_platform: SDL2 window + GLES2 context, fixed-step loop, input, widescreen layout, screenshots.
add_library(rb_platform STATIC Platform.cpp Layout.cpp Png.cpp StubGame.cpp)
target_include_directories(rb_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(rb_platform PUBLIC SDL2::SDL2 ${RB_GLES2})
if(RB_ANDROID)
  target_link_libraries(rb_platform PUBLIC ${RB_LOG})
endif()
target_compile_options(rb_platform PRIVATE -Wall -Wextra)

if(RB_ANDROID)
  # SDLActivity loads libSDL2.so then this library and calls SDL_main.
  add_library(rollerbot SHARED main.cpp)
  target_link_libraries(rollerbot PRIVATE rb_platform rb_game)
elseif(RB_DESKTOP)
  add_executable(rollerbot main.cpp)
  target_link_libraries(rollerbot PRIVATE rb_platform rb_game)
  install(TARGETS rollerbot RUNTIME DESTINATION bin)
endif()

if(RB_TESTS AND NOT RB_ANDROID)
  add_executable(rb_platform_test ${CMAKE_SOURCE_DIR}/tests/platform/platform_test.cpp)
  target_link_libraries(rb_platform_test PRIVATE rb_platform)
  add_test(NAME platform COMMAND rb_platform_test)
endif()

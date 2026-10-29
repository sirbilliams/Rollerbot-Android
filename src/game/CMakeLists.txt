# rb_game: Rollerbot Game implementation (BrainVM + rb::Scene + rb::Audio). Headers: Game.h.
add_library(rb_game STATIC Game.cpp BrainHost.cpp Save.cpp MenuRaster.cpp SoundBank.cpp)
target_compile_features(rb_game PUBLIC cxx_std_17)
target_include_directories(rb_game PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(rb_game PUBLIC rb_brain rb_assets rb_scene rb_audio rb_platform rb_ui SDL2::SDL2)

if(RB_TESTS AND NOT RB_ANDROID)
  add_executable(rb_save_test ${CMAKE_SOURCE_DIR}/tests/save/save_test.cpp)
  target_link_libraries(rb_save_test PRIVATE rb_game)
  add_test(NAME save COMMAND rb_save_test)
endif()

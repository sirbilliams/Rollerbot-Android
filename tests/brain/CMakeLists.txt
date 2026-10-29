add_executable(rbbrain rbbrain.cpp)
target_link_libraries(rbbrain PRIVATE rb_brain)
add_executable(brain_units units.cpp)
target_link_libraries(brain_units PRIVATE rb_brain)
add_test(NAME brain_units COMMAND brain_units)
if(DEFINED ENV{RB_GAME_DIR})
  add_test(NAME brain_level001_stable
    COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:rbbrain> -DGAME=$ENV{RB_GAME_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/stable.cmake)
endif()

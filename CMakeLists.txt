cmake_minimum_required(VERSION 3.16)
project(Rollerbot LANGUAGES CXX)
include(CTest)
# Each lane owns src/<lane>; the platform lane takes this file over.
foreach(lane assets scene brain audio ui platform)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/${lane}/CMakeLists.txt)
    add_subdirectory(src/${lane})
  endif()
endforeach()

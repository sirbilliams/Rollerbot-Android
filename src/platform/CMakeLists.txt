# rb_platform: SDL2 window + GLES2 context, fixed-step loop, input, widescreen layout, screenshots.
add_library(rb_platform STATIC Platform.cpp Layout.cpp Png.cpp StubGame.cpp Bridge.cpp)
target_include_directories(rb_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(rb_platform PUBLIC SDL2::SDL2 ${RB_GLES2})
if(RB_ANDROID)
  target_link_libraries(rb_platform PUBLIC ${RB_LOG})
endif()
target_compile_options(rb_platform PRIVATE -Wall -Wextra)

if(RB_ANDROID)
  # SDLActivity loads libSDL2.so then this library and calls SDL_main.
  add_library(rollerbot SHARED main.cpp BridgeJNI.cpp)
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
  # Compile the Android JNI glue against a stand-in jni.h so broken edits fail here, not only in the APK job.
  add_library(rb_jni_check OBJECT BridgeJNI.cpp)
  target_include_directories(rb_jni_check PRIVATE ${CMAKE_SOURCE_DIR}/tests/platform/jni_stub ${CMAKE_SOURCE_DIR}/src)
endif()

# Disc importer: InstallShield cabinet reader (unshield, MIT; fetch-deps.sh) + ISO/BIN reader.
# Android: librbimport.so (JNI, used by the launcher). Desktop: the rb_import tool.
set(RB_UNSHIELD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../android/app/jni/unshield" CACHE PATH "unshield source dir")
if(EXISTS ${RB_UNSHIELD_DIR}/lib/libunshield.h)
  find_package(ZLIB REQUIRED)
  file(GLOB RB_US_SRC ${RB_UNSHIELD_DIR}/lib/*.c)
  add_library(rb_import STATIC import/RbImport.c ${RB_US_SRC}
              ${RB_UNSHIELD_DIR}/lib/convert_utf/ConvertUTF.c ${RB_UNSHIELD_DIR}/lib/md5/md5c.c)
  target_include_directories(rb_import PUBLIC import PRIVATE import/config ${RB_UNSHIELD_DIR}/lib ${RB_UNSHIELD_DIR})
  target_compile_options(rb_import PRIVATE -w)
  set_target_properties(rb_import PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 99)
  target_link_libraries(rb_import PUBLIC ZLIB::ZLIB)
  if(RB_ANDROID)
    add_library(rbimport SHARED import/RbImportJNI.c)
    target_link_libraries(rbimport PRIVATE rb_import ${RB_LOG})
  else()
    add_executable(rb_import_tool import/rb_import_main.c)
    set_target_properties(rb_import_tool PROPERTIES OUTPUT_NAME rb_import)
    target_link_libraries(rb_import_tool PRIVATE rb_import)
  endif()
endif()

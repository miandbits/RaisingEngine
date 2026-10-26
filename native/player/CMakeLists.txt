cmake_minimum_required(VERSION 3.20)
project(raising_player VERSION 0.1.0 LANGUAGES C CXX)
if(APPLE)
  enable_language(OBJC)
  enable_language(OBJCXX)
endif()
set(CMAKE_CXX_STANDARD 17)
option(RAISING_PORTABLE_SAVE_TESTING "Enable explicit portable-save backend injection for test binaries" OFF)
set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
if(ANDROID)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  set(SDL_SHARED ON CACHE BOOL "" FORCE)
  set(SDL_STATIC OFF CACHE BOOL "" FORCE)
else()
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
endif()
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  # SDL 3.4 implements the scene lifecycle required by recent iOS SDKs.
  add_subdirectory(vendor/SDL-ios vendor/SDL EXCLUDE_FROM_ALL)
else()
  add_subdirectory(vendor/SDL EXCLUDE_FROM_ALL)
endif()
set(SDLIMAGE_INSTALL OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_WEBP ON CACHE BOOL "" FORCE)
set(SDLIMAGE_VENDORED ON CACHE BOOL "" FORCE)
set(SDLIMAGE_BACKEND_STB ON CACHE BOOL "" FORCE)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  # Use the same portable PNG/JPEG decoder as the other mobile build.
  set(SDLIMAGE_BACKEND_IMAGEIO OFF CACHE BOOL "" FORCE)
endif()
set(SDLIMAGE_WEBP_SHARED OFF CACHE BOOL "" FORCE)
foreach(option ANIM_UTILS CWEBP DWEBP GIF2WEBP IMG2WEBP VWEBP WEBPINFO LIBWEBPMUX WEBPMUX EXTRAS)
  set(WEBP_BUILD_${option} OFF CACHE BOOL "" FORCE)
endforeach()
set(SDLIMAGE_TIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_JXL OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/SDL_image EXCLUDE_FROM_ALL)
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
add_subdirectory(vendor/freetype EXCLUDE_FROM_ALL)
add_library(Freetype::Freetype ALIAS freetype)
set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
set(RMLUI_TESTS OFF CACHE BOOL "" FORCE)
set(RMLUI_LUA_BINDINGS OFF CACHE BOOL "" FORCE)
set(RMLUI_PRECOMPILED_HEADERS OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/RmlUi EXCLUDE_FROM_ALL)
add_subdirectory(.. core EXCLUDE_FROM_ALL)
set(RAISING_PLAYER_SOURCES main.cpp audio.c
  vendor/RmlUi/Backends/RmlUi_Platform_SDL.cpp
  vendor/RmlUi/Backends/RmlUi_Renderer_SDL.cpp)
if(ANDROID)
  add_library(raising-player SHARED ${RAISING_PLAYER_SOURCES})
  set_target_properties(raising-player PROPERTIES OUTPUT_NAME main)
  set_target_properties(raising_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
else()
  add_executable(raising-player ${RAISING_PLAYER_SOURCES})
  set_target_properties(raising-player PROPERTIES OUTPUT_NAME RaisingPlayer)
  # SDL's Windows entry point converts the wide command line to UTF-8.
  # A CRT main() uses the active code page and corrupts non-ASCII bundle/save paths.
  if(NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_compile_definitions(raising-player PRIVATE SDL_MAIN_HANDLED)
  endif()
  if(APPLE)
    set_target_properties(raising-player PROPERTIES MACOSX_BUNDLE TRUE
      MACOSX_BUNDLE_GUI_IDENTIFIER "org.raisingengine.player"
      MACOSX_BUNDLE_BUNDLE_NAME "Raising Player"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0")
  endif()
endif()
target_include_directories(raising-player PRIVATE ../vendor vendor/RmlUi/Backends)
target_compile_definitions(raising-player PRIVATE RMLUI_SDL_VERSION_MAJOR=3)
target_link_libraries(raising-player PRIVATE raising_core RmlUi::RmlUi SDL3::SDL3 SDL3_image::SDL3_image-static)
# The injectable backend is absent from shipping players, even during host tests.
# Its separate console executable reuses the already-built desktop dependencies.
if(RAISING_PORTABLE_SAVE_TESTING AND NOT ANDROID AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS"
    AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/native-player/portable-save-host.test.cpp")
  add_executable(raising-portable-host-test
    ../../tests/native-player/portable-save-host.test.cpp audio.c
    vendor/RmlUi/Backends/RmlUi_Platform_SDL.cpp
    vendor/RmlUi/Backends/RmlUi_Renderer_SDL.cpp)
  target_include_directories(raising-portable-host-test PRIVATE . ../vendor vendor/RmlUi/Backends)
  target_compile_definitions(raising-portable-host-test PRIVATE
    RAISING_PORTABLE_SAVE_TESTING=1 SDL_MAIN_HANDLED RMLUI_SDL_VERSION_MAJOR=3)
  target_link_libraries(raising-portable-host-test PRIVATE raising_core RmlUi::RmlUi SDL3::SDL3 SDL3_image::SDL3_image-static)
  if(MINGW)
    target_compile_options(raising-portable-host-test PRIVATE -finput-charset=UTF-8)
    target_link_options(raising-portable-host-test PRIVATE -static -static-libgcc -static-libstdc++)
  endif()
endif()
if(MINGW)
  target_compile_options(raising-player PRIVATE -finput-charset=UTF-8)
  target_link_options(raising-player PRIVATE -mwindows -static -static-libgcc -static-libstdc++)
  # The redistributable runtime is branded before signing. PE resource editing
  # cannot relocate MinGW's COFF symbol table; omit it from Release binaries.
  # Keep symbols in Debug and RelWithDebInfo builds for native debugging.
  target_link_options(raising-player PRIVATE "$<$<CONFIG:Release>:-s>")
endif()
add_custom_command(TARGET raising-player POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:raising-player>/fonts)
if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set_target_properties(raising-player PROPERTIES OBJCXX_STANDARD 17 OBJCXX_STANDARD_REQUIRED YES)
  target_sources(raising-player PRIVATE platform-macos.mm)
  set_source_files_properties(platform-macos.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
  target_link_libraries(raising-player PRIVATE "-framework Cocoa")
  if(TARGET raising-portable-host-test)
    target_sources(raising-portable-host-test PRIVATE platform-macos.mm)
    set_target_properties(raising-portable-host-test PROPERTIES OBJCXX_STANDARD 17 OBJCXX_STANDARD_REQUIRED YES)
    target_link_libraries(raising-portable-host-test PRIVATE "-framework Cocoa")
  endif()
  add_custom_command(TARGET raising-player POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_BUNDLE_CONTENT_DIR:raising-player>/Resources/fonts)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set_target_properties(raising-player PROPERTIES OBJCXX_STANDARD 17 OBJCXX_STANDARD_REQUIRED YES)
  target_sources(raising-player PRIVATE portable-save-files-ios.mm ios-text-input.mm)
  set_source_files_properties(portable-save-files-ios.mm ios-text-input.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
  target_compile_definitions(raising-player PRIVATE RAISING_PORTABLE_SAVE_IOS=1)
  set_source_files_properties(audio.c PROPERTIES LANGUAGE OBJC)
  target_link_libraries(raising-player PRIVATE "-framework AVFoundation" "-framework UIKit" "-framework CoreText" "-framework Foundation" "-framework UniformTypeIdentifiers")
  set(RAISING_APP_ID "org.raisingengine.player" CACHE STRING "iOS application identifier")
  set(RAISING_IOS_VERSION "0.1.0" CACHE STRING "iOS release version")
  set(RAISING_IOS_BUILD_NUMBER "1" CACHE STRING "iOS build number")
  if(NOT RAISING_IOS_VERSION MATCHES "^[0-9]+[.][0-9]+[.][0-9]+$")
    message(FATAL_ERROR "RAISING_IOS_VERSION requires three numeric components, for example 1.2.0")
  endif()
  if(NOT RAISING_IOS_BUILD_NUMBER MATCHES "^[0-9]+([.][0-9]+)?([.][0-9]+)?$")
    message(FATAL_ERROR "RAISING_IOS_BUILD_NUMBER requires one to three numeric components")
  endif()
  set_target_properties(raising-player PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "${RAISING_APP_ID}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${RAISING_IOS_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${RAISING_IOS_BUILD_NUMBER}"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/../mobile/ios/Info.plist.in"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${RAISING_APP_ID}"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
    XCODE_GENERATE_SCHEME TRUE)
  if(RAISING_GAME_BUNDLE)
    add_custom_command(TARGET raising-player POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${RAISING_GAME_BUNDLE}" $<TARGET_BUNDLE_CONTENT_DIR:raising-player>/game)
  endif()
  if(RAISING_IOS_RELEASE_NAME_SCRIPT)
    add_custom_command(TARGET raising-player POST_BUILD COMMAND ${CMAKE_COMMAND}
      "-DAPP_BUNDLE=$<TARGET_BUNDLE_DIR:raising-player>" -P "${RAISING_IOS_RELEASE_NAME_SCRIPT}" VERBATIM)
  endif()
endif()

cmake_minimum_required(VERSION 3.21)
project(cutout_animation LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(CUTOUT_BUILD_STUDIO "Build the Qt authoring module" OFF)
set(RAISING_ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "Engine root (for native/vendor/json.hpp)")

# Qt/SDL independent evaluation and playback core.
add_library(cutout_core STATIC core/src/rig.cpp core/src/runtime.cpp core/src/host.cpp core/src/signals.cpp
  core/src/editing.cpp core/src/templates.cpp core/src/curve.cpp core/src/mesh.cpp)
target_include_directories(cutout_core PUBLIC core/include "${RAISING_ENGINE_ROOT}/native/vendor")
set_target_properties(cutout_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MINGW)
  target_compile_options(cutout_core PRIVATE -finput-charset=UTF-8)
endif()

if(EMSCRIPTEN)
  # Refused ops (unknown skin/clip, EditError...) are thrown and answered {ok:false}; without
  # catching enabled Emscripten aborts the whole module instead (service/LLM edit path, players).
  target_compile_options(cutout_core PUBLIC -fexceptions)
  add_executable(cutout-runtime core/src/api.cpp)
  target_link_libraries(cutout-runtime PRIVATE cutout_core)
  set_target_properties(cutout-runtime PROPERTIES SUFFIX ".mjs")
  target_link_options(cutout-runtime PRIVATE -fexceptions -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web,node -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1MB
    -sEXPORTED_FUNCTIONS=_raising_extension_request,_raising_extension_api_version,_malloc,_free -sEXPORTED_RUNTIME_METHODS=ccall,cwrap -sEXPORT_NAME=createCutoutRuntime)
  return()
endif()

# Windows runtime module: C ABI only, so the host player never shares C++ ABI.
add_library(cutout-runtime SHARED core/src/api.cpp)
target_link_libraries(cutout-runtime PRIVATE cutout_core)
target_compile_definitions(cutout-runtime PRIVATE CUTOUT_BUILD_DLL)
set_target_properties(cutout-runtime PROPERTIES PREFIX "")
if(MINGW)
  target_link_options(cutout-runtime PRIVATE -static -static-libgcc -static-libstdc++)
endif()

add_executable(cutout-core-tests core/tests/core-tests.cpp)
target_link_libraries(cutout-core-tests PRIVATE cutout_core)
if(MINGW)
  target_link_options(cutout-core-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()

if(CUTOUT_BUILD_STUDIO)
  add_subdirectory(studio)
endif()

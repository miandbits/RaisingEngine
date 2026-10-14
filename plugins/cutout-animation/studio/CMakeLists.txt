# Qt authoring module (QPluginLoader DLL) + QtTest driving the real panel widgets.
# Built only with -DCUTOUT_BUILD_STUDIO=ON. Same Qt/MinGW as the studio: Qt and libstdc++
# stay dynamic because StudioHost passes Qt/std types across the plugin boundary.
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Test)
set(CMAKE_AUTOMOC ON)

add_library(cutout_studio_obj OBJECT
  src/rig-render.cpp src/rig-canvas.cpp src/dopesheet.cpp src/cutout-panel.cpp src/cutout-plugin.cpp
  src/rig-render.hpp src/rig-canvas.hpp src/dopesheet.hpp src/cutout-panel.hpp src/cutout-plugin.hpp)
target_include_directories(cutout_studio_obj PUBLIC src "${RAISING_ENGINE_ROOT}/native/studio")
target_link_libraries(cutout_studio_obj PUBLIC Qt6::Widgets cutout_core)
# Qt keywords (signals/slots/emit) would clash with plain identifiers in shared core headers.
target_compile_definitions(cutout_studio_obj PUBLIC QT_NO_KEYWORDS)
set_target_properties(cutout_studio_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MINGW)
  target_compile_options(cutout_studio_obj PRIVATE -finput-charset=UTF-8)
endif()

add_library(cutout-studio SHARED)
target_link_libraries(cutout-studio PRIVATE cutout_studio_obj)
set_target_properties(cutout-studio PROPERTIES PREFIX "" OUTPUT_NAME "cutout-studio")

add_executable(cutout-studio-tests tests/studio-tests.cpp)
target_link_libraries(cutout-studio-tests PRIVATE cutout_studio_obj Qt6::Test)

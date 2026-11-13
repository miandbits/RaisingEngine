cmake_minimum_required(VERSION 3.25)
project(IsolatedQtCocoaFix LANGUAGES C CXX OBJCXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OBJCXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.12.0 EXACT REQUIRED COMPONENTS Core Gui Widgets Test CorePrivate GuiPrivate)
if(NOT QTBASE_SOURCE_DIR OR NOT IS_DIRECTORY "${QTBASE_SOURCE_DIR}/src/plugins/platforms/cocoa")
 message(FATAL_ERROR "Set QTBASE_SOURCE_DIR to the verified and patched Qt6.12.0 source")
endif()
if(NOT COMPILE_HEADERS_DIR OR NOT EXISTS "${COMPILE_HEADERS_DIR}/MoltenVK/mvk_vulkan.h")
 message(FATAL_ERROR "Set COMPILE_HEADERS_DIR to the verified compile-only headers")
endif()
set(COCOA "${QTBASE_SOURCE_DIR}/src/plugins/platforms/cocoa")
qt_add_plugin(QCocoaIntegrationPlugin SHARED PLUGIN_TYPE platforms CLASS_NAME QCocoaIntegrationPlugin OUTPUT_NAME qcocoa)
target_sources(QCocoaIntegrationPlugin PRIVATE
 "${COCOA}/main.mm"
 "${COCOA}/qcocoaapplication.h"
 "${COCOA}/qcocoaapplication.mm"
 "${COCOA}/qcocoaapplicationdelegate.h"
 "${COCOA}/qcocoaapplicationdelegate.mm"
 "${COCOA}/qcocoabackingstore.h"
 "${COCOA}/qcocoabackingstore.mm"
 "${COCOA}/qcocoaclipboard.h"
 "${COCOA}/qcocoaclipboard.mm"
 "${COCOA}/qcocoacursor.h"
 "${COCOA}/qcocoacursor.mm"
 "${COCOA}/qcocoadrag.h"
 "${COCOA}/qcocoadrag.mm"
 "${COCOA}/qcocoaeventdispatcher.h"
 "${COCOA}/qcocoaeventdispatcher.mm"
 "${COCOA}/qcocoahelpers.h"
 "${COCOA}/qcocoahelpers.mm"
 "${COCOA}/qcocoainputcontext.h"
 "${COCOA}/qcocoainputcontext.mm"
 "${COCOA}/qcocoaintegration.h"
 "${COCOA}/qcocoaintegration.mm"
 "${COCOA}/qcocoaintrospection.h"
 "${COCOA}/qcocoaintrospection.mm"
 "${COCOA}/qcocoamenu.h"
 "${COCOA}/qcocoamenu.mm"
 "${COCOA}/qcocoamenubar.h"
 "${COCOA}/qcocoamenubar.mm"
 "${COCOA}/qcocoamenuitem.h"
 "${COCOA}/qcocoamenuitem.mm"
 "${COCOA}/qcocoamenuloader.h"
 "${COCOA}/qcocoamenuloader.mm"
 "${COCOA}/qcocoamimetypes.h"
 "${COCOA}/qcocoamimetypes.mm"
 "${COCOA}/qcocoanativeinterface.h"
 "${COCOA}/qcocoanativeinterface.mm"
 "${COCOA}/qcocoansmenu.h"
 "${COCOA}/qcocoansmenu.mm"
 "${COCOA}/qcocoascreen.h"
 "${COCOA}/qcocoascreen.mm"
 "${COCOA}/qcocoaservices.h"
 "${COCOA}/qcocoaservices.mm"
 "${COCOA}/qcocoasystemtrayicon.h"
 "${COCOA}/qcocoasystemtrayicon.mm"
 "${COCOA}/qcocoatheme.h"
 "${COCOA}/qcocoatheme.mm"
 "${COCOA}/qcocoawindow.h"
 "${COCOA}/qcocoawindow.mm"
 "${COCOA}/qcocoawindowmanager.h"
 "${COCOA}/qcocoawindowmanager.mm"
 "${COCOA}/qiosurfacegraphicsbuffer.h"
 "${COCOA}/qiosurfacegraphicsbuffer.mm"
 "${COCOA}/qmacclipboard.h"
 "${COCOA}/qmacclipboard.mm"
 "${COCOA}/qmultitouch_mac.mm"
 "${COCOA}/qmultitouch_mac_p.h"
 "${COCOA}/qnsview.h"
 "${COCOA}/qnsview.mm"
 "${COCOA}/qnswindow.h"
 "${COCOA}/qnswindow.mm"
 "${COCOA}/qnswindowdelegate.h"
 "${COCOA}/qnswindowdelegate.mm"
 "${COCOA}/qcocoacolordialoghelper.h"
 "${COCOA}/qcocoacolordialoghelper.mm"
 "${COCOA}/qcocoafiledialoghelper.h"
 "${COCOA}/qcocoafiledialoghelper.mm"
 "${COCOA}/qcocoafontdialoghelper.h"
 "${COCOA}/qcocoafontdialoghelper.mm"
 "${COCOA}/qcocoamessagedialog.h"
 "${COCOA}/qcocoamessagedialog.mm"
)
target_include_directories(QCocoaIntegrationPlugin PRIVATE "${COCOA}")
target_include_directories(QCocoaIntegrationPlugin PRIVATE "${COMPILE_HEADERS_DIR}")
target_compile_definitions(QCocoaIntegrationPlugin PRIVATE QT_NO_FOREACH)
target_link_libraries(QCocoaIntegrationPlugin PRIVATE Qt6::Core Qt6::CorePrivate Qt6::Gui Qt6::GuiPrivate)
foreach(fw Foundation AppKit Carbon CoreServices CoreVideo IOKit IOSurface Metal QuartzCore UniformTypeIdentifiers ScreenCaptureKit)
 find_library(FW_${fw} ${fw} REQUIRED)
 target_link_libraries(QCocoaIntegrationPlugin PRIVATE ${FW_${fw}})
endforeach()
foreach(feature opengl vulkan accessibility sessionmanager)
 message(STATUS "Qt feature ${feature}=${QT_FEATURE_${feature}}")
endforeach()
if(QT_FEATURE_opengl)
 target_sources(QCocoaIntegrationPlugin PRIVATE "${COCOA}/qcocoaglcontext.h" "${COCOA}/qcocoaglcontext.mm")
endif()
if(QT_FEATURE_vulkan)
 target_sources(QCocoaIntegrationPlugin PRIVATE "${COCOA}/qcocoavulkaninstance.h" "${COCOA}/qcocoavulkaninstance.mm")
endif()
if(QT_FEATURE_accessibility)
 target_sources(QCocoaIntegrationPlugin PRIVATE "${COCOA}/qcocoaaccessibility.h" "${COCOA}/qcocoaaccessibility.mm" "${COCOA}/qcocoaaccessibilityelement.h" "${COCOA}/qcocoaaccessibilityelement.mm")
else()
 message(FATAL_ERROR "Accessibility must remain enabled")
endif()
if(QT_FEATURE_sessionmanager)
 target_sources(QCocoaIntegrationPlugin PRIVATE "${COCOA}/qcocoasessionmanager.cpp" "${COCOA}/qcocoasessionmanager.h")
endif()
qt_add_resources(QCocoaIntegrationPlugin qcocoaresources PREFIX "/qt-project.org/mac/cursors" BASE "${COCOA}" FILES "${COCOA}/images/sizeallcursor.png" "${COCOA}/images/spincursor.png" "${COCOA}/images/waitcursor.png")
set_target_properties(QCocoaIntegrationPlugin PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/platforms" SUFFIX ".dylib")
qt_add_executable(tst_qaccessibilitymac "${QTBASE_SOURCE_DIR}/tests/auto/other/qaccessibilitymac/tst_qaccessibilitymac.mm")
target_link_libraries(tst_qaccessibilitymac PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test "-framework AppKit" "-framework ApplicationServices" "-framework Security")

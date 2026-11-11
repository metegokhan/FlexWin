cmake_minimum_required(VERSION 3.20)
project(FlexWin LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT DEFINED FLEXWIN_ARCH)
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(FLEXWIN_ARCH "x86")
    else()
        set(FLEXWIN_ARCH "x64")
    endif()
endif()

add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

if(MSVC)
    add_compile_options(/W4 /utf-8 /permissive-)
endif()

set(CORE_SOURCES
    src/json/Json.cpp
    src/core/I18n.cpp
    src/core/Logger.cpp
    src/core/MonitorManager.cpp
    src/core/WindowInspector.cpp
    src/core/ConfigManager.cpp
    src/core/HookBridge.cpp
    src/core/WindowEngine.cpp
    src/core/HotkeyManager.cpp
)

if(FLEXWIN_ARCH STREQUAL "x64")
    add_library(FlexWinHook64 SHARED
        hook/HookMain.cpp
        hook/Hook.def
    )
    target_link_libraries(FlexWinHook64 PRIVATE user32 comctl32)

    add_executable(FlexWin WIN32
        src/main.cpp
        ${CORE_SOURCES}
        src/ui/TrayIcon.cpp
        src/ui/CrosshairPicker.cpp
        src/ui/MonitorPreview.cpp
        src/ui/OpenWindowsDialog.cpp
        src/ui/LogDialog.cpp
        src/ui/AppWindow.cpp
        src/ui/FlexWin.rc
    )
    target_link_libraries(FlexWin PRIVATE user32 gdi32 comctl32 comdlg32 shell32 dwmapi advapi32 shcore)
    set_target_properties(FlexWin PROPERTIES LINK_FLAGS "/MANIFEST:NO")

    add_executable(FlexWinTests
        tests/UnitTests.cpp
        ${CORE_SOURCES}
    )
    target_link_libraries(FlexWinTests PRIVATE user32 gdi32 comctl32 shell32 dwmapi advapi32 shcore)

    add_executable(StubbornTestApp64 WIN32
        tests/StubbornTestApp.cpp
    )
    target_link_libraries(StubbornTestApp64 PRIVATE user32 gdi32)

    add_executable(FlexWinIntegrationTest
        tests/IntegrationTest.cpp
        ${CORE_SOURCES}
    )
    target_link_libraries(FlexWinIntegrationTest PRIVATE user32 gdi32 comctl32 shell32 dwmapi advapi32 shcore)
elseif(FLEXWIN_ARCH STREQUAL "x86")
    add_library(FlexWinHook32 SHARED
        hook/HookMain.cpp
        hook/Hook.def
    )
    target_link_libraries(FlexWinHook32 PRIVATE user32 comctl32)

    add_executable(FlexWinHelper32 WIN32
        helper32/Helper32Main.cpp
    )
    target_link_libraries(FlexWinHelper32 PRIVATE user32 shell32)

    add_executable(StubbornTestApp32 WIN32
        tests/StubbornTestApp.cpp
    )
    target_link_libraries(StubbornTestApp32 PRIVATE user32 gdi32)
else()
    message(FATAL_ERROR "Unsupported FLEXWIN_ARCH: ${FLEXWIN_ARCH}")
endif()

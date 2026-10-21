cmake_minimum_required(VERSION 3.22.1)
project(cyberbox_native LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Definitions
add_definitions(-D__ANDROID__)
add_definitions(-DIMGUI_IMPL_OPENGL_ES3)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${ANDROID_NDK}/sources/android/native_app_glue
)

# Native App Glue helper provided by Android NDK
add_library(app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

# ImGui core and Android backends
file(GLOB IMGUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/imgui/*.cpp
)

# CyberBox Core Sources for Android
set(CYBERBOX_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/android_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/android_native_sandbox.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/ai_client.cpp
    ${IMGUI_SOURCES}
)

# Main Native Shared Library (.so) loaded by NativeActivity
add_library(cyberbox_native SHARED
    ${CYBERBOX_SOURCES}
)

# Target Link Libraries
target_link_libraries(cyberbox_native
    app_glue
    EGL
    GLESv3
    android
    log
)

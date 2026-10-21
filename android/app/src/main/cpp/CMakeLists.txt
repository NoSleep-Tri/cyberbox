cmake_minimum_required(VERSION 3.22.1)
project(cyberbox_native LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Definitions
add_definitions(-D__ANDROID__)

# Root of the repo is 5 levels up:
# CMAKE_CURRENT_SOURCE_DIR is: <repo>/android/app/src/main/cpp
# ../ => <repo>/android/app/src/main
# ../../ => <repo>/android/app/src
# ../../../ => <repo>/android/app
# ../../../../ => <repo>/android
# ../../../../../ => <repo>
get_filename_component(CYBERBOX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)

message(STATUS "CYBERBOX_ROOT: ${CYBERBOX_ROOT}")

# Include directories
include_directories(
    ${CYBERBOX_ROOT}/src
    ${CYBERBOX_ROOT}/include
    ${ANDROID_NDK}/sources/android/native_app_glue
)

# Native App Glue helper provided by Android NDK
add_library(app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

# CyberBox Core Sources for Android
set(CYBERBOX_SOURCES
    ${CYBERBOX_ROOT}/src/android_main.cpp
    ${CYBERBOX_ROOT}/src/android_native_sandbox.cpp
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

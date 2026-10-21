cmake_minimum_required(VERSION 3.16)
project(CyberBoxAI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include src)

set(SOURCES
    src/main.cpp
    src/app_server.cpp
    src/ai_client.cpp
    src/sandbox_manager.cpp
    src/docker_sandbox.cpp
    src/win32_jail_sandbox.cpp
)

add_executable(cyberbox_ai ${SOURCES})

if (WIN32)
    target_link_libraries(cyberbox_ai PRIVATE ws2_32 winhttp shell32 shlwapi)
endif()

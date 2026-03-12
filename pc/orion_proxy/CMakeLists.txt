cmake_minimum_required(VERSION 3.14)
project(orion_proxy C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Windows XP targeting (MSVC)
if(MSVC)
    add_definitions(-D_WIN32_WINNT=0x0501 -DWINVER=0x0501)
endif()

# Static linking option for Linux portability (optional)
option(STATIC_BUILD "Build with static linking for better portability" OFF)
if(STATIC_BUILD AND UNIX AND NOT APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
endif()

# Include the parent orion_bolid library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/orion_bolid)

# Proxy service executable
add_executable(orion_proxy
    src/main.c
    src/device_tracker.c
    src/proxy_server.c
    src/proxy_config.c
    src/orion_device_types.c
    src/http_server.c
    src/tray_icon.c
)

target_include_directories(orion_proxy PRIVATE
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(orion_proxy PRIVATE orion_bolid)

if(WIN32)
    target_link_libraries(orion_proxy PRIVATE ws2_32)
else()
    target_link_libraries(orion_proxy PRIVATE pthread)
endif()

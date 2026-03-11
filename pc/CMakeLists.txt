cmake_minimum_required(VERSION 3.14)
project(orion_bolid C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Windows XP targeting (MSVC)
if(MSVC)
    add_definitions(-D_WIN32_WINNT=0x0501 -DWINVER=0x0501)
endif()

# Library
add_library(orion_bolid STATIC
    src/orion_crc.c
    src/orion_serial.c
    src/orion_protocol.c
    src/orion_commands.c
    src/orion_sniffer.c
)

target_include_directories(orion_bolid PUBLIC include)

if(WIN32)
    target_compile_definitions(orion_bolid PRIVATE ORION_PLATFORM_WIN32)
else()
    target_compile_definitions(orion_bolid PRIVATE ORION_PLATFORM_POSIX)
endif()

# Example application
add_executable(orion_example examples/main.c)
target_link_libraries(orion_example PRIVATE orion_bolid)

if(WIN32)
    target_link_libraries(orion_bolid PRIVATE setupapi)
endif()

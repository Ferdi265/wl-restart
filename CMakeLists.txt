cmake_minimum_required(VERSION 3.24)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)

project(wl-restart C)

file(GLOB sources CONFIGURE_DEPENDS src/*.c)
add_executable(wl-restart ${sources})
target_compile_options(wl-restart PRIVATE -Wall -Wextra)

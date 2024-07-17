cmake_minimum_required(VERSION 3.24)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)

project(wl-restart C)

# options
option(INSTALL_DOCUMENTATION "install wl-restart manual pages" OFF)

# manual pages
add_subdirectory(man)
if (${BUILD_DOCUMENTATION})
    build_scdoc_man_page(wl-restart 1)
endif()

file(GLOB sources CONFIGURE_DEPENDS src/*.c)
add_executable(wl-restart ${sources})
target_compile_options(wl-restart PRIVATE -Wall -Wextra)

# installation rules
include(GNUInstallDirs)

install(TARGETS wl-restart DESTINATION "${CMAKE_INSTALL_BINDIR}")

if (${INSTALL_DOCUMENTATION})
    install_scdoc_man_page(wl-restart 1)
endif()

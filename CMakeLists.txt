cmake_minimum_required(VERSION 2.8)
project(FreeRDP C)
set(CMAKE_COLOR_MAKEFILE ON)

# Include our extra modules
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/)

# Soname versioning - 0.0.0 since it is not being managed yet
set(FREERDP_VERSION_MAJOR "0")
set(FREERDP_VERSION_MINOR "0")
set(FREERDP_VERSION_PATCH "0")
set(FREERDP_VERSION "${FREERDP_VERSION_MAJOR}.${FREERDP_VERSION_MINOR}")
set(FREERDP_VERSION_FULL "${FREERDP_VERSION}.${FREERDP_VERSION_PATCH}")

# Default to release build type
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
endif()

# Path to put keymaps
set(FREERDP_KEYMAP_PATH "${CMAKE_INSTALL_PREFIX}/freerdp/keymaps")

# Libraries
add_subdirectory(include)
add_subdirectory(libfreerdp-asn1)
add_subdirectory(libfreerdp-utils)
add_subdirectory(libfreerdp-kbd)
add_subdirectory(libfreerdp-gdi)


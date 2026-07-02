# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP Harmony native bridge cmake build script

cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()
if(NOT FREERDP_DEFAULT_PROJECT_VERSION)
  set(FREERDP_DEFAULT_PROJECT_VERSION "1.0.0.0")
endif()

project(freerdp-harmony-bridge LANGUAGES C CXX VERSION ${FREERDP_DEFAULT_PROJECT_VERSION})

add_subdirectory(native)

set_property(TARGET freerdp_harmony_bridge PROPERTY FOLDER "Client/Harmony")

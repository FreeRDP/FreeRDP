# FreeRDP: A Remote Desktop Protocol Implementation
# libfreerdp cmake build script
#
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(MODULE_NAME "freerdp")
set(MODULE_PREFIX "FREERDP")

option(FREERDP_USE_VENDOR_PRODUCT_CONFIG_DIR "Use <vendor>/<product> path for resources" OFF)
set(FREERDP_RESOURCE_ROOT ${CMAKE_INSTALL_FULL_DATAROOTDIR})
if(FREERDP_USE_VENDOR_PRODUCT_CONFIG_DIR)
  string(APPEND FREERDP_RESOURCE_ROOT "/${VENDOR}")
endif()
string(APPEND FREERDP_RESOURCE_ROOT "/${PRODUCT}")

if(WITH_RESOURCE_VERSIONING)
  string(APPEND FREERDP_RESOURCE_ROOT "${FREERDP_VERSION_MAJOR}")
endif()

# CMake modules includes
include(FindCairo)

set(LIBFREERDP_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(LIBFREERDP_SRCS "")
set(LIBFREERDP_OBJECT_LIBS "")
set(LIBFREERDP_LIBS "")
set(LIBFREERDP_INCLUDES "")
set(LIBFREERDP_DEFINITIONS "")
set(LIBFREERDP_COMPILE_OPTIONS "")

macro(freerdp_module_add)
  file(RELATIVE_PATH _relPath "${LIBFREERDP_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
  foreach(_src ${ARGN})
    if(_relPath)
      list(APPEND LIBFREERDP_SRCS "${_relPath}/${_src}")
    else()
      list(APPEND LIBFREERDP_SRCS "${_src}")
    endif()
  endforeach()
  if(_relPath)
    set(LIBFREERDP_SRCS ${LIBFREERDP_SRCS} PARENT_SCOPE)
  endif()
endmacro()

macro(freerdp_include_directory_add)
  file(RELATIVE_PATH _relPath "${LIBFREERDP_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
  foreach(_inc ${ARGN})
    if(IS_ABSOLUTE ${_inc})
      list(APPEND LIBFREERDP_INCLUDES "${_inc}")
    else()
      if(_relPath)
        list(APPEND LIBFREERDP_INCLUDES "${_relPath}/${_inc}")
      else()
        list(APPEND LIBFREERDP_INCLUDES "${_inc}")
      endif()
    endif()
  endforeach()
  if(_relPath)
    set(LIBFREERDP_INCLUDES ${LIBFREERDP_INCLUDES} PARENT_SCOPE)
  endif()
endmacro()

macro(freerdp_library_add_public)
  foreach(_lib ${ARGN})
    list(APPEND LIBFREERDP_PUB_LIBS "${_lib}")
  endforeach()
  set(LIBFREERDP_PUB_LIBS ${LIBFREERDP_PUB_LIBS} PARENT_SCOPE)
endmacro()

macro(freerdp_object_library_add)
  foreach(_lib ${ARGN})
    list(APPEND LIBFREERDP_OBJECT_LIBS "$<TARGET_OBJECTS:${_lib}>")
  endforeach()
  set(LIBFREERDP_OBJECT_LIBS ${LIBFREERDP_OBJECT_LIBS} PARENT_SCOPE)
endmacro()

macro(freerdp_library_add)
  foreach(_lib ${ARGN})
    list(APPEND LIBFREERDP_LIBS "${_lib}")
  endforeach()
  set(LIBFREERDP_LIBS ${LIBFREERDP_LIBS} PARENT_SCOPE)
endmacro()

macro(freerdp_definition_add)
  foreach(_define ${ARGN})
    list(APPEND LIBFREERDP_DEFINITIONS "${_define}")
  endforeach()
  set(LIBFREERDP_DEFINITIONS ${LIBFREERDP_DEFINITIONS} PARENT_SCOPE)
endmacro()

macro(freerdp_compile_options_add)
  foreach(_lib ${ARGN})
    list(APPEND LIBFREERDP_COMPILE_OPTIONS "${_lib}")
  endforeach()
  set(LIBFREERDP_COMPILE_OPTIONS ${LIBFREERDP_COMPILE_OPTIONS} PARENT_SCOPE)
endmacro()

option(WITH_FDK_AAC "Enable FDK_AAC support" OFF)
if(WITH_FDK_AAC)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(FDK_AAC REQUIRED fdk-aac)

  add_compile_definitions(WITH_FDK_AAC)
  include_directories(SYSTEM ${FDK_AAC_INCLUDE_DIRS})

  link_directories(${FDK_AAC_LIBRARY_DIRS})
  freerdp_library_add(${FDK_AAC_LIBRARIES})
endif()

set(OPUS_DEFAULT OFF)
if(NOT WITH_DSP_FFMPEG)
  find_package(Opus)
  if(Opus_FOUND)
    set(OPUS_DEFAULT ${OPUS_FOUND})
  else()
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
      pkg_check_modules(OPUS opus)
      set(OPUS_DEFAULT ${OPUS_FOUND})
    endif()
  endif()

  message("Using OPUS: ${OPUS_DEFAULT}")
endif()

option(WITH_OPUS "compile with opus codec support" ${OPUS_DEFAULT})
if(WITH_OPUS)
  find_package(Opus)
  if(Opus_FOUND)
    freerdp_library_add(${OPUS_LIBRARIES})
  else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(OPUS REQUIRED opus)
    freerdp_library_add(${OPUS_LIBRARIES})
    freerdp_include_directory_add(${OPUS_INCLUDE_DIRS})
    link_directories(${OPUS_LIBRARY_DIRS})
  endif()
endif()

if(WITH_SWSCALE)
  find_package(FFmpeg REQUIRED COMPONENTS SWSCALE)
endif(WITH_SWSCALE)
if(WITH_CAIRO)
  find_package(Cairo REQUIRED)
endif(WITH_CAIRO)

# Prefer SWScale over Cairo, both at the same time are not possible.
if(WITH_SWSCALE)
  include_directories(SYSTEM ${SWSCALE_INCLUDE_DIRS})
  freerdp_library_add(${SWSCALE_LIBRARIES})
endif()
if(WITH_CAIRO)
  include_directories(SYSTEM ${CAIRO_INCLUDE_DIR})
  freerdp_library_add(${CAIRO_LIBRARY})
endif()
if(NOT WITH_SWSCALE AND NOT WITH_CAIRO)
  message(WARNING "-DWITH_SWSCALE=OFF and -DWITH_CAIRO=OFF, compiling without image scaling support!")
endif()

set(${MODULE_PREFIX}_SUBMODULES emu utils common gdi cache crypto locale core)

foreach(${MODULE_PREFIX}_SUBMODULE ${${MODULE_PREFIX}_SUBMODULES})
  add_subdirectory(${${MODULE_PREFIX}_SUBMODULE})
endforeach()

if(NOT WITH_DSP_FFMPEG AND NOT WITH_FAAC AND NOT WITH_FDK_AAC)
  message(WARNING "Compiling without WITH_DSP_FFMPEG, WITH_FAAC and WITH_FDK_AAC. AAC encoder support disabled")
endif()

add_subdirectory(codec)
add_subdirectory(primitives)

if(WITH_AAD)
  if(NOT WITH_WINPR_JSON)
    message(FATAL_ERROR "Trying to build -DWITH_AAD=ON but WITH_WINPR_JSON is not defined")
  endif()
endif()

list(APPEND LIBFREERDP_PUB_LIBS winpr)

list(REMOVE_DUPLICATES LIBFREERDP_DEFINITIONS)
list(REMOVE_DUPLICATES LIBFREERDP_INCLUDES)
include_directories(SYSTEM ${LIBFREERDP_INCLUDES})

if(LIBFREERDP_OBJECT_LIBS)
  list(REMOVE_DUPLICATES LIBFREERDP_OBJECT_LIBS)
  list(APPEND LIBFREERDP_SRCS ${LIBFREERDP_OBJECT_LIBS})
endif()

addtargetwithresourcefile(${MODULE_NAME} FALSE "${FREERDP_VERSION}" LIBFREERDP_SRCS)

if(WITH_RESOURCE_VERSIONING)
  target_compile_definitions(${MODULE_NAME} PRIVATE WITH_RESOURCE_VERSIONING)
endif()
if(FREERDP_USE_VENDOR_PRODUCT_CONFIG_DIR)
  target_compile_definitions(${MODULE_NAME} PRIVATE FREERDP_USE_VENDOR_PRODUCT_CONFIG_DIR)
endif()

add_compile_definitions(${LIBFREERDP_DEFINITIONS})

if(LIBFREERDP_COMPILE_OPTIONS)
  list(REMOVE_DUPLICATES LIBFREERDP_COMPILE_OPTIONS)
  target_compile_options(${MODULE_NAME} PRIVATE ${LIBFREERDP_COMPILE_OPTIONS})
endif()

if(WITH_FULL_CONFIG_PATH)
  add_compile_definitions(WITH_FULL_CONFIG_PATH)
endif()

target_include_directories(${MODULE_NAME} INTERFACE $<INSTALL_INTERFACE:include>)
target_link_libraries(${MODULE_NAME} PRIVATE ${LIBFREERDP_LIBS})
target_link_libraries(${MODULE_NAME} PUBLIC ${LIBFREERDP_PUB_LIBS})
install(TARGETS ${MODULE_NAME} COMPONENT libraries EXPORT FreeRDPTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/libfreerdp")

include(pkg-config-install-prefix)
set(FREERDP_REQUIRES_PRIVATE "")

if(WITH_SMARTCARD_EMULATE)
  string(APPEND FREERDP_REQUIRES_PRIVATE " zlib")
  list(APPEND FREERDP_PC_PRIVATE_LIBS "-lz")
endif()
list(JOIN FREERDP_PC_PRIVATE_LIBS " " FREERDP_PC_PRIVATE_LIBS)
cleaning_configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/freerdp.pc.in ${CMAKE_CURRENT_BINARY_DIR}/freerdp${FREERDP_VERSION_MAJOR}.pc @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/freerdp${FREERDP_VERSION_MAJOR}.pc DESTINATION ${PKG_CONFIG_PC_INSTALL_DIR})

## cmake project
export(PACKAGE freerdp)

setfreerdpcmakeinstalldir(FREERDP_CMAKE_INSTALL_DIR "FreeRDP${FREERDP_VERSION_MAJOR}")

configure_package_config_file(
  FreeRDPConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/FreeRDPConfig.cmake
  INSTALL_DESTINATION ${FREERDP_CMAKE_INSTALL_DIR} PATH_VARS FREERDP_INCLUDE_DIR FREERDP_PLUGIN_PATH
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/FreeRDPConfigVersion.cmake VERSION ${FREERDP_VERSION} COMPATIBILITY SameMajorVersion
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/FreeRDPConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/FreeRDPConfigVersion.cmake
        DESTINATION ${FREERDP_CMAKE_INSTALL_DIR}
)

install(EXPORT FreeRDPTargets DESTINATION ${FREERDP_CMAKE_INSTALL_DIR})

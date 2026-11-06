# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP Shadow Server cmake build script
#
# Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#	 http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# freerdp-shadow  library

set(MODULE_NAME "freerdp-shadow")

set(SRCS
    shadow_client.c
    shadow_client.h
    shadow_autodetect.c
    shadow_autodetect.h
    shadow_lobby.c
    shadow_lobby.h
    shadow_input.c
    shadow_input.h
    shadow_screen.c
    shadow_screen.h
    shadow_surface.c
    shadow_surface.h
    shadow_encoder.c
    shadow_encoder.h
    shadow_rfx_dirty.c
    shadow_rfx_dirty.h
    shadow_capture.c
    shadow_capture.h
    shadow_channels.c
    shadow_channels.h
    shadow_encomsp.c
    shadow_encomsp.h
    shadow_remdesk.c
    shadow_remdesk.h
    shadow_rdpsnd.c
    shadow_rdpsnd.h
    shadow_audin.c
    shadow_audin.h
    shadow_cliprdr.c
    shadow_cliprdr.h
    shadow_disp.c
    shadow_disp.h
    shadow_rdpgfx.c
    shadow_rdpgfx.h
    shadow_subsystem.c
    shadow_subsystem.h
    shadow_mcevent.c
    shadow_mcevent.h
    shadow_server.c
    shadow.h
)

if(WITH_RDTK)
  if(NOT FREERDP_UNIFIED_BUILD)
    find_package(rdtk 0 REQUIRED)
    include_directories(SYSTEM ${RDTK_INCLUDE_DIR})
  else()
    include_directories(${PROJECT_SOURCE_DIR}/rdtk/include)
    include_directories(${PROJECT_BINARY_DIR}/rdtk/include)
  endif()
endif()

addtargetwithresourcefile(${MODULE_NAME} "FALSE" "${FREERDP_VERSION}" SRCS)

if(WITH_RDTK)
  target_compile_definitions(${MODULE_NAME} PRIVATE WITH_RDTK)
  list(APPEND LIBS rdtk)
endif()

list(APPEND LIBS freerdp-server)
if(WITH_WINPR_TOOLS)
  target_compile_definitions(${MODULE_NAME} PRIVATE WITH_WINPR_TOOLS)
  list(APPEND LIBS winpr-tools)
endif()

target_include_directories(${MODULE_NAME} INTERFACE $<INSTALL_INTERFACE:include/freerdp${FREERDP_VERSION_MAJOR}>)
target_link_libraries(${MODULE_NAME} PRIVATE ${LIBS})

installwithrpath(
  TARGETS
  ${MODULE_NAME}
  COMPONENT
  server
  EXPORT
  FreeRDP-ShadowTargets
  ARCHIVE
  DESTINATION
  ${CMAKE_INSTALL_LIBDIR}
  LIBRARY
  DESTINATION
  ${CMAKE_INSTALL_LIBDIR}
  RUNTIME
  DESTINATION
  ${CMAKE_INSTALL_BINDIR}
)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Server/shadow")

# subsystem library

set(MODULE_NAME "freerdp-shadow-subsystem")

set(SRCS shadow_subsystem_builtin.c)

option(WITH_SHADOW_SUBSYSTEM "Build actual shadow platform subsystem implementation" ON)
# Each subsystem is a static library; the server picks one when it starts (/subsystem)
set(SHADOW_SUBSYSTEM_LIBS "")
set(SHADOW_SUBSYSTEM_DEFS "")
if(WITH_SHADOW_SUBSYSTEM)
  if(WIN32)
    add_subdirectory(Win)
    list(APPEND SHADOW_SUBSYSTEM_LIBS freerdp-shadow-subsystem-impl)
  elseif(APPLE)
    if(NOT IOS)
      add_subdirectory(Mac)
      list(APPEND SHADOW_SUBSYSTEM_LIBS freerdp-shadow-subsystem-impl)
    endif()
  else()
    option(WITH_SHADOW_X11 "Share an X11 display" ON)
    # GNOME, also on Wayland: Mutter's ScreenCast and RemoteDesktop APIs
    option(WITH_SHADOW_MUTTER "Share a GNOME session through Mutter" OFF)
    if(NOT WITH_SHADOW_X11 AND NOT WITH_SHADOW_MUTTER)
      message(FATAL_ERROR "WITH_SHADOW_SUBSYSTEM needs WITH_SHADOW_X11 or WITH_SHADOW_MUTTER")
    endif()
    if(WITH_SHADOW_MUTTER)
      add_subdirectory(Mutter)
      list(APPEND SHADOW_SUBSYSTEM_LIBS freerdp-shadow-subsystem-mutter)
      list(APPEND SHADOW_SUBSYSTEM_DEFS WITH_SHADOW_MUTTER)
    endif()
    if(WITH_SHADOW_X11)
      add_subdirectory(X11)
      list(APPEND SHADOW_SUBSYSTEM_LIBS freerdp-shadow-subsystem-impl)
    endif()
  endif()
else()
  add_subdirectory(Sample)
  list(APPEND SHADOW_SUBSYSTEM_LIBS freerdp-shadow-subsystem-impl)
endif()

addtargetwithresourcefile(${MODULE_NAME} FALSE "${FREERDP_VERSION}" SRCS)

if(TARGET freerdp-shadow-subsystem-impl)
  list(APPEND SHADOW_SUBSYSTEM_DEFS WITH_SHADOW_PLATFORM)
endif()
target_compile_definitions(${MODULE_NAME} PRIVATE ${SHADOW_SUBSYSTEM_DEFS})

list(APPEND LIBS ${SHADOW_SUBSYSTEM_LIBS} freerdp-shadow freerdp winpr)

target_include_directories(${MODULE_NAME} INTERFACE $<INSTALL_INTERFACE:include/freerdp${FREERDP_VERSION_MAJOR}>)
target_link_libraries(${MODULE_NAME} PRIVATE ${LIBS})

if(NOT BUILD_SHARED_LIBS AND SHADOW_SUBSYSTEM_LIBS)
  installwithrpath(TARGETS ${SHADOW_SUBSYSTEM_LIBS} DESTINATION ${CMAKE_INSTALL_LIBDIR} EXPORT FreeRDP-ShadowTargets)
endif()

installwithrpath(
  TARGETS
  ${MODULE_NAME}
  COMPONENT
  server
  EXPORT
  FreeRDP-ShadowTargets
  ARCHIVE
  DESTINATION
  ${CMAKE_INSTALL_LIBDIR}
  LIBRARY
  DESTINATION
  ${CMAKE_INSTALL_LIBDIR}
  RUNTIME
  DESTINATION
  ${CMAKE_INSTALL_BINDIR}
)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Server/shadow")

# command-line executable

option(WITH_SERVER_SHADOW_CLI "Build shadow server cli tool" ON)
if(WITH_SERVER_SHADOW_CLI)
  add_subdirectory(cli)
endif()

if(BUILD_TESTING)
  add_subdirectory(test)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Server/shadow")

# Do not set Requires.Private if not a static build
if(NOT BUILD_SHARED_LIBS)
  set(FREERDP_SHADOW_PC_REQUIRES_PRIVATE "freerdp${FREERDP_API_VERSION}")

  include(PkgConfigAddThreads)
  pkg_config_add_threads(FREERDP_SHADOW_PC_LIBRARY_PRIVATE)
endif()
set(FREERDP_SHADOW_PC_REQUIRES freerdp-server${FREERDP_API_VERSION})

include(pkg-config-install-prefix)
cleaning_configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/freerdp-shadow.pc.in
  ${CMAKE_CURRENT_BINARY_DIR}/freerdp-shadow${FREERDP_VERSION_MAJOR}.pc @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/freerdp-shadow${FREERDP_VERSION_MAJOR}.pc
        DESTINATION ${PKG_CONFIG_PC_INSTALL_DIR}
)

export(PACKAGE freerdp-shadow)

setfreerdpcmakeinstalldir(FREERDP_SERVER_CMAKE_INSTALL_DIR "FreeRDP-Shadow${FREERDP_VERSION_MAJOR}")

configure_package_config_file(
  FreeRDP-ShadowConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/FreeRDP-ShadowConfig.cmake
  INSTALL_DESTINATION ${FREERDP_SERVER_CMAKE_INSTALL_DIR} PATH_VARS FREERDP_INCLUDE_DIR
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/FreeRDP-ShadowConfigVersion.cmake VERSION ${FREERDP_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/FreeRDP-ShadowConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/FreeRDP-ShadowConfigVersion.cmake
        DESTINATION ${FREERDP_SERVER_CMAKE_INSTALL_DIR}
)

install(EXPORT FreeRDP-ShadowTargets DESTINATION ${FREERDP_SERVER_CMAKE_INSTALL_DIR})

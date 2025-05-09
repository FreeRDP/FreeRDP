# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP Servers
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

# Servers
option(WITH_SHADOW "Compile with shadow server" ON)
option(WITH_PROXY "Compile with proxy server" ON)
option(WITH_PLATFORM_SERVER "Compile with platform server" ON)

add_subdirectory(common)
if(WITH_SHADOW)
  if(APPLE)
    message(WARNING "Mac shadow server implementation no longer compiles")
  else()
    add_subdirectory(shadow)
  endif()
endif()
if(WITH_PROXY)
  add_subdirectory(proxy)
endif()

if(WITH_SAMPLE)
  add_subdirectory(Sample)
endif()

if(WITH_PLATFORM_SERVER)
  if(NOT WIN32)
    if(APPLE AND (NOT IOS))
      #      add_subdirectory(Mac)
      message(WARNING "Mac platform server implementation no longer compiles")
    endif()
  else()
    add_subdirectory(Windows)
  endif()

  if(NOT DEFINED WITH_FREERDS)
    set(WITH_FREERDS 1)
  endif()

  if(WITH_FREERDS AND (IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/FreeRDS"))
    add_subdirectory("FreeRDS")
  endif()
endif()

# Pick up other servers

set(FILENAME "ModuleOptions.cmake")
file(GLOB FILEPATHS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*/${FILENAME}")

foreach(FILEPATH ${FILEPATHS})
  if(${FILEPATH} MATCHES "^([^/]*)/+${FILENAME}")
    string(REGEX REPLACE "^([^/]*)/+${FILENAME}" "\\1" FREERDP_SERVER ${FILEPATH})
    set(FREERDP_SERVER_ENABLED 0)
    include(${FILEPATH})
    if(FREERDP_SERVER_ENABLED)
      if(NOT (${FREERDP_SERVER_VENDOR} MATCHES "FreeRDP"))
        list(APPEND FREERDP_EXTRA_SERVERS ${FREERDP_SERVER})
      endif()
    endif()
  endif()
endforeach()

foreach(FREERDP_SERVER ${FREERDP_EXTRA_SERVERS})
  add_subdirectory(${FREERDP_SERVER})
endforeach()

include(pkg-config-install-prefix)
cleaning_configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/freerdp-server.pc.in
  ${CMAKE_CURRENT_BINARY_DIR}/freerdp-server${FREERDP_VERSION_MAJOR}.pc @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/freerdp-server${FREERDP_VERSION_MAJOR}.pc
        DESTINATION ${PKG_CONFIG_PC_INSTALL_DIR}
)

export(PACKAGE freerdp-server)

setfreerdpcmakeinstalldir(FREERDP_SERVER_CMAKE_INSTALL_DIR "FreeRDP-Server${FREERDP_VERSION_MAJOR}")

configure_package_config_file(
  FreeRDP-ServerConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/FreeRDP-ServerConfig.cmake
  INSTALL_DESTINATION ${FREERDP_SERVER_CMAKE_INSTALL_DIR} PATH_VARS FREERDP_INCLUDE_DIR
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/FreeRDP-ServerConfigVersion.cmake VERSION ${FREERDP_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/FreeRDP-ServerConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/FreeRDP-ServerConfigVersion.cmake
        DESTINATION ${FREERDP_SERVER_CMAKE_INSTALL_DIR}
)

install(EXPORT FreeRDP-ServerTargets DESTINATION ${FREERDP_SERVER_CMAKE_INSTALL_DIR})

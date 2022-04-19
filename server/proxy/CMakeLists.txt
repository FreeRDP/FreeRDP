# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP Proxy Server
#
# Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
# Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
# Copyright 2019 Idan Freiberg <speidy@gmail.com>
# Copyright 2021 Armin Novak <anovak@thincast.com>
# Copyright 2021 Thincast Technologies GmbH
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

include(CMakeDependentOption)
set(MODULE_NAME "freerdp-server-proxy")
set(MODULE_PREFIX "FREERDP_SERVER_PROXY")

set(${MODULE_PREFIX}_SRCS
  pf_context.c
  pf_channel.c
  pf_channel.h
  pf_client.c
  pf_client.h
  pf_input.c
  pf_input.h
  pf_update.c
  pf_update.h
  pf_server.c
  pf_server.h
  pf_config.c
  pf_modules.c
  pf_utils.h
  pf_utils.c
  )

set(PROXY_APP_SRCS freerdp_proxy.c)

option(WITH_PROXY_EMULATE_SMARTCARD "Compile proxy smartcard emulation" OFF)
add_subdirectory("channels")

# On windows create dll version information.
# Vendor, product and year are already set in top level CMakeLists.txt
if (WIN32)
  set (RC_VERSION_MAJOR ${FREERDP_VERSION_MAJOR})
  set (RC_VERSION_MINOR ${FREERDP_VERSION_MINOR})
  set (RC_VERSION_BUILD ${FREERDP_VERSION_REVISION})
  set (RC_VERSION_FILE "${MODULE_NAME}${CMAKE_EXECUTABLE_SUFFIX}" )

  configure_file(
    ${PROJECT_SOURCE_DIR}/cmake/WindowsDLLVersion.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.rc
    @ONLY)

  set ( ${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
  list(APPEND PROXY_APP_SRCS ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

add_library(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS} $<TARGET_OBJECTS:pf_channels>)
set_target_properties(${MODULE_NAME} PROPERTIES OUTPUT_NAME ${MODULE_NAME}${FREERDP_API_VERSION})

if (WITH_LIBRARY_VERSIONING)
  set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${FREERDP_VERSION} SOVERSION ${FREERDP_API_VERSION})
endif()

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} freerdp-server)
set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} freerdp-client)
set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} winpr freerdp)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})
install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT server EXPORT FreeRDP-ProxyTargets)
if (WITH_DEBUG_SYMBOLS AND MSVC)
    install(FILES ${CMAKE_PDB_BINARY_DIR}/${MODULE_NAME}${FREERDP_VERSION_MAJOR}.pdb DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT symbols)
endif()
# pkg-config
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/freerdp-proxy.pc.in ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}${FREERDP_VERSION_MAJOR}.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}${FREERDP_VERSION_MAJOR}.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

export(PACKAGE freerdp-proxy)

SetFreeRDPCMakeInstallDir(FREERDP_PROXY_CMAKE_INSTALL_DIR "FreeRDP-Proxy${FREERDP_VERSION_MAJOR}")

configure_package_config_file(FreeRDP-ProxyConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/FreeRDP-ProxyConfig.cmake
	INSTALL_DESTINATION ${FREERDP_PROXY_CMAKE_INSTALL_DIR}
	PATH_VARS FREERDP_INCLUDE_DIR)

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/FreeRDP-ProxyConfigVersion.cmake
	VERSION ${FREERDP_VERSION} COMPATIBILITY SameMajorVersion)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/FreeRDP-ProxyConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/FreeRDP-ProxyConfigVersion.cmake
	DESTINATION ${FREERDP_PROXY_CMAKE_INSTALL_DIR})
install(EXPORT FreeRDP-ProxyTargets DESTINATION ${FREERDP_PROXY_CMAKE_INSTALL_DIR})

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Server/proxy")

option(WITH_PROXY_APP "Compile proxy application" ON)

if (WITH_PROXY_APP)
  add_subdirectory("cli")
endif()

option(WITH_PROXY_MODULES "Compile proxy modules" ON)
if (WITH_PROXY_MODULES)
  add_subdirectory("modules")
endif()


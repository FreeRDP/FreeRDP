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
    $<TARGET_OBJECTS:pf_channels>
)

set(PROXY_APP_SRCS freerdp_proxy.c)

option(WITH_PROXY_EMULATE_SMARTCARD "Compile proxy smartcard emulation" OFF)
add_subdirectory("channels")

addtargetwithresourcefile(${MODULE_NAME} FALSE "${FREERDP_VERSION}" ${MODULE_PREFIX}_SRCS)

set(PRIVATE_LIBS freerdp-client freerdp-server)

set(PUBLIC_LIBS winpr freerdp)

target_include_directories(${MODULE_NAME} INTERFACE $<INSTALL_INTERFACE:include>)
target_link_libraries(${MODULE_NAME} PRIVATE ${PRIVATE_LIBS} PUBLIC ${PUBLIC_LIBS})

install(TARGETS ${MODULE_NAME} COMPONENT server EXPORT FreeRDP-ProxyTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Server/Proxy")

# pkg-config
include(pkg-config-install-prefix)
cleaning_configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/freerdp-proxy.pc.in ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}${FREERDP_VERSION_MAJOR}.pc
  @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}${FREERDP_VERSION_MAJOR}.pc
        DESTINATION ${PKG_CONFIG_PC_INSTALL_DIR}
)

export(PACKAGE freerdp-proxy)

setfreerdpcmakeinstalldir(FREERDP_PROXY_CMAKE_INSTALL_DIR "FreeRDP-Proxy${FREERDP_VERSION_MAJOR}")

configure_package_config_file(
  FreeRDP-ProxyConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/FreeRDP-ProxyConfig.cmake
  INSTALL_DESTINATION ${FREERDP_PROXY_CMAKE_INSTALL_DIR} PATH_VARS FREERDP_INCLUDE_DIR
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/FreeRDP-ProxyConfigVersion.cmake VERSION ${FREERDP_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/FreeRDP-ProxyConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/FreeRDP-ProxyConfigVersion.cmake
        DESTINATION ${FREERDP_PROXY_CMAKE_INSTALL_DIR}
)
install(EXPORT FreeRDP-ProxyTargets DESTINATION ${FREERDP_PROXY_CMAKE_INSTALL_DIR})

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Server/proxy")

option(WITH_PROXY_APP "Compile proxy application" ON)

if(WITH_PROXY_APP)
  add_subdirectory("cli")
endif()

option(WITH_PROXY_MODULES "Compile proxy modules" ON)
if(WITH_PROXY_MODULES)
  add_subdirectory("modules")
endif()

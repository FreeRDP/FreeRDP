# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP Proxy Server
#
# Copyright 2021 Armin Novak <armin.novak@thincast.com>
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

set(PROXY_APP_SRCS freerdp_proxy.c)

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

  list(APPEND PROXY_APP_SRCS ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

set(APP_NAME "freerdp-proxy")
add_executable(${APP_NAME}
    ${PROXY_APP_SRCS})
target_link_libraries(${APP_NAME} ${MODULE_NAME})
install(TARGETS ${APP_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT server)
if (WITH_DEBUG_SYMBOLS AND MSVC)
    install(FILES ${CMAKE_PDB_BINARY_DIR}/${APP_NAME}.pdb DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT symbols)
endif()

set_property(TARGET ${APP_NAME} PROPERTY FOLDER "Server/proxy")

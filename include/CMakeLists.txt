# FreeRDP: A Remote Desktop Protocol Implementation
# include headers cmake build script
#
# Copyright 2011 O.S. Systems Software Ltda.
# Copyright 2011 Otavio Salvador <otavio@ossystems.com.br>
# Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/freerdp/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/freerdp/version.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/freerdp/build-config.h.in ${CMAKE_CURRENT_BINARY_DIR}/freerdp/build-config.h)
set(FREERDP_INSTALL_INCLUDE_DIR include/freerdp${FREERDP_VERSION_MAJOR}/freerdp)

file(GLOB FREERDP_HEADERS "freerdp/*.h")
install(FILES ${FREERDP_HEADERS} DESTINATION ${FREERDP_INSTALL_INCLUDE_DIR} COMPONENT headers)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/freerdp/version.h DESTINATION ${FREERDP_INSTALL_INCLUDE_DIR} COMPONENT headers)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/freerdp/build-config.h DESTINATION ${FREERDP_INSTALL_INCLUDE_DIR} COMPONENT headers)

install(DIRECTORY freerdp/cache DESTINATION ${FREERDP_INSTALL_INCLUDE_DIR} COMPONENT headers FILES_MATCHING PATTERN "*.h")
install(DIRECTORY freerdp/codec DESTINATION ${FREERDP_INSTALL_INCLUDE_DIR} COMPONENT headers FILES_MATCHING PATTERN "*.h")
install(DIRECTORY freerdp/crypto DESTINATION ${FREERDP_INSTALL_INCLUDE_DIR} COMPONENT headers FILES_MATCHING PATTERN "*.h")
install(DIRECTORY freerdp/gdi DESTINATION ${FREERDP_INSTALL_INCLUDE_DIR} COMPONENT headers FILES_MATCHING PATTERN "*.h")
install(DIRECTORY freerdp/locale DESTINATION ${FREERDP_INSTALL_INCLUDE_DIR} COMPONENT headers FILES_MATCHING PATTERN "*.h")
install(DIRECTORY freerdp/utils DESTINATION ${FREERDP_INSTALL_INCLUDE_DIR} COMPONENT headers FILES_MATCHING PATTERN "*.h")
install(DIRECTORY freerdp/client DESTINATION ${FREERDP_INSTALL_INCLUDE_DIR} COMPONENT headers FILES_MATCHING PATTERN "*.h")
install(DIRECTORY freerdp/server DESTINATION ${FREERDP_INSTALL_INCLUDE_DIR} COMPONENT headers FILES_MATCHING PATTERN "*.h")
install(DIRECTORY freerdp/channels DESTINATION ${FREERDP_INSTALL_INCLUDE_DIR} COMPONENT headers FILES_MATCHING PATTERN "*.h")

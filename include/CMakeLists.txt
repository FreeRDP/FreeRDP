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

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/freerdp/version.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/build-config.h.in ${CMAKE_CURRENT_BINARY_DIR}/freerdp/build-config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/freerdp/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/buildflags.h.in ${CMAKE_CURRENT_BINARY_DIR}/freerdp/buildflags.h)

file(GLOB_RECURSE PUBLIC_COMMON_HEADERS
	LIST_DIRECTORIES false
	"freerdp/*.h"
)
file(GLOB_RECURSE PUBLIC_COMMON_BIN_HEADERS
	LIST_DIRECTORIES false
    "${CMAKE_CURRENT_BINARY_DIR}/freerdp/*.h"
)
list(SORT PUBLIC_COMMON_HEADERS)

if (WITH_SERVER)
    set(PUBLIC_SERVER_HEADERS ${PUBLIC_COMMON_HEADERS})
    list(FILTER PUBLIC_SERVER_HEADERS INCLUDE REGEX ".*freerdp/server.*")

    set(PUBLIC_PROXY_HEADERS ${PUBLIC_SERVER_HEADERS})
    list(FILTER PUBLIC_SERVER_HEADERS EXCLUDE REGEX ".*freerdp/server/proxy.*")
    list(FILTER PUBLIC_PROXY_HEADERS INCLUDE REGEX ".*freerdp/server/proxy.*")
    if (WITH_SERVER)
        set_property(TARGET freerdp-server APPEND PROPERTY SOURCES
            ${PUBLIC_SERVER_HEADERS}
        )
    endif()
    if (WITH_PROXY)
        set_property(TARGET freerdp-server-proxy APPEND PROPERTY SOURCES
            ${PUBLIC_PROXY_HEADERS}
        )
    endif()
endif()

if (WITH_CLIENT_COMMON)
    set(PUBLIC_CLIENT_HEADERS ${PUBLIC_COMMON_HEADERS})
    list(FILTER PUBLIC_CLIENT_HEADERS INCLUDE REGEX ".*freerdp/client.*")
    set_property( TARGET freerdp-client APPEND PROPERTY SOURCES
        ${PUBLIC_CLIENT_HEADERS}
    )
endif()

if (WITH_SERVER)
    list(FILTER PUBLIC_COMMON_HEADERS EXCLUDE REGEX ".*freerdp/server.*")
endif()

list(FILTER PUBLIC_COMMON_HEADERS EXCLUDE REGEX ".*freerdp/client.*")
list(APPEND PUBLIC_COMMON_HEADERS ${PUBLIC_COMMON_BIN_HEADERS})
set_property(TARGET freerdp APPEND PROPERTY SOURCES
    ${PUBLIC_COMMON_HEADERS}
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/freerdp
        DESTINATION ${FREERDP_INCLUDE_DIR}
        FILES_MATCHING PATTERN "*.h")

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/freerdp
        DESTINATION ${FREERDP_INCLUDE_DIR}
        FILES_MATCHING PATTERN "*.h")

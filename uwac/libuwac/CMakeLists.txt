# UWAC: Using Wayland As Client
#
# Copyright 2015 David FORT <contact@hardening-consulting.com>
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

set(MODULE_NAME "uwac")
set(MODULE_PREFIX "UWAC")


set(GENERATED_SOURCES "")
macro(generate_protocol_file PROTO)
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PROTO}-protocol.c"
        COMMAND ${WAYLAND_SCANNER} code < ${CMAKE_SOURCE_DIR}/uwac/protocols/${PROTO}.xml > ${CMAKE_CURRENT_BINARY_DIR}/${PROTO}-protocol.c
        DEPENDS ${CMAKE_SOURCE_DIR}/uwac/protocols/${PROTO}.xml
    )

    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PROTO}-client-protocol.h"
        COMMAND ${WAYLAND_SCANNER} client-header < ${CMAKE_SOURCE_DIR}/uwac/protocols/${PROTO}.xml > ${CMAKE_CURRENT_BINARY_DIR}/${PROTO}-client-protocol.h
        DEPENDS ${CMAKE_SOURCE_DIR}/uwac/protocols/${PROTO}.xml
    )
    
    list(APPEND GENERATED_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/${PROTO}-client-protocol.h)
    list(APPEND GENERATED_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/${PROTO}-protocol.c)
endmacro()

generate_protocol_file(xdg-shell)
generate_protocol_file(ivi-application)
generate_protocol_file(fullscreen-shell)

include_directories(${WAYLAND_INCLUDE_DIR})
include_directories(${XKBCOMMON_INCLUDE_DIR})
include_directories("${CMAKE_SOURCE_DIR}/uwac/include")
include_directories("${CMAKE_BINARY_DIR}/uwac/include")
include_directories("${CMAKE_BINARY_DIR}/uwac/libuwac")

add_definitions(-DBUILD_IVI -DBUILD_FULLSCREEN_SHELL -DENABLE_XKBCOMMON)

set(${MODULE_PREFIX}_SRCS
    ${GENERATED_SOURCES}
    uwac-display.c
    uwac-input.c
    uwac-os.c
    uwac-os.h
    uwac-output.c
    uwac-priv.h
    uwac-tools.c
    uwac-utils.c
	uwac-window.c)


add_library(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})


if (WITH_LIBRARY_VERSIONING)
	set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${UWAC_VERSION} SOVERSION ${UWAC_API_VERSION})
endif()

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS} ${PRIVATE_KEYWORD} ${WAYLAND_LIBS} ${XKBCOMMON_LIBS} freerdp)

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} EXPORT uwac)


set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "uwac")

if(BUILD_TESTING)
#	add_subdirectory(test)
endif()

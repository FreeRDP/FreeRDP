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
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/protocols/${PROTO}-protocol.c"
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/protocols
	COMMAND ${WAYLAND_SCANNER} code < ${CMAKE_CURRENT_SOURCE_DIR}/../protocols/${PROTO}.xml > ${CMAKE_CURRENT_BINARY_DIR}/protocols/${PROTO}-protocol.c
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../protocols/${PROTO}.xml
    )

    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/protocols/${PROTO}-client-protocol.h"
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/protocols
	COMMAND ${WAYLAND_SCANNER} client-header < ${CMAKE_CURRENT_SOURCE_DIR}/../protocols/${PROTO}.xml > ${CMAKE_CURRENT_BINARY_DIR}/protocols/${PROTO}-client-protocol.h
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../protocols/${PROTO}.xml
    )

    list(APPEND GENERATED_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/protocols/${PROTO}-client-protocol.h)
    list(APPEND GENERATED_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/protocols/${PROTO}-protocol.c)
endmacro()

generate_protocol_file(xdg-shell)
generate_protocol_file(xdg-decoration-unstable-v1)
generate_protocol_file(server-decoration)
generate_protocol_file(ivi-application)
generate_protocol_file(fullscreen-shell-unstable-v1)
generate_protocol_file(keyboard-shortcuts-inhibit-unstable-v1)

if(FREEBSD)
	include_directories(${EPOLLSHIM_INCLUDE_DIR})
endif()
include_directories(${WAYLAND_INCLUDE_DIR})
include_directories(${XKBCOMMON_INCLUDE_DIR})
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../include")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/../include")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/protocols")

add_definitions(-DBUILD_IVI -DBUILD_FULLSCREEN_SHELL -DENABLE_XKBCOMMON)

set(${MODULE_PREFIX}_SRCS
    ${GENERATED_SOURCES}
    uwac-display.c
    uwac-input.c
    uwac-clipboard.c
    uwac-os.c
    uwac-os.h
    uwac-output.c
    uwac-priv.h
    uwac-tools.c
    uwac-utils.c
    uwac-window.c)


add_library(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

set_target_properties(${MODULE_NAME} PROPERTIES LINKER_LANGUAGE C)
set_target_properties(${MODULE_NAME} PROPERTIES OUTPUT_NAME ${MODULE_NAME}${UWAC_API_VERSION})
if (WITH_LIBRARY_VERSIONING)
	set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${UWAC_VERSION} SOVERSION ${UWAC_API_VERSION})
endif()

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS} PRIVATE ${WAYLAND_LIBS} ${XKBCOMMON_LIBS} ${EPOLLSHIM_LIBS})
if (HAVE_PIXMAN_REGION)
	target_link_libraries(${MODULE_NAME} PRIVATE ${pixman_LINK_LIBRARIES})
else()
	target_link_libraries(${MODULE_NAME} PRIVATE freerdp)
endif()

install(TARGETS ${MODULE_NAME} COMPONENT libraries EXPORT uwac
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "uwac")

if(BUILD_TESTING)
#	add_subdirectory(test)
endif()

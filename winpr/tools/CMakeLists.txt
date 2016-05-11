# WinPR: Windows Portable Runtime
# winpr cmake build script
#
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
# Copyright 2016  Thincast Technologies GmbH
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

# Soname versioning - use winpr version
set(WINPR_TOOLS_VERSION_MAJOR "${WINPR_VERSION_MAJOR}")
set(WINPR_TOOLS_VERSION_MINOR "${WINPR_VERSION_MINOR}")
set(WINPR_TOOLS_VERSION_REVISION "${WINPR_VERSION_REVISION}")

set(WINPR_TOOLS_API_VERSION "${WINPR_TOOLS_VERSION_MAJOR}")
set(WINPR_TOOLS_VERSION "${WINPR_TOOLS_VERSION_MAJOR}.${WINPR_TOOLS_VERSION_MINOR}.${WINPR_TOOLS_VERSION_REVISION}")
set(WINPR_TOOLS_VERSION_FULL "${WINPR_TOOLS_VERSION}")
set(WINPR_TOOLS_API_VERSION "${WINPR_TOOLS_VERSION_MAJOR}")

set(WINPR_TOOLS_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(WINPR_TOOLS_SRCS "")
set(WINPR_TOOLS_LIBS "")
set(WINPR_TOOLS_INCLUDES "")
set(WINPR_TOOLS_DEFINITIONS "")

macro (winpr_tools_module_add)
    file (RELATIVE_PATH _relPath "${WINPR_TOOLS_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
    foreach (_src ${ARGN})
        if (_relPath)
            list (APPEND WINPR_TOOLS_SRCS "${_relPath}/${_src}")
        else()
            list (APPEND WINPR_TOOLS_SRCS "${_src}")
        endif()
    endforeach()
    if (_relPath)
        set (WINPR_TOOLS_SRCS ${WINPR_TOOLS_SRCS} PARENT_SCOPE)
    endif()
endmacro()

macro (winpr_tools_include_directory_add)
    file (RELATIVE_PATH _relPath "${WINPR_TOOLS_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
    foreach (_inc ${ARGN})
                if (IS_ABSOLUTE ${_inc})
                            list (APPEND WINPR_TOOLS_INCLUDES "${_inc}")
                else()
                    if (_relPath)
                            list (APPEND WINPR_TOOLS_INCLUDES "${_relPath}/${_inc}")
                    else()
                            list (APPEND WINPR_TOOLS_INCLUDES "${_inc}")
                    endif()
                endif()
    endforeach()
    if (_relPath)
        set (WINPR_TOOLS_INCLUDES ${WINPR_TOOLS_INCLUDES} PARENT_SCOPE)
    endif()
endmacro()

macro (winpr_tools_library_add)
    foreach (_lib ${ARGN})
            list (APPEND WINPR_TOOLS_LIBS "${_lib}")
    endforeach()
     set (WINPR_TOOLS_LIBS ${WINPR_TOOLS_LIBS} PARENT_SCOPE)
endmacro()

macro (winpr_tools_definition_add)
    foreach (_define ${ARGN})
            list (APPEND WINPR_TOOLS_DEFINITONS "${_define}")
    endforeach()
     set (WINPR_TOOLS_DEFINITONS ${WINPR_TOOLS_DEFINITONS} PARENT_SCOPE)
endmacro()

add_subdirectory(makecert)

set(MODULE_NAME winpr-tools)
list(REMOVE_DUPLICATES WINPR_TOOLS_DEFINITIONS)
list(REMOVE_DUPLICATES WINPR_TOOLS_LIBS)
list(REMOVE_DUPLICATES WINPR_TOOLS_INCLUDES)
include_directories(${WINPR_TOOLS_INCLUDES})

# On windows create dll version information.
# Vendor, product and year are already set in top level CMakeLists.txt
if (WIN32)
  set (RC_VERSION_MAJOR ${WINPR_VERSION_MAJOR})
  set (RC_VERSION_MINOR ${WINPR_VERSION_MINOR})
  set (RC_VERSION_BUILD ${WINPR_VERSION_REVISION})
  set (RC_VERSION_FILE "${CMAKE_SHARED_LIBRARY_PREFIX}${MODULE_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}" )

  configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/WindowsDLLVersion.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.rc
    @ONLY)

  set (WINPR_TOOLS_SRCS ${WINPR_TOOLS_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

add_library(${MODULE_NAME} ${WINPR_TOOLS_SRCS})
set_target_properties(${MODULE_NAME} PROPERTIES LINKER_LANGUAGE C)
if (WITH_LIBRARY_VERSIONING)
    set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${WINPR_TOOLS_VERSION} SOVERSION ${WINPR_TOOLS_API_VERSION})
endif()

add_definitions(${WINPR_DEFINITIONS})
target_link_libraries(${MODULE_NAME} ${PRIVATE_KEYWORD} ${WINPR_TOOLS_LIBS})

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries EXPORT WinPRTargets)
if (WITH_DEBUG_SYMBOLS AND MSVC AND BUILD_SHARED_LIBS)
    install(FILES ${CMAKE_PDB_BINARY_DIR}/${MODULE_NAME}.pdb DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT symbols)
endif()
set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/Tools")

# Add all command line utilities
add_subdirectory(makecert-cli)
add_subdirectory(hash-cli)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/winpr-tools.pc.in ${CMAKE_CURRENT_BINARY_DIR}/winpr-tools${WINPR_TOOLS_VERSION_MAJOR}.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/winpr-tools${WINPR_TOOLS_VERSION_MAJOR}.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

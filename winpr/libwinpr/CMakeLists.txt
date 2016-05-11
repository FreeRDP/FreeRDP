# WinPR: Windows Portable Runtime
# winpr cmake build script
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

if (APPLE)
	# flat_namespace should be avoided, but is required for -undefined warning. Since WinPR currently has
	# a lot of undefined symbols in use, use this hack until they're filled out.
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-flat_namespace,-undefined,warning")
endif()

set(WINPR_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(WINPR_SRCS "")
set(WINPR_LIBS "")
set(WINPR_INCLUDES "")
set(WINPR_DEFINITIONS "")

macro (winpr_module_add)
    file (RELATIVE_PATH _relPath "${WINPR_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
    foreach (_src ${ARGN})
        if (_relPath)
            list (APPEND WINPR_SRCS "${_relPath}/${_src}")
        else()
            list (APPEND WINPR_SRCS "${_src}")
        endif()
    endforeach()
    if (_relPath)
        set (WINPR_SRCS ${WINPR_SRCS} PARENT_SCOPE)
    endif()
endmacro()

macro (winpr_include_directory_add)
    file (RELATIVE_PATH _relPath "${WINPR_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
    foreach (_inc ${ARGN})
                if (IS_ABSOLUTE ${_inc})
                            list (APPEND WINPR_INCLUDES "${_inc}")
                else()
                    if (_relPath)
                            list (APPEND WINPR_INCLUDES "${_relPath}/${_inc}")
                    else()
                            list (APPEND WINPR_INCLUDES "${_inc}")
                    endif()
                endif()
    endforeach()
    if (_relPath)
        set (WINPR_INCLUDES ${WINPR_INCLUDES} PARENT_SCOPE)
    endif()
endmacro()

macro (winpr_library_add)
    foreach (_lib ${ARGN})
            list (APPEND WINPR_LIBS "${_lib}")
    endforeach()
     set (WINPR_LIBS ${WINPR_LIBS} PARENT_SCOPE)
endmacro()

macro (winpr_definition_add)
    foreach (_define ${ARGN})
            list (APPEND WINPR_DEFINITIONS "${_define}")
    endforeach()
     set (WINPR_DEFINITIONS ${WINPR_DEFINITIONS} PARENT_SCOPE)
endmacro()

if (ANDROID)
    winpr_library_add(log)
endif()

# Level "1" API as defined for MinCore.lib
set(WINPR_CORE synch locale library file comm pipe interlocked security
	environment crypto registry credentials path io memory input shell
	heap utils error com timezone sysinfo pool handle thread)

foreach(DIR ${WINPR_CORE})
	add_subdirectory(${DIR})
endforeach()

set(WINPR_LEVEL2 winsock sspi winhttp asn1 sspicli crt bcrypt rpc credui
	wtsapi dsparse wnd smartcard nt clipboard)

foreach(DIR ${WINPR_LEVEL2})
	add_subdirectory(${DIR})
endforeach()

set(MODULE_NAME winpr)
list(REMOVE_DUPLICATES WINPR_DEFINITIONS)
list(REMOVE_DUPLICATES WINPR_LIBS)
list(REMOVE_DUPLICATES WINPR_INCLUDES)
include_directories(${WINPR_INCLUDES})

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

  set (WINPR_SRCS ${WINPR_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
  list(APPEND WINPR_LIBS "Shlwapi")
endif()

add_library(${MODULE_NAME} ${WINPR_SRCS})
set_target_properties(${MODULE_NAME} PROPERTIES LINKER_LANGUAGE C)
if (WITH_LIBRARY_VERSIONING)
    set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${WINPR_VERSION} SOVERSION ${WINPR_API_VERSION})
endif()

add_definitions(${WINPR_DEFINITIONS})
if (WIN32)
target_link_libraries(${MODULE_NAME} ${PUBLIC_KEYWORD} ${WINPR_LIBS})
else()
target_link_libraries(${MODULE_NAME} ${PRIVATE_KEYWORD} ${WINPR_LIBS})
endif(WIN32)
install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries EXPORT WinPRTargets)
if (WITH_DEBUG_SYMBOLS AND MSVC AND BUILD_SHARED_LIBS)
    install(FILES ${CMAKE_PDB_BINARY_DIR}/${MODULE_NAME}.pdb DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT symbols)
endif()
set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/libwinpr")

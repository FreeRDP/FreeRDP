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

include(CheckFunctionExists)

set(WINPR_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(WINPR_SRCS "")
set(WINPR_LIBS_PRIVATE "")
set(WINPR_LIBS_PUBLIC "")
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

macro (winpr_library_add_private)
    foreach (_lib ${ARGN})
            list (APPEND WINPR_LIBS_PRIVATE "${_lib}")
    endforeach()
     set (WINPR_LIBS_PRIVATE ${WINPR_LIBS_PRIVATE} PARENT_SCOPE)
endmacro()

macro (winpr_library_add_public)
    foreach (_lib ${ARGN})
            list (APPEND WINPR_LIBS_PUBLIC "${_lib}")
    endforeach()
     set (WINPR_LIBS_PUBLIC ${WINPR_LIBS_PUBLIC} PARENT_SCOPE)
endmacro()

macro (winpr_definition_add)
    foreach (_define ${ARGN})
            list (APPEND WINPR_DEFINITIONS "${_define}")
    endforeach()
     set (WINPR_DEFINITIONS ${WINPR_DEFINITIONS} PARENT_SCOPE)
endmacro()

set(CMAKE_REQUIRED_LIBRARIES rt)

if(NOT IOS)
    check_function_exists(timer_create TIMER_CREATE)
    check_function_exists(timer_delete TIMER_DELETE)
    check_function_exists(timer_settime TIMER_SETTIME)
    check_function_exists(timer_gettime TIMER_GETTIME)
    if (TIMER_CREATE AND TIMER_DELETE AND TIMER_SETTIME AND TIMER_GETTIME)
        add_definitions(-DWITH_POSIX_TIMER)
        winpr_library_add_private(rt)
    endif()
endif()

if (ANDROID)
    winpr_library_add_private(log)
endif()

# Level "1" API as defined for MinCore.lib
set(WINPR_CORE synch library file comm pipe interlocked security
	environment crypto registry path io memory ncrypt input shell
	utils error timezone sysinfo pool handle thread)

foreach(DIR ${WINPR_CORE})
	add_subdirectory(${DIR})
	source_group("${DIR}" REGULAR_EXPRESSION "${DIR}/.*\\.[ch]")
endforeach()

set(WINPR_LEVEL2 winsock sspi sspicli crt bcrypt rpc
	wtsapi dsparse smartcard nt clipboard)

foreach(DIR ${WINPR_LEVEL2})
	add_subdirectory(${DIR})
	source_group("${DIR}" REGULAR_EXPRESSION "${DIR}/.*\\.[ch]")
endforeach()

set(MODULE_NAME winpr)
list(REMOVE_DUPLICATES WINPR_DEFINITIONS)
list(REMOVE_DUPLICATES WINPR_LIBS_PRIVATE)
list(REMOVE_DUPLICATES WINPR_LIBS_PUBLIC)
list(REMOVE_DUPLICATES WINPR_INCLUDES)
include_directories(${WINPR_INCLUDES})

# On windows create dll version information.
# Vendor, product and year are already set in top level CMakeLists.txt
if (WIN32)
  set (RC_VERSION_MAJOR ${WINPR_VERSION_MAJOR})
  set (RC_VERSION_MINOR ${WINPR_VERSION_MINOR})
  set (RC_VERSION_BUILD ${WINPR_VERSION_REVISION})
  set (RC_VERSION_FILE "${CMAKE_SHARED_LIBRARY_PREFIX}${MODULE_NAME}${WINPR_API_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}" )

  configure_file(
    ${PROJECT_SOURCE_DIR}/cmake/WindowsDLLVersion.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.rc
    @ONLY)

  set (WINPR_SRCS ${WINPR_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
  winpr_library_add_public("shlwapi")
endif()

add_library(${MODULE_NAME} ${WINPR_SRCS})
if (APPLE)
    set_target_properties(${MODULE_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION FALSE)
endif()
set_target_properties(${MODULE_NAME} PROPERTIES LINKER_LANGUAGE C)
set_target_properties(${MODULE_NAME} PROPERTIES OUTPUT_NAME ${MODULE_NAME}${WINPR_API_VERSION})
if (WITH_LIBRARY_VERSIONING)
    set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${WINPR_VERSION} SOVERSION ${WINPR_API_VERSION})
endif()

add_definitions(${WINPR_DEFINITIONS})
target_link_libraries(${MODULE_NAME} PRIVATE ${WINPR_LIBS_PRIVATE} PUBLIC ${WINPR_LIBS_PUBLIC})
install(TARGETS ${MODULE_NAME} COMPONENT libraries EXPORT WinPRTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if (WITH_DEBUG_SYMBOLS AND MSVC AND BUILD_SHARED_LIBS)
    get_target_property(OUTPUT_FILENAME ${MODULE_NAME} OUTPUT_NAME)
    install(FILES ${CMAKE_PDB_BINARY_DIR}/${OUTPUT_FILENAME}.pdb DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT symbols)
endif()
set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/libwinpr")

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
include(JsonDetect)

set(WINPR_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(WINPR_SRCS "")
set(WINPR_LIBS_PRIVATE "")
set(WINPR_LIBS_PUBLIC "")
set(WINPR_INCLUDES "")
set(WINPR_DEFINITIONS "")
set(WINPR_COMPILE_OPTIONS "")
set(WINPR_LINK_OPTIONS "")
set(WINPR_LINK_DIRS "")

macro(winpr_module_add)
  file(RELATIVE_PATH _relPath "${WINPR_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
  foreach(_src ${ARGN})
    if(_relPath)
      list(APPEND WINPR_SRCS "${_relPath}/${_src}")
    else()
      list(APPEND WINPR_SRCS "${_src}")
    endif()
  endforeach()
  if(_relPath)
    set(WINPR_SRCS ${WINPR_SRCS} PARENT_SCOPE)
  endif()
endmacro()

macro(winpr_include_directory_add)
  file(RELATIVE_PATH _relPath "${WINPR_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
  foreach(_inc ${ARGN})
    if(IS_ABSOLUTE ${_inc})
      list(APPEND WINPR_INCLUDES "${_inc}")
    else()
      if(_relPath)
        list(APPEND WINPR_INCLUDES "${_relPath}/${_inc}")
      else()
        list(APPEND WINPR_INCLUDES "${_inc}")
      endif()
    endif()
  endforeach()
  if(_relPath)
    set(WINPR_INCLUDES ${WINPR_INCLUDES} PARENT_SCOPE)
  endif()
endmacro()

macro(winpr_library_add_private)
  foreach(_lib ${ARGN})
    list(APPEND WINPR_LIBS_PRIVATE "${_lib}")
  endforeach()
  set(WINPR_LIBS_PRIVATE ${WINPR_LIBS_PRIVATE} PARENT_SCOPE)
endmacro()

macro(winpr_library_add_public)
  foreach(_lib ${ARGN})
    list(APPEND WINPR_LIBS_PUBLIC "${_lib}")
  endforeach()
  set(WINPR_LIBS_PUBLIC ${WINPR_LIBS_PUBLIC} PARENT_SCOPE)
endmacro()

macro(winpr_definition_add)
  foreach(_define ${ARGN})
    list(APPEND WINPR_DEFINITIONS "${_define}")
  endforeach()
  set(WINPR_DEFINITIONS ${WINPR_DEFINITIONS} PARENT_SCOPE)
endmacro()

macro(winpr_library_add_compile_options)
  foreach(_define ${ARGN})
    list(APPEND WINPR_COMPILE_OPTIONS "${_define}")
  endforeach()
  set(WINPR_COMPILE_OPTIONS ${WINPR_COMPILE_OPTIONS} PARENT_SCOPE)
endmacro()

macro(winpr_library_add_link_options)
  foreach(_define ${ARGN})
    list(APPEND WINPR_LINK_OPTIONS "${_define}")
  endforeach()
  set(WINPR_LINK_OPTIONS ${WINPR_LINK_OPTIONS} PARENT_SCOPE)
endmacro()

macro(winpr_library_add_link_directory)
  foreach(_define ${ARGN})
    list(APPEND WINPR_LINK_DIRS "${_define}")
  endforeach()
  set(WINPR_LINK_DIRS ${WINPR_LINK_DIRS} PARENT_SCOPE)
endmacro()

set(CMAKE_REQUIRED_LIBRARIES rt)

find_package(uriparser)
option(WITH_URIPARSER "use uriparser library to handle URIs" ${uriparser_FOUND})
if(WITH_URIPARSER)
  find_package(uriparser CONFIG COMPONENTS char)
  if(uriparser_FOUND)
    winpr_library_add_private(uriparser::uriparser)
  else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(uriparser REQUIRED liburiparser)
    winpr_include_directory_add(${uriparser_INCLUDEDIR})
    winpr_include_directory_add(${uriparser_INCLUDE_DIRS})
    winpr_library_add_private(${uriparser_LIBRARIES})
  endif()
  add_compile_definitions("WITH_URIPARSER")
endif()

if(NOT IOS)
  check_function_exists(timer_create TIMER_CREATE)
  check_function_exists(timer_delete TIMER_DELETE)
  check_function_exists(timer_settime TIMER_SETTIME)
  check_function_exists(timer_gettime TIMER_GETTIME)
  if(TIMER_CREATE AND TIMER_DELETE AND TIMER_SETTIME AND TIMER_GETTIME)
    add_compile_definitions(WITH_POSIX_TIMER)
    winpr_library_add_private(rt)
  endif()
endif()

check_function_exists(pthread_setschedprio PTHREAD_SETSCHEDPRIO)
if(PTHREAD_SETSCHEDPRIO)
  winpr_definition_add(PTHREAD_SETSCHEDPRIO)
endif()

if(ANDROID)
  winpr_library_add_private(log)
endif()

# Level "1" API as defined for MinCore.lib
set(WINPR_CORE
    synch
    library
    file
    comm
    pipe
    interlocked
    security
    environment
    crypto
    registry
    path
    io
    memory
    ncrypt
    input
    shell
    utils
    error
    timezone
    sysinfo
    pool
    handle
    thread
)

foreach(DIR ${WINPR_CORE})
  add_subdirectory(${DIR})
  source_group("${DIR}" REGULAR_EXPRESSION "${DIR}/.*\\.[ch]")
endforeach()

set(WINPR_LEVEL2
    winsock
    sspi
    sspicli
    crt
    bcrypt
    rpc
    wtsapi
    dsparse
    smartcard
    nt
    clipboard
)

foreach(DIR ${WINPR_LEVEL2})
  add_subdirectory(${DIR})
  source_group("${DIR}" REGULAR_EXPRESSION "${DIR}/.*\\.[ch]")
endforeach()

set(MODULE_NAME winpr)
list(REMOVE_DUPLICATES WINPR_DEFINITIONS)
list(REMOVE_DUPLICATES WINPR_COMPILE_OPTIONS)
list(REMOVE_DUPLICATES WINPR_LINK_OPTIONS)
list(REMOVE_DUPLICATES WINPR_LINK_DIRS)
list(REMOVE_DUPLICATES WINPR_INCLUDES)

addtargetwithresourcefile(${MODULE_NAME} FALSE "${WINPR_VERSION}" WINPR_SRCS)

if(WITH_RESOURCE_VERSIONING)
  target_compile_definitions(${MODULE_NAME} PRIVATE WITH_RESOURCE_VERSIONING)
endif()
if(WINPR_USE_VENDOR_PRODUCT_CONFIG_DIR)
  target_compile_definitions(${MODULE_NAME} PRIVATE WINPR_USE_VENDOR_PRODUCT_CONFIG_DIR)
endif()

if(APPLE)
  set_target_properties(${MODULE_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION FALSE)
endif()

if(NOT BUILD_SHARED_LIBS)
  set(LINK_OPTS_MODE PUBLIC)
else()
  set(LINK_OPTS_MODE PRIVATE)
endif()
target_link_options(${MODULE_NAME} ${LINK_OPTS_MODE} ${WINPR_LINK_OPTIONS})
target_include_directories(${MODULE_NAME} PRIVATE ${WINPR_INCLUDES})
target_include_directories(${MODULE_NAME} INTERFACE $<INSTALL_INTERFACE:include/winpr${WINPR_VERSION_MAJOR}>)
target_link_directories(${MODULE_NAME} PRIVATE ${WINPR_LINK_DIRS})
target_compile_options(${MODULE_NAME} PRIVATE ${WINPR_COMPILE_OPTIONS})
target_compile_definitions(${MODULE_NAME} PRIVATE ${WINPR_DEFINITIONS})

target_link_libraries(${MODULE_NAME} PRIVATE ${WINPR_LIBS_PRIVATE} PUBLIC ${WINPR_LIBS_PUBLIC})
install(TARGETS ${MODULE_NAME} COMPONENT libraries EXPORT WinPRTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/libwinpr")

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

macro(winpr_tools_module_add)
  file(RELATIVE_PATH _relPath "${WINPR_TOOLS_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
  foreach(_src ${ARGN})
    if(_relPath)
      list(APPEND WINPR_TOOLS_SRCS "${_relPath}/${_src}")
    else()
      list(APPEND WINPR_TOOLS_SRCS "${_src}")
    endif()
  endforeach()
  if(_relPath)
    set(WINPR_TOOLS_SRCS ${WINPR_TOOLS_SRCS} PARENT_SCOPE)
  endif()
endmacro()

macro(winpr_tools_include_directory_add)
  file(RELATIVE_PATH _relPath "${WINPR_TOOLS_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
  foreach(_inc ${ARGN})
    if(IS_ABSOLUTE ${_inc})
      list(APPEND WINPR_TOOLS_INCLUDES "${_inc}")
    else()
      if(_relPath)
        list(APPEND WINPR_TOOLS_INCLUDES "${_relPath}/${_inc}")
      else()
        list(APPEND WINPR_TOOLS_INCLUDES "${_inc}")
      endif()
    endif()
  endforeach()
  if(_relPath)
    set(WINPR_TOOLS_INCLUDES ${WINPR_TOOLS_INCLUDES} PARENT_SCOPE)
  endif()
endmacro()

macro(winpr_tools_library_add)
  foreach(_lib ${ARGN})
    list(APPEND WINPR_TOOLS_LIBS "${_lib}")
  endforeach()
  set(WINPR_TOOLS_LIBS ${WINPR_TOOLS_LIBS} PARENT_SCOPE)
endmacro()

macro(winpr_tools_definition_add)
  foreach(_define ${ARGN})
    list(APPEND WINPR_TOOLS_DEFINITONS "${_define}")
  endforeach()
  set(WINPR_TOOLS_DEFINITONS ${WINPR_TOOLS_DEFINITONS} PARENT_SCOPE)
endmacro()

add_subdirectory(makecert)

set(MODULE_NAME winpr-tools)
list(REMOVE_DUPLICATES WINPR_TOOLS_DEFINITIONS)
list(REMOVE_DUPLICATES WINPR_TOOLS_INCLUDES)
include_directories(${WINPR_TOOLS_INCLUDES})

addtargetwithresourcefile(${MODULE_NAME} FALSE "${WINPR_VERSION}" WINPR_TOOLS_SRCS)

add_compile_definitions(${WINPR_DEFINITIONS})
target_include_directories(${MODULE_NAME} INTERFACE $<INSTALL_INTERFACE:include/winpr${WINPR_VERSION_MAJOR}>)
target_link_libraries(${MODULE_NAME} PRIVATE ${WINPR_TOOLS_LIBS})

install(TARGETS ${MODULE_NAME} COMPONENT libraries EXPORT WinPR-toolsTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/Tools")

# Add all command line utilities
add_subdirectory(makecert-cli)
add_subdirectory(hash-cli)

include(pkg-config-install-prefix)
cleaning_configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/winpr-tools.pc.in ${CMAKE_CURRENT_BINARY_DIR}/winpr-tools${WINPR_TOOLS_VERSION_MAJOR}.pc
  @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/winpr-tools${WINPR_TOOLS_VERSION_MAJOR}.pc
        DESTINATION ${PKG_CONFIG_PC_INSTALL_DIR}
)

export(PACKAGE ${MODULE_NAME})

setfreerdpcmakeinstalldir(WINPR_CMAKE_INSTALL_DIR "WinPR-tools${WINPR_VERSION_MAJOR}")

configure_package_config_file(
  WinPR-toolsConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/WinPR-toolsConfig.cmake
  INSTALL_DESTINATION ${WINPR_CMAKE_INSTALL_DIR} PATH_VARS WINPR_INCLUDE_DIR
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/WinPR-toolsConfigVersion.cmake VERSION ${WINPR_VERSION} COMPATIBILITY SameMajorVersion
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/WinPR-toolsConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/WinPR-toolsConfigVersion.cmake DESTINATION ${WINPR_CMAKE_INSTALL_DIR}
)

install(EXPORT WinPR-toolsTargets DESTINATION ${WINPR_CMAKE_INSTALL_DIR})

# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP Client Common
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

set(MODULE_NAME "freerdp-client")
set(MODULE_PREFIX "FREERDP_CLIENT")

#  Policy CMP0022: INTERFACE_LINK_LIBRARIES defines the link
#  interface.  Run "cmake --help-policy CMP0022" for policy details.  Use the
#  cmake_policy command to set the policy and suppress this warning.
if(POLICY CMP0022)
  cmake_policy(SET CMP0022 NEW)
endif()

set(SRCS
    client.c
    cmdline.c
    cmdline.h
    file.c
    client_cliprdr_file.c
    geometry.c
    smartcard_cli.c
)

foreach(FREERDP_CHANNELS_CLIENT_SRC ${FREERDP_CHANNELS_CLIENT_SRCS})
  get_filename_component(NINC ${FREERDP_CHANNELS_CLIENT_SRC} PATH)
  include_directories(${NINC})
  list(APPEND SRCS "${FREERDP_CHANNELS_CLIENT_SRC}")
endforeach()

if(NOT APPLE AND NOT WIN32 AND NOT ANDROID)
  set(OPT_FUSE_DEFAULT ON)
else()
  set(OPT_FUSE_DEFAULT OFF)
endif()

option(WITH_FUSE "Build clipboard with FUSE file copy support" ${OPT_FUSE_DEFAULT})
if(WITH_FUSE)
  find_package(PkgConfig REQUIRED)

  pkg_check_modules(FUSE3 REQUIRED fuse3)
  include_directories(SYSTEM ${FUSE3_INCLUDE_DIRS})
  add_compile_definitions(WITH_FUSE)
  list(APPEND LIBS ${FUSE3_LIBRARIES})

  add_compile_definitions(_FILE_OFFSET_BITS=64)
endif()

include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})

addtargetwithresourcefile(${MODULE_NAME} FALSE "${FREERDP_VERSION}" SRCS)

list(APPEND LIBS freerdp winpr)

include(CheckLibraryExists)
check_library_exists(m lround "" HAVE_LIB_M)
if(HAVE_LIB_M)
  list(APPEND LIBS m)
endif()

target_include_directories(${MODULE_NAME} INTERFACE $<INSTALL_INTERFACE:include>)
target_link_libraries(${MODULE_NAME} PRIVATE ${FREERDP_CHANNELS_CLIENT_LIBS})
target_link_libraries(${MODULE_NAME} PUBLIC ${LIBS})

install(TARGETS ${MODULE_NAME} COMPONENT libraries EXPORT FreeRDP-ClientTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Client/Common")

if(BUILD_TESTING_INTERNAL OR BUILD_TESTING)
  add_subdirectory(test)
endif()

if(WITH_MANPAGES)
  add_subdirectory(man)
endif()

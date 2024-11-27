# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP Server Common
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

set(MODULE_NAME "freerdp-server")
set(MODULE_PREFIX "FREERDP_SERVER")

#  Policy CMP0022: INTERFACE_LINK_LIBRARIES defines the link
#  interface.  Run "cmake --help-policy CMP0022" for policy details.  Use the
#  cmake_policy command to set the policy and suppress this warning.
if(POLICY CMP0022)
  cmake_policy(SET CMP0022 NEW)
endif()

set(${MODULE_PREFIX}_SRCS server.c)

foreach(FREERDP_CHANNELS_SERVER_SRC ${FREERDP_CHANNELS_SERVER_SRCS})
  set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} "${FREERDP_CHANNELS_SERVER_SRC}")
endforeach()

if(MSVC)
  set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS})
endif()

addtargetwithresourcefile(${MODULE_NAME} FALSE "${FREERDP_VERSION}" ${MODULE_PREFIX}_SRCS)

target_include_directories(${MODULE_NAME} INTERFACE $<INSTALL_INTERFACE:include>)
target_link_libraries(${MODULE_NAME} PRIVATE ${FREERDP_CHANNELS_SERVER_LIBS})
target_link_libraries(${MODULE_NAME} PUBLIC winpr freerdp)

install(TARGETS ${MODULE_NAME} COMPONENT libraries EXPORT FreeRDP-ServerTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Server/Common")

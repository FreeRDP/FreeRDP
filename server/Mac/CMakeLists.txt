# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP Mac OS X Server cmake build script
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

set(MODULE_NAME "mfreerdp-server")
set(MODULE_PREFIX "FREERDP_SERVER_MAC")

find_library(AUDIO_TOOL AudioToolbox)
find_library(CORE_AUDIO CoreAudio)
find_library(CORE_VIDEO CoreVideo)
find_library(APP_SERVICES ApplicationServices)
find_library(IOKIT IOKit)
find_library(IOSURFACE IOSurface)
find_library(CARBON Carbon)

set(${MODULE_PREFIX}_SRCS
    mfreerdp.c
    mfreerdp.h
    mf_interface.c
    mf_interface.h
    mf_event.c
    mf_event.h
    mf_peer.c
    mf_peer.h
    mf_info.c
    mf_info.h
    mf_input.c
    mf_input.h
    mf_mountain_lion.c
    mf_mountain_lion.h
)

if(CHANNEL_AUDIN_SERVER)
  set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} mf_audin.c mf_audin.h)
endif()

if(CHANNEL_RDPSND_SERVER)
  set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} mf_rdpsnd.c mf_rdpsnd.h)

endif()

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})
if(WITH_BINARY_VERSIONING)
  set_target_properties(${MODULE_NAME} PROPERTIES OUTPUT_NAME "${MODULE_NAME}${FREERDP_API_VERSION}")
endif()

set(${MODULE_PREFIX}_LIBS
    ${${MODULE_PREFIX}_LIBS}
    freerdp-server
    ${AUDIO_TOOL}
    ${CORE_AUDIO}
    ${CORE_VIDEO}
    ${APP_SERVICES}
    ${IOKIT}
    ${IOSURFACE}
    ${CARBON}
)

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} winpr freerdp)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})
install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Server/Mac")

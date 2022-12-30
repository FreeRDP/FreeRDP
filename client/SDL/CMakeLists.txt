# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP SDL Client
#
# Copyright 2022 Armin Novak <anovak@thincast.com>
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

set(MODULE_NAME "sdl-client")
set(MODULE_PREFIX "FREERDP_CLIENT_X11_CONTROL")

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})
message("xxxxxxxxxxxxx ${SDL2_INCLUDE_DIRS}")
set(SRCS
    sdl_utils.c
    sdl_utils.h
    sdl_kbd.c
    sdl_kbd.h
    sdl_touch.c
    sdl_touch.h
    sdl_pointer.c
    sdl_pointer.h
    sdl_disp.c
    sdl_disp.h
    sdl_monitor.c
    sdl_monitor.h
    sdl_freerdp.h
    sdl_freerdp.c
    sdl_channels.h
    sdl_channels.c)

set(LIBS
	${SDL2_LIBRARIES}
        freerdp-client)

add_executable(${MODULE_NAME} ${SRCS})
set_target_properties(${MODULE_NAME} PROPERTIES OUTPUT_NAME "sdl-freerdp")
target_link_libraries(${MODULE_NAME} PRIVATE ${LIBS})
set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Client/SDL")
install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT client)

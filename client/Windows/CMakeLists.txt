# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP Windows cmake build script
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

set(MODULE_NAME "wfreerdp-client")
set(MODULE_PREFIX "FREERDP_CLIENT_WINDOWS_CONTROL")

set(${MODULE_PREFIX}_SRCS
    wf_gdi.c
    wf_gdi.h
    wf_event.c
    wf_event.h
    wf_channels.c
    wf_channels.h
    wf_graphics.c
    wf_graphics.h
    wf_cliprdr.c
    wf_cliprdr.h
    wf_rail.c
    wf_rail.h
    wf_client.c
    wf_client.h
    wf_floatbar.c
    wf_floatbar.h
    wf_defaults.h
    wf_defaults.c
    resource/wfreerdp.rc
    resource/resource.h
)

option(WITH_WINDOWS_CERT_STORE
       "Build ${MODULE_NAME} with additional certificate validation against windows certificate store" ON
)
if(WITH_WINDOWS_CERT_STORE)
  add_compile_definitions("WITH_WINDOWS_CERT_STORE")
endif()

option(WITH_WIN_CONSOLE "Build ${MODULE_NAME} with console support" OFF)
if(WITH_WIN_CONSOLE)
  add_compile_definitions("WITH_WIN_CONSOLE")
  set(WIN32_GUI_FLAG "TRUE")
else()
  set(WIN32_GUI_FLAG "WIN32")
endif()

option(WITH_PROGRESS_BAR "Build ${MODULE_NAME} with connect progress bar (Windows 7+ or 2008 R2+)" ON)
if(WITH_PROGRESS_BAR)
  add_compile_definitions("WITH_PROGRESS_BAR")
endif()

if(CLIENT_INTERFACE_SHARED)
  addtargetwithresourcefile(${MODULE_NAME} "SHARED" "${FREERDP_VERSION}" ${MODULE_PREFIX}_SRCS)
else()
  addtargetwithresourcefile(${MODULE_NAME} FALSE "${FREERDP_VERSION}" ${MODULE_PREFIX}_SRCS)
endif()
target_include_directories(${MODULE_NAME} INTERFACE $<INSTALL_INTERFACE:include>)

list(APPEND PUB_LIBS freerdp-client)
list(APPEND PUB_LIBS winpr freerdp)

list(APPEND PRIV_LIBS msimg32.lib credui.lib)

if(MINGW)
  list(APPEND PRIV_LIBS ntdll.lib) # only required with MINGW
endif()
target_link_libraries(${MODULE_NAME} PUBLIC ${PUB_LIBS})
target_link_libraries(${MODULE_NAME} PRIVATE ${PRIV_LIBS})

if(WITH_CLIENT_INTERFACE)
  install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries)
endif()
add_subdirectory(cli)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Client/Windows")

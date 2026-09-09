# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP X11 cmake build script
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

set(SRCS xfreerdp.c)

addtargetwithresourcefile(${MODULE_NAME} TRUE "${PROJECT_VERSION}" SRCS)
set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "..")
set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "Client/X11")

option(WITH_XFREERDP_I18N "Build gettext catalogs for the xfreerdp client help" ON)
if(WITH_XFREERDP_I18N)
  include(GNUInstallDirs)
  find_package(Gettext)
  find_package(Intl)
  if(Gettext_FOUND AND Intl_FOUND)
    gettext_process_po_files(
      sv
      ALL
      INSTALL_DESTINATION "${CMAKE_INSTALL_LOCALEDIR}"
      PO_FILES po/xfreerdp.po
    )
    target_compile_definitions(${MODULE_NAME} PRIVATE WITH_XFREERDP_I18N
                                                      FREERDP_I18N_LOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}")
    target_link_libraries(${MODULE_NAME} PRIVATE Intl::Intl)
  else()
    message(STATUS "xfreerdp i18n disabled: gettext tools or libintl were not found")
  endif()
endif()

list(APPEND LIBS xfreerdp-client)

target_link_libraries(${MODULE_NAME} PRIVATE ${LIBS})

installwithrpath(
  TARGETS
  ${MODULE_NAME}
  RUNTIME
  DESTINATION
  ${CMAKE_INSTALL_BINDIR}
  COMPONENT
  client
)
install_freerdp_desktop("${MODULE_NAME}")

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Client/X11")

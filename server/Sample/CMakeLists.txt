# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP Sample Server cmake build script
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

set(MODULE_NAME "sfreerdp-server")
set(MODULE_PREFIX "FREERDP_SERVER_SAMPLE")

set(SRCS
	sfreerdp.c
	sfreerdp.h
	sf_audin.c
	sf_audin.h
	sf_rdpsnd.c
	sf_rdpsnd.h
	sf_encomsp.c
	sf_encomsp.h)

if (CHANNEL_AINPUT_SERVER)
    list(APPEND SRCS sf_ainput.c sf_ainput.h)
endif()

if (WITH_BINARY_VERSIONING)
  set(SAMPLE_RESOURCE_ROOT ${CMAKE_INSTALL_FULL_DATAROOTDIR}/FreeRDP${FREERDP_VERSION_MAJOR}/images)
else()
  set(SAMPLE_RESOURCE_ROOT ${CMAKE_INSTALL_FULL_DATAROOTDIR}/FreeRDP/images)
endif()

set(SAMPLE_ICONS
    test_icon.bmp
    test_icon.png
    test_icon.jpg
    test_icon.webp
)
install(FILES ${SAMPLE_ICONS} DESTINATION ${SAMPLE_RESOURCE_ROOT})

# We need this in runtime path for TestConnect
file(COPY test_icon.bmp DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

AddTargetWithResourceFile(${MODULE_NAME} TRUE "${FREERDP_VERSION}" SRCS)

target_compile_definitions(${MODULE_NAME}
  PRIVATE
    -DSAMPLE_RESOURCE_ROOT="${SAMPLE_RESOURCE_ROOT}"
)
list(APPEND LIBS freerdp-server)
list(APPEND LIBS winpr freerdp)

target_link_libraries(${MODULE_NAME} ${LIBS})
install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT server)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Server/Sample")

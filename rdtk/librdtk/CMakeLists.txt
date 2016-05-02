# RdTk: Remote Desktop Toolkit
#
# Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

set(MODULE_NAME "rdtk")
set(MODULE_PREFIX "RDTK")


set(${MODULE_PREFIX}_SRCS
	rdtk_resources.c
	rdtk_resources.h
	rdtk_surface.c
	rdtk_surface.h
	rdtk_font.c
	rdtk_font.h
	rdtk_button.c
	rdtk_button.h
	rdtk_label.c
	rdtk_label.h
	rdtk_nine_patch.c
	rdtk_nine_patch.h
	rdtk_text_field.c
	rdtk_text_field.h
	rdtk_engine.c
	rdtk_engine.h)

add_library(${MODULE_NAME} STATIC ${${MODULE_PREFIX}_SRCS})

list(APPEND ${MODULE_PREFIX}_LIBS winpr)
list(APPEND ${MODULE_PREFIX}_LIBS freerdp)

target_link_libraries(${MODULE_NAME} ${PRIVATE_KEYWORD} ${${MODULE_PREFIX}_LIBS})

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "RdTk")

if(BUILD_TESTING)
	add_subdirectory(test)
endif()

if (NOT BUILD_SHARED_LIBS)
	install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} EXPORT FreeRDP-ShadowTargets)
endif()

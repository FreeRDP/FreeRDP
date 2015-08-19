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

include_directories(${OPENSSL_INCLUDE_DIR})

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

# On windows create dll version information.
# Vendor, product and year are already set in top level CMakeLists.txt
if (WIN32)
  set (RC_VERSION_MAJOR ${RDTK_VERSION_MAJOR})
  set (RC_VERSION_MINOR ${RDTK_VERSION_MINOR})
  set (RC_VERSION_BUILD ${RDTK_VERSION_REVISION})
  set (RC_VERSION_FILE "${CMAKE_SHARED_LIBRARY_PREFIX}${MODULE_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}" )

  configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/WindowsDLLVersion.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.rc
    @ONLY)

  set ( ${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

add_library(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

list(APPEND ${MODULE_PREFIX}_LIBS winpr)
list(APPEND ${MODULE_PREFIX}_LIBS freerdp)

if (WITH_LIBRARY_VERSIONING)
	set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${RDTK_VERSION} SOVERSION ${RDTK_API_VERSION})
endif()

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} EXPORT RdTkTargets)
if (MSVC AND BUILD_SHARED_LIBS)
	install(FILES ${CMAKE_PDB_BINARY_DIR}/${MODULE_NAME}.pdb DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT symbols)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "RdTk")

if(BUILD_TESTING)
	add_subdirectory(test)
endif()

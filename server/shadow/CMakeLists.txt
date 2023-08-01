# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP Shadow Server cmake build script
#
# Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#	 http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# freerdp-shadow  library

set(MODULE_NAME "freerdp-shadow")

set(SRCS
	shadow_client.c
	shadow_client.h
	shadow_lobby.c
	shadow_lobby.h
	shadow_input.c
	shadow_input.h
	shadow_screen.c
	shadow_screen.h
	shadow_surface.c
	shadow_surface.h
	shadow_encoder.c
	shadow_encoder.h
	shadow_capture.c
	shadow_capture.h
	shadow_channels.c
	shadow_channels.h
	shadow_encomsp.c
	shadow_encomsp.h
	shadow_remdesk.c
	shadow_remdesk.h
	shadow_rdpsnd.c
	shadow_rdpsnd.h
	shadow_audin.c
	shadow_audin.h
	shadow_rdpgfx.c
	shadow_rdpgfx.h
	shadow_subsystem.c
	shadow_subsystem.h
	shadow_mcevent.c
	shadow_mcevent.h
	shadow_server.c
	shadow.h)

if (NOT FREERDP_UNIFIED_BUILD)
	find_package(rdtk 0 REQUIRED)
	include_directories(${RDTK_INCLUDE_DIR})
else()
	if (NOT WITH_RDTK)
		message(FATAL_ERROR "-DWITH_RDTK=ON is required for unified FreeRDP build with shadow server")
	endif()
	include_directories(${PROJECT_SOURCE_DIR}/rdtk/include)
	include_directories(${PROJECT_BINARY_DIR}/rdtk/include)
endif()

# On windows create dll version information.
# Vendor, product and year are already set in top level CMakeLists.txt
if (WIN32)
  set (RC_VERSION_MAJOR ${FREERDP_VERSION_MAJOR})
  set (RC_VERSION_MINOR ${FREERDP_VERSION_MINOR})
  set (RC_VERSION_BUILD ${FREERDP_VERSION_REVISION})
  set (RC_VERSION_PATCH 0)
  set (RC_VERSION_FILE "${CMAKE_SHARED_LIBRARY_PREFIX}${MODULE_NAME}${FREERDP_API_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}" )

  configure_file(
	${PROJECT_SOURCE_DIR}/cmake/WindowsDLLVersion.rc.in
	${CMAKE_CURRENT_BINARY_DIR}/version.rc
	@ONLY)

list (APPEND SRCS ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

add_library(${MODULE_NAME} ${SRCS})

list(APPEND LIBS
	freerdp
	freerdp-server
	winpr
	winpr-tools
	rdtk
)

target_include_directories(${MODULE_NAME} INTERFACE $<INSTALL_INTERFACE:include>)
target_link_libraries(${MODULE_NAME} PRIVATE ${LIBS})

set_target_properties(${MODULE_NAME} PROPERTIES OUTPUT_NAME ${MODULE_NAME}${FREERDP_VERSION_MAJOR})
if (WITH_LIBRARY_VERSIONING)
	  set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${FREERDP_VERSION} SOVERSION ${FREERDP_API_VERSION})
endif()

install(TARGETS ${MODULE_NAME} COMPONENT server EXPORT FreeRDP-ShadowTargets
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if (WITH_DEBUG_SYMBOLS AND MSVC)
	get_target_property(OUTPUT_FILENAME ${MODULE_NAME} OUTPUT_NAME)
	install(FILES ${PROJECT_BINARY_DIR}/${OUTPUT_FILENAME}.pdb DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT symbols)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Server/shadow")

# subsystem library

set(MODULE_NAME "freerdp-shadow-subsystem")

set(SRCS
	shadow_subsystem_builtin.c)

if(WIN32)
	add_subdirectory(Win)
elseif(NOT APPLE)
	add_subdirectory(X11)
elseif(APPLE AND NOT IOS)
	add_subdirectory(Mac)
endif()

if (WIN32)
  set (RC_VERSION_MAJOR ${FREERDP_VERSION_MAJOR})
  set (RC_VERSION_MINOR ${FREERDP_VERSION_MINOR})
  set (RC_VERSION_BUILD ${FREERDP_VERSION_REVISION})
  set (RC_VERSION_FILE "${MODULE_NAME}${CMAKE_EXECUTABLE_SUFFIX}" )

  configure_file(
	${PROJECT_SOURCE_DIR}/cmake/WindowsDLLVersion.rc.in
	${CMAKE_CURRENT_BINARY_DIR}/version.rc
	@ONLY)

  set ( SRCS ${SRCS} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

add_library(${MODULE_NAME} ${SRCS})

list(APPEND LIBS
	freerdp-shadow-subsystem-impl
	freerdp-shadow
	freerdp
	winpr
)

target_include_directories(${MODULE_NAME} INTERFACE $<INSTALL_INTERFACE:include>)
target_link_libraries(${MODULE_NAME} PRIVATE ${LIBS})

set_target_properties(${MODULE_NAME} PROPERTIES OUTPUT_NAME ${MODULE_NAME}${FREERDP_API_VERSION})
if (WITH_LIBRARY_VERSIONING)
	set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${FREERDP_VERSION} SOVERSION ${FREERDP_API_VERSION})
endif()

if (NOT BUILD_SHARED_LIBS)
	install(TARGETS freerdp-shadow-subsystem-impl
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        EXPORT FreeRDP-ShadowTargets
    )
endif()

install(TARGETS ${MODULE_NAME} COMPONENT server EXPORT FreeRDP-ShadowTargets
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if (WITH_DEBUG_SYMBOLS AND MSVC)
	get_target_property(OUTPUT_FILENAME ${MODULE_NAME} OUTPUT_NAME)
	install(FILES ${PROJECT_BINARY_DIR}/${OUTPUT_FILENAME}.pdb DESTINATION ${CMAKE_INSTALL_LIBDIR}
		COMPONENT symbols)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Server/shadow")

# command-line executable

set(MODULE_NAME "freerdp-shadow-cli")

set(SRCS
	shadow.c)

# On windows create dll version information.
# Vendor, product and year are already set in top level CMakeLists.txt
if (WIN32)
  set (RC_VERSION_MAJOR ${FREERDP_VERSION_MAJOR})
  set (RC_VERSION_MINOR ${FREERDP_VERSION_MINOR})
  set (RC_VERSION_BUILD ${FREERDP_VERSION_REVISION})
  set (RC_VERSION_FILE "${MODULE_NAME}${CMAKE_EXECUTABLE_SUFFIX}" )

  configure_file(
	${PROJECT_SOURCE_DIR}/cmake/WindowsDLLVersion.rc.in
	${CMAKE_CURRENT_BINARY_DIR}/version.rc
	@ONLY)

  set ( SRCS ${SRCS} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

add_executable(${MODULE_NAME} ${SRCS})

list(APPEND LIBS freerdp-shadow-subsystem freerdp-shadow freerdp winpr)

target_link_libraries(${MODULE_NAME} PRIVATE ${LIBS})

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT server)
if (WITH_DEBUG_SYMBOLS AND MSVC)
	install(FILES ${PROJECT_BINARY_DIR}/${MODULE_NAME}.pdb DESTINATION ${CMAKE_INSTALL_BINDIR}
		COMPONENT symbols)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Server/shadow")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/freerdp-shadow.pc.in ${CMAKE_CURRENT_BINARY_DIR}/freerdp-shadow${FREERDP_VERSION_MAJOR}.pc @ONLY)
configure_file(freerdp-shadow-cli.1.in ${CMAKE_CURRENT_BINARY_DIR}/freerdp-shadow-cli.1)
install_freerdp_man(${CMAKE_CURRENT_BINARY_DIR}/freerdp-shadow-cli.1 1)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/freerdp-shadow${FREERDP_VERSION_MAJOR}.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

export(PACKAGE freerdp-shadow)

SetFreeRDPCMakeInstallDir(FREERDP_SERVER_CMAKE_INSTALL_DIR "FreeRDP-Shadow${FREERDP_VERSION_MAJOR}")

configure_package_config_file(FreeRDP-ShadowConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/FreeRDP-ShadowConfig.cmake
	INSTALL_DESTINATION ${FREERDP_SERVER_CMAKE_INSTALL_DIR}
	PATH_VARS FREERDP_INCLUDE_DIR)

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/FreeRDP-ShadowConfigVersion.cmake
	VERSION ${FREERDP_VERSION} COMPATIBILITY SameMajorVersion)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/FreeRDP-ShadowConfig.cmake
	${CMAKE_CURRENT_BINARY_DIR}/FreeRDP-ShadowConfigVersion.cmake
	DESTINATION ${FREERDP_SERVER_CMAKE_INSTALL_DIR})

install(EXPORT FreeRDP-ShadowTargets DESTINATION ${FREERDP_SERVER_CMAKE_INSTALL_DIR})

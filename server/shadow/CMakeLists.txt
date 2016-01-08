# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP Shadow Server cmake build script
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

set(MODULE_NAME "freerdp-shadow")
set(MODULE_PREFIX "FREERDP_SERVER_SHADOW")

include_directories(${OPENSSL_INCLUDE_DIR})

set(${MODULE_PREFIX}_SRCS
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
	shadow_subsystem.c
	shadow_subsystem.h
	shadow_mcevent.c
	shadow_mcevent.h
	shadow_server.c
	shadow.h)

# On windows create dll version information.
# Vendor, product and year are already set in top level CMakeLists.txt
if (WIN32)
  set (RC_VERSION_MAJOR ${FREERDP_VERSION_MAJOR})
  set (RC_VERSION_MINOR ${FREERDP_VERSION_MINOR})
  set (RC_VERSION_BUILD ${FREERDP_VERSION_REVISION})
  set (RC_VERSION_PATCH 0)
  set (RC_VERSION_FILE "${CMAKE_SHARED_LIBRARY_PREFIX}${MODULE_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}" )

  configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/WindowsDLLVersion.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.rc
    @ONLY)

  set ( ${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

add_library(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

list(APPEND ${MODULE_PREFIX}_LIBS freerdp)
list(APPEND ${MODULE_PREFIX}_LIBS freerdp-server)
list(APPEND ${MODULE_PREFIX}_LIBS winpr)
list(APPEND ${MODULE_PREFIX}_LIBS winpr-makecert-tool)

list(APPEND ${MODULE_PREFIX}_LIBS rdtk)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

if (WITH_LIBRARY_VERSIONING)
	  set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${FREERDP_VERSION} SOVERSION ${FREERDP_API_VERSION})
endif()

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT server)

if (WITH_DEBUG_SYMBOLS AND MSVC)
	install(FILES ${CMAKE_BINARY_DIR}/${MODULE_NAME}.pdb DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT symbols)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Server/shadow")

# command-line executable

set(MODULE_NAME "freerdp-shadow-cli")
set(MODULE_PREFIX "FREERDP_SERVER_SHADOW_CLI")

if(WIN32)
	set(WITH_SHADOW_WIN 1)
elseif(X11_FOUND AND NOT APPLE)
	set(WITH_SHADOW_X11 1)
elseif(APPLE AND NOT IOS)
	set(WITH_SHADOW_MAC 1)
endif()

# Authentication

if(WITH_SHADOW_X11 OR WITH_SHADOW_MAC)
	set(PAM_FEATURE_TYPE "RECOMMENDED")
	set(PAM_FEATURE_PURPOSE "authentication")
	set(PAM_FEATURE_DESCRIPTION "user authentication")

	find_feature(PAM ${PAM_FEATURE_TYPE} ${PAM_FEATURE_PURPOSE} ${PAM_FEATURE_DESCRIPTION})

	if(PAM_FOUND)
		add_definitions(-DWITH_PAM)
		include_directories(${PAM_INCLUDE_DIR})
		list(APPEND ${MODULE_PREFIX}_AUTH_LIBS ${PAM_LIBRARY})
	endif()
endif()

if(WITH_SHADOW_X11)
	set(XEXT_FEATURE_TYPE "RECOMMENDED")
	set(XEXT_FEATURE_PURPOSE "X11 extension")
	set(XEXT_FEATURE_DESCRIPTION "X11 core extensions")

	set(XSHM_FEATURE_TYPE "RECOMMENDED")
	set(XSHM_FEATURE_PURPOSE "X11 shared memory")
	set(XSHM_FEATURE_DESCRIPTION "X11 shared memory extension")

	set(XINERAMA_FEATURE_TYPE "RECOMMENDED")
	set(XINERAMA_FEATURE_PURPOSE "multi-monitor")
	set(XINERAMA_FEATURE_DESCRIPTION "X11 multi-monitor extension")

	set(XTEST_FEATURE_TYPE "RECOMMENDED")
	set(XTEST_FEATURE_PURPOSE "X11 input event injection")
	set(XTEST_FEATURE_DESCRIPTION "X11 input event injection extension")

	set(XCURSOR_FEATURE_TYPE "RECOMMENDED")
	set(XCURSOR_FEATURE_PURPOSE "cursor")
	set(XCURSOR_FEATURE_DESCRIPTION "X11 cursor extension")

	set(XFIXES_FEATURE_TYPE "RECOMMENDED")
	set(XFIXES_FEATURE_PURPOSE "X11 region")
	set(XFIXES_FEATURE_DESCRIPTION "X11 region fix extension")

	set(XRANDR_FEATURE_TYPE "RECOMMENDED")
	set(XRANDR_FEATURE_PURPOSE "X11 resize, rotate and reflect")
	set(XRANDR_FEATURE_DESCRIPTION "X11 resize, rotate and reflect extension")

	set(XDAMAGE_FEATURE_TYPE "RECOMMENDED")
	set(XDAMAGE_FEATURE_PURPOSE "X11 region damage")
	set(XDAMAGE_FEATURE_DESCRIPTION "X11 region damage extension")

	find_feature(Xext ${XEXT_FEATURE_TYPE} ${XEXT_FEATURE_PURPOSE} ${XEXT_FEATURE_DESCRIPTION})
	find_feature(XShm ${XSHM_FEATURE_TYPE} ${XSHM_FEATURE_PURPOSE} ${XSHM_FEATURE_DESCRIPTION})
	find_feature(XTest ${XTEST_FEATURE_TYPE} ${XTEST_FEATURE_PURPOSE} ${XTEST_FEATURE_DESCRIPTION})
	find_feature(Xfixes ${XFIXES_FEATURE_TYPE} ${XFIXES_FEATURE_PURPOSE} ${XFIXES_FEATURE_DESCRIPTION})
	find_feature(XRandR ${XRANDR_FEATURE_TYPE} ${XRANDR_FEATURE_PURPOSE} ${XRANDR_FEATURE_DESCRIPTION})
	find_feature(Xdamage ${XDAMAGE_FEATURE_TYPE} ${XDAMAGE_FEATURE_PURPOSE} ${XDAMAGE_FEATURE_DESCRIPTION})
	find_feature(Xcursor ${XCURSOR_FEATURE_TYPE} ${XCURSOR_FEATURE_PURPOSE} ${XCURSOR_FEATURE_DESCRIPTION})
	find_feature(Xinerama ${XINERAMA_FEATURE_TYPE} ${XINERAMA_FEATURE_PURPOSE} ${XINERAMA_FEATURE_DESCRIPTION})

	if(WITH_X11)
		add_definitions(-DWITH_X11)
		include_directories(${X11_INCLUDE_DIRS})
		list(APPEND ${MODULE_PREFIX}_X11_LIBS ${X11_LIBRARIES})
	endif()

	if(WITH_XSHM)
		add_definitions(-DWITH_XSHM)
		include_directories(${XSHM_INCLUDE_DIRS})
		list(APPEND ${MODULE_PREFIX}_X11_LIBS ${XSHM_LIBRARIES})
	endif()

	if(WITH_XEXT)
		add_definitions(-DWITH_XEXT)
		include_directories(${XEXT_INCLUDE_DIRS})
		list(APPEND ${MODULE_PREFIX}_X11_LIBS ${XEXT_LIBRARIES})
	endif()

	if(WITH_XINERAMA)
		add_definitions(-DWITH_XINERAMA)
		include_directories(${XINERAMA_INCLUDE_DIRS})
		list(APPEND ${MODULE_PREFIX}_X11_LIBS ${XINERAMA_LIBRARIES})
	endif()

	if(WITH_XCURSOR)
		add_definitions(-DWITH_XCURSOR)
		include_directories(${XCURSOR_INCLUDE_DIRS})
		list(APPEND ${MODULE_PREFIX}_X11_LIBS ${XCURSOR_LIBRARIES})
	endif()

	if(WITH_XDAMAGE)
		add_definitions(-DWITH_XDAMAGE)
		include_directories(${XDAMAGE_INCLUDE_DIRS})
		list(APPEND ${MODULE_PREFIX}_X11_LIBS ${XDAMAGE_LIBRARIES})
	endif()

	if(WITH_XFIXES)
		add_definitions(-DWITH_XFIXES)
		include_directories(${XFIXES_INCLUDE_DIRS})
		list(APPEND ${MODULE_PREFIX}_X11_LIBS ${XFIXES_LIBRARIES})
	endif()

	if(WITH_XTEST)
		add_definitions(-DWITH_XTEST)
		include_directories(${XTEST_INCLUDE_DIRS})
		list(APPEND ${MODULE_PREFIX}_X11_LIBS ${XTEST_LIBRARIES})
	endif()

	if(WITH_XRANDR)
		add_definitions(-DWITH_XRANDR)
		include_directories(${XRANDR_INCLUDE_DIRS})
		list(APPEND ${MODULE_PREFIX}_X11_LIBS ${XRANDR_LIBRARIES})
	endif()
endif()

if(WITH_SHADOW_MAC)
	find_library(IOKIT IOKit)
	find_library(IOSURFACE IOSurface)
	find_library(CARBON Carbon)
	list(APPEND ${MODULE_PREFIX}_MAC_LIBS ${IOKIT} ${IOSURFACE} ${CARBON})
endif()

set(${MODULE_PREFIX}_SRCS
	shadow.c)

set(${MODULE_PREFIX}_WIN_SRCS
	Win/win_rdp.c
	Win/win_rdp.h
	Win/win_wds.c
	Win/win_wds.h
	Win/win_dxgi.c
	Win/win_dxgi.h
	Win/win_shadow.c
	Win/win_shadow.h)

set(${MODULE_PREFIX}_X11_SRCS
	X11/x11_shadow.c
	X11/x11_shadow.h)

set(${MODULE_PREFIX}_MAC_SRCS
	Mac/mac_shadow.c
	Mac/mac_shadow.h)

if(WITH_SHADOW_WIN)
	add_definitions(-DWITH_SHADOW_WIN)
	list(APPEND ${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_WIN_SRCS})
	list(APPEND ${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_WIN_LIBS})
elseif(WITH_SHADOW_X11)
	add_definitions(-DWITH_SHADOW_X11)
	list(APPEND ${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_X11_SRCS})
	list(APPEND ${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_X11_LIBS})
elseif(WITH_SHADOW_MAC)
	add_definitions(-DWITH_SHADOW_MAC)
	list(APPEND ${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_MAC_SRCS})
	list(APPEND ${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_MAC_LIBS})
endif()

list(APPEND ${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_AUTH_LIBS})

	# On windows create dll version information.
# Vendor, product and year are already set in top level CMakeLists.txt
if (WIN32)
  set (RC_VERSION_MAJOR ${FREERDP_VERSION_MAJOR})
  set (RC_VERSION_MINOR ${FREERDP_VERSION_MINOR})
  set (RC_VERSION_BUILD ${FREERDP_VERSION_REVISION})
  set (RC_VERSION_FILE "${MODULE_NAME}${CMAKE_EXECUTABLE_SUFFIX}" )

  configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/WindowsDLLVersion.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.rc
    @ONLY)

  set ( ${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} freerdp-shadow)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT server)
if (WITH_DEBUG_SYMBOLS AND MSVC)
	install(FILES ${CMAKE_BINARY_DIR}/${MODULE_NAME}.pdb DESTINATION ${CMAKE_INSTALL_BINDIR}
		COMPONENT symbols)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Server/shadow")

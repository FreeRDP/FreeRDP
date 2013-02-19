# FreeRDP: A Remote Desktop Protocol Implementation
# Android Client
#
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
# Copyright 2013 Martin Fleisz <mfleisz@thinstuff.at>
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

set(IOS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(IOS_PACKAGE_NAME "iFreeRDP")

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/iFreeRDP.xcodeproj/project.pbxproj.cmake ${CMAKE_CURRENT_SOURCE_DIR}/iFreeRDP.xcodeproj/project.pbxproj @ONLY)

if(IOS_BUILD_OBJC)
	# And isn't shiped with the android ndk/sdk so
	# we need to find it on the local machine
	SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER, BOTH)
	find_program(XCODEBUILD_COMMAND xcodebuild)
  	SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER, ONLY)

	if(XCODEBUILD_COMMAND STREQUAL "XCODEBUILD_COMMAND-NOTFOUND")
		message(FATAL_ERROR "xcodebuild not found but required to build ios objective-c")
	endif()

	if(IOS_BUILD_OBJC_DEBUG)
		set(IOS_BUILD_TYPE "Debug")
	else()
		set(IOS_BUILD_TYPE "Release")
	endif()

	set(IOS_APP_OUTPUT_DIR "${IOS_SOURCE_DIR}/bin/${IOS_BUILD_TYPE}")
	set(IOS_APP "${IOS_APP_OUTPUT_DIR}/${IOS_PACKAGE_NAME}.app")
	
	# command to create the ios package
	add_custom_command(
		OUTPUT "${IOS_APP}"
		COMMAND ${XCODEBUILD_COMMAND} -project "${IOS_SOURCE_DIR}/iFreeRDP.xcodeproj" -sdk ${CMAKE_IOS_SDK_ROOT} -configuration ${IOS_BUILD_TYPE} CONFIGURATION_BUILD_DIR="${IOS_APP_OUTPUT_DIR}"
		WORKING_DIRECTORY "${IOS_SOURCE_DIR}"
		MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/iFreeRDP.xcodeproj/project.pbxproj
		DEPENDS freerdp winpr
	)
	add_custom_target(ios-package ALL SOURCES "${IOS_APP}")
	SET_DIRECTORY_PROPERTIES(PROPERTIES  ADDITIONAL_MAKE_CLEAN_FILES "bin;build")
endif()

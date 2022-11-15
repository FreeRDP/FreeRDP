# WinPR: Windows Portable Runtime
# libwinpr-crt cmake build script
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

set (CRT_FILES alignment.c
	conversion.c
	buffer.c
	memory.c
	unicode.c
	string.c)

IF (ANDROID)
	list(APPEND CRT_FILES unicode_android.c)
elseif (NOT APPLE AND NOT WIN32)
	find_package(ICU REQUIRED i18n uc io)
	list(APPEND CRT_FILES unicode_icu.c)
	winpr_include_directory_add(${ICU_INCLUDE_DIRS})
	winpr_library_add_private(${ICU_LIBRARIES})
elseif(APPLE)
	list(APPEND CRT_FILES unicode_apple.m)
	find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
	winpr_library_add_private(${FOUNDATION_FRAMEWORK})
endif()

winpr_module_add(${CRT_FILES})

if(BUILD_TESTING)
	add_subdirectory(test)
endif()

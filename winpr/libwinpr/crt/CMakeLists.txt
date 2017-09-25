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

if (NOT WITH_ICU)
	set (CRT_FILES ${CRT_FILES}
		utf.c
		utf.h)
endif(NOT WITH_ICU)

if (WITH_ICU)
	find_package(ICU REQUIRED i18n uc io)
	include_directories(${ICU_INCLUDE_DIRS})
	winpr_library_add(${ICU_LIBRARIES})
endif (WITH_ICU)

winpr_module_add(${CRT_FILES})

if(BUILD_TESTING)
	add_subdirectory(test)
endif()

# WinPR: Windows Portable Runtime
# libwinpr-wtsapi cmake build script
#
# Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

winpr_module_add(wtsapi.c)

if(WIN32)
	winpr_module_add(wtsapi_win32.c wtsapi_win32.h)
endif()

if(BUILD_TESTING)
	add_subdirectory(test)
endif()

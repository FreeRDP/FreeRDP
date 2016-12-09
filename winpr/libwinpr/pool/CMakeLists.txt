# WinPR: Windows Portable Runtime
# libwinpr-pool cmake build script
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

winpr_module_add(
	synch.c
	work.c
	timer.c
	io.c
	cleanup_group.c
	pool.c
	pool.h
	callback.c
	callback_cleanup.c)

winpr_library_add(
	${CMAKE_THREAD_LIBS_INIT}
	${CMAKE_DL_LIBS})

if(${CMAKE_SYSTEM_NAME} MATCHES SunOS)
	winpr_library_add(rt)
endif()

if(BUILD_TESTING)
	add_subdirectory(test)
endif()



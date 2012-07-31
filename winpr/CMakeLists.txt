# WinPR: Windows Portable Runtime
# winpr cmake build script
#
# Copyright 2011 O.S. Systems Software Ltda.
# Copyright 2011 Otavio Salvador <otavio@ossystems.com.br>
# Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

if (APPLE)
	# flat_namespace should be avoided, but is required for -undefined warning. Since WinPR currently has
	# a lot of undefined symbols in use, use this hack until they're filled out.
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-flat_namespace,-undefined,warning")	
endif()

add_subdirectory(crt)
add_subdirectory(utils)
add_subdirectory(heap)
add_subdirectory(handle)
add_subdirectory(synch)
add_subdirectory(sysinfo)
add_subdirectory(bcrypt)
add_subdirectory(dsparse)
add_subdirectory(asn1)
add_subdirectory(rpc)
add_subdirectory(sspicli)
add_subdirectory(sspi)
add_subdirectory(registry)

add_subdirectory(tools)


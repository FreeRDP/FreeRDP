# WinPR: Windows Portable Runtime
# libwinpr-rpc cmake build script
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
	rpc.c
	ndr.c
	ndr_array.c
	ndr_array.h
	ndr_context.c
	ndr_context.h
	ndr_correlation.c
	ndr_correlation.h
	ndr_pointer.c
	ndr_pointer.h
	ndr_private.c
	ndr_private.h
	ndr_simple.c
	ndr_simple.h
	ndr_string.c
	ndr_string.h
	ndr_structure.c
	ndr_structure.h
	ndr_union.c
	ndr_union.h
	midl.c)

winpr_include_directory_add(${OPENSSL_INCLUDE_DIR})

winpr_library_add(${OPENSSL_LIBRARIES})

if(WIN32)
	winpr_library_add(ws2_32 rpcrt4)
endif()

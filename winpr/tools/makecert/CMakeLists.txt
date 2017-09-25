# WinPR: Windows Portable Runtime
# winpr-makecert cmake build script
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

set(MODULE_NAME "winpr-makecert-tool")
set(MODULE_PREFIX "WINPR_MAKECERT_TOOL")

set(${MODULE_PREFIX}_SRCS makecert.c)

if(OPENSSL_FOUND)
	winpr_tools_include_directory_add(${OPENSSL_INCLUDE_DIR})
endif()

if(MBEDTLS_FOUND)
	winpr_tools_include_directory_add(${MBEDTLS_INCLUDE_DIR})
endif()


winpr_tools_module_add(${${MODULE_PREFIX}_SRCS})

if(OPENSSL_FOUND)
	if(WIN32)
		list(APPEND ${MODULE_PREFIX}_LIBS ${OPENSSL_LIBRARIES})
	else()
		# if ${OPENSSL_LIBRARIES} libssl and libcrypto is linked
		# therefor explicitly link against libcrypto
		list(APPEND ${MODULE_PREFIX}_LIBS ${OPENSSL_CRYPTO_LIBRARIES})
	endif()
endif()

if(MBEDTLS_FOUND)
	list(APPEND ${MODULE_PREFIX}_LIBS ${MBEDTLS_LIBRARIES})
endif()


winpr_tools_library_add(${${MODULE_PREFIX}_LIBS} winpr)

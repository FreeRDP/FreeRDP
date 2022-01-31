# WinPR: Windows Portable Runtime
# libwinpr-ncrypt cmake build script
#
# Copyright 2021 David Fort <contact@hardening-consulting.com>
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
	ncrypt.c
	ncrypt.h
	ncrypt_pkcs11.c
)

if(WITH_PKCS11 AND NOT WIN32)
	winpr_include_directory_add(${PKCS11_INCLUDE_DIR})
	winpr_library_add_public(${PKCS11_LIBRARY})
endif()

if(BUILD_TESTING)
	add_subdirectory(test)
endif()

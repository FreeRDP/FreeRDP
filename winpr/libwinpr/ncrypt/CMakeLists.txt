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

if (PKCS11_FOUND)
	winpr_include_directory_add(${PKCS11_INCLUDE_DIR})
	winpr_library_add_public(${PKCS11_LIBRARY})

	option(WITH_OPENSC_PKCS11_LINKED "Directly link opensc-pkcs11" OFF)
	if (WITH_OPENSC_PKCS11_LINKED)

		# opensc-pkcs11 is installed without any library prefix.
		# Disable this when checking for this library.
		set(backup ${CMAKE_FIND_LIBRARY_PREFIXES})
		set(CMAKE_FIND_LIBRARY_PREFIXES "")
		find_library(OPENSC_PKCS11
			NAMES opensc-pkcs11
			PATH_SUFFIXES pkcs11
			REQUIRED)
		set(CMAKE_FIND_LIBRARY_PREFIXES ${backup})

		winpr_definition_add(-DWITH_OPENSC_PKCS11_LINKED)
		winpr_library_add_private(${OPENSC_PKCS11})
	endif()
endif()

if (PKCS11_FOUND)
	winpr_module_add(
		ncrypt_pkcs11.c
		)
endif()

winpr_module_add(
	ncrypt.c
	ncrypt.h
)

if (WIN32)
	winpr_library_add_public(ncrypt)
endif()

if(BUILD_TESTING)
	add_subdirectory(test)
endif()

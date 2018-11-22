# FreeRDP: A Remote Desktop Protocol Implementation
# libfreerdp-scquery cmake build script
#
# Copyright 2018 Pascal Bourguignon <pjb@informatimago.com>
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

set(MODULE_NAME "freerdp-scquery")
set(MODULE_PREFIX "FREERDP_SCQUERY")

set(${MODULE_PREFIX}_SRCS
    buffer.c
    certificate-list.c
    certificate.c
    error.c
    pkcs11errors.c
    pkcs11module.c
    smartcard-certificate.c
    string.c
    x509_alt_names.c
)

freerdp_module_add(${${MODULE_PREFIX}_SRCS})

freerdp_library_add(${CMAKE_THREAD_LIBS_INIT})

# if(BUILD_TESTING)
# 	add_subdirectory(test)
# endif()

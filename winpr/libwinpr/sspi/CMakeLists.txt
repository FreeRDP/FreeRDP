# WinPR: Windows Portable Runtime
# libwinpr-sspi cmake build script
#
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

set(MODULE_PREFIX "WINPR_SSPI")

set(${MODULE_PREFIX}_NTLM_SRCS
    NTLM/ntlm_av_pairs.c
    NTLM/ntlm_av_pairs.h
    NTLM/ntlm_compute.c
    NTLM/ntlm_compute.h
    NTLM/ntlm_message.c
    NTLM/ntlm_message.h
    NTLM/ntlm.c
    NTLM/ntlm.h
    NTLM/ntlm_export.h
)

set(${MODULE_PREFIX}_KERBEROS_SRCS Kerberos/kerberos.c Kerberos/kerberos.h)

set(${MODULE_PREFIX}_NEGOTIATE_SRCS Negotiate/negotiate.c Negotiate/negotiate.h)

set(${MODULE_PREFIX}_SCHANNEL_SRCS Schannel/schannel_openssl.c Schannel/schannel_openssl.h Schannel/schannel.c
                                   Schannel/schannel.h
)

set(${MODULE_PREFIX}_CREDSSP_SRCS CredSSP/credssp.c CredSSP/credssp.h)

set(${MODULE_PREFIX}_SRCS
    sspi_winpr.c
    sspi_winpr.h
    sspi_export.c
    sspi_gss.c
    sspi_gss.h
    sspi.c
    sspi.h
)

set(KRB5_DEFAULT OFF)
if(NOT WIN32 AND NOT ANDROID AND NOT IOS AND NOT APPLE)
  set(KRB5_DEFAULT ON)
endif()

option(WITH_DEBUG_SCHANNEL "Compile support for SCHANNEL debug" ${DEFAULT_DEBUG_OPTION})
if(WITH_DEBUG_SCHANNEL)
  winpr_definition_add(WITH_DEBUG_SCHANNEL)
endif()

option(WITH_KRB5 "Compile support for kerberos authentication." ${KRB5_DEFAULT})
if(WITH_KRB5)
  find_package(KRB5 REQUIRED)

  list(APPEND ${MODULE_PREFIX}_KERBEROS_SRCS Kerberos/krb5glue.h)

  winpr_include_directory_add(${KRB5_INCLUDEDIR})
  winpr_include_directory_add(${KRB5_INCLUDE_DIRS})
  winpr_library_add_private(${KRB5_LIBRARIES})
  winpr_library_add_private(${KRB5_LIBRARY})
  winpr_library_add_compile_options(${KRB5_CFLAGS})
  winpr_library_add_link_options(${KRB5_LDFLAGS})
  winpr_library_add_link_directory(${KRB5_LIBRARY_DIRS})

  winpr_definition_add(WITH_KRB5)

  if(KRB5_FLAVOUR STREQUAL "MIT")
    winpr_definition_add(WITH_KRB5_MIT)
    list(APPEND ${MODULE_PREFIX}_KERBEROS_SRCS Kerberos/krb5glue_mit.c)
  elseif(KRB5_FLAVOUR STREQUAL "Heimdal")
    winpr_definition_add(WITH_KRB5_HEIMDAL)
    list(APPEND ${MODULE_PREFIX}_KERBEROS_SRCS Kerberos/krb5glue_heimdal.c)
  else()
    message(WARNING "Kerberos version not detected")
  endif()

  include(CMakeDependentOption)
  cmake_dependent_option(
    WITH_KRB5_NO_NTLM_FALLBACK "Do not fall back to NTLM if no kerberos ticket available" OFF "WITH_KRB5" OFF
  )
  if(WITH_KRB5_NO_NTLM_FALLBACK)
    add_compile_definitions("WITH_KRB5_NO_NTLM_FALLBACK")
  endif()
endif()

winpr_module_add(
  ${${MODULE_PREFIX}_CREDSSP_SRCS} ${${MODULE_PREFIX}_NTLM_SRCS} ${${MODULE_PREFIX}_KERBEROS_SRCS}
  ${${MODULE_PREFIX}_NEGOTIATE_SRCS} ${${MODULE_PREFIX}_SCHANNEL_SRCS} ${${MODULE_PREFIX}_SRCS}
)

if(OPENSSL_FOUND)
  winpr_include_directory_add(${OPENSSL_INCLUDE_DIR})
  winpr_library_add_private(${OPENSSL_LIBRARIES})
endif()

if(MBEDTLS_FOUND)
  winpr_include_directory_add(${MBEDTLS_INCLUDE_DIR})
  winpr_library_add_private(${MBEDTLS_LIBRARIES})
endif()

if(WIN32)
  winpr_library_add_public(ws2_32)
endif()

if(BUILD_TESTING_INTERNAL OR BUILD_TESTING)
  add_subdirectory(test)
endif()

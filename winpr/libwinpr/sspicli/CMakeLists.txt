# WinPR: Windows Portable Runtime
# libwinpr-sspicli cmake build script
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

set(SSPICLI_SRCS sspicli.c)

if(NOT WIN32)
  # The LSA client API and its wire codec are backed by the lsa4posix daemon on
  # non-Windows; on Windows the native LSA from <ntsecapi.h> is used instead.
  list(
    APPEND
    SSPICLI_SRCS
    lsa.c
    lsa_transport.c
    lsa_transport.h
    lsa_proto.c
    lsa_proto.h
  )
endif()

winpr_module_add(${SSPICLI_SRCS})

if(NOT WIN32 AND (BUILD_TESTING_INTERNAL OR BUILD_TESTING OR BUILD_FUZZERS))
  add_subdirectory(test)
endif()

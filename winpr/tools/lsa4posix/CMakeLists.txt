# WinPR: Windows Portable Runtime
# lsa4posix daemon cmake build script
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

set(MODULE_NAME "lsa4posix")

# lsa_proto.* is the private wire codec, shared with libwinpr; compile it into the daemon.
set(${MODULE_NAME}_SRCS main.c server.c ap_tsssp.c ap_tsssp.h lsa4posix.h
                        ${CMAKE_CURRENT_SOURCE_DIR}/../../libwinpr/sspicli/lsa_proto.c
)

add_executable(${MODULE_NAME} ${${MODULE_NAME}_SRCS})

target_include_directories(
  ${MODULE_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../libwinpr/sspicli
                         ${PROJECT_SOURCE_DIR}/channels/rdpear/common
)

# rdpear-common carries the [MS-RDPEAR] NDR codec and the Kerberos redirected-call handlers used to
# service LSA_CALL_PACKAGE; it pulls in MIT krb5.
target_link_libraries(${MODULE_NAME} PRIVATE winpr rdpear-common ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT tools)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/Tools")

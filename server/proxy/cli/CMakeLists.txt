# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP Proxy Server
#
# Copyright 2021 Armin Novak <armin.novak@thincast.com>
# Copyright 2021 Thincast Technologies GmbH
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

set(PROXY_APP_SRCS freerdp_proxy.c)

set(APP_NAME "freerdp-proxy")
addtargetwithresourcefile(${APP_NAME} TRUE "${FREERDP_VERSION}" PROXY_APP_SRCS)

target_link_libraries(${APP_NAME} ${MODULE_NAME})
install(TARGETS ${APP_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT server)

set_property(TARGET ${APP_NAME} PROPERTY FOLDER "Server/proxy")

generate_and_install_freerdp_man_from_template(${APP_NAME} "1" "${FREERDP_API_VERSION}")

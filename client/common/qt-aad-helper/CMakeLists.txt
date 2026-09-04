# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP out-of-process Qt-based AAD auth helper
#
# Copyright 2026 David Fort <contact@hardening-consulting.com>
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

set(MODULE_NAME "freerdp-qt-aad-helper")

# the top-level project only declares C; this may be the first CXX target reached in the build
enable_language(CXX)

find_package(Qt6 QUIET COMPONENTS Widgets WebEngineWidgets)
if(Qt6_FOUND)
  set(QT_AAD_AUTH_HELPER_LIBS Qt6::Widgets Qt6::WebEngineWidgets)
else()
  find_package(Qt5 5.12 REQUIRED COMPONENTS Widgets WebEngineWidgets)
  set(QT_AAD_AUTH_HELPER_LIBS Qt5::Widgets Qt5::WebEngineWidgets)
endif()

add_executable(${MODULE_NAME} main.cpp redirect_watcher.hpp redirect_watcher.cpp)
set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Client/Common")
set_target_properties(${MODULE_NAME} PROPERTIES AUTOMOC ON)

target_link_libraries(${MODULE_NAME} PRIVATE winpr ${QT_AAD_AUTH_HELPER_LIBS})

installwithrpath(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT client)

if(BUILD_TESTING_INTERNAL OR BUILD_TESTING)
  add_subdirectory(test)
endif()

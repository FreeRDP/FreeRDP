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

cmake_minimum_required(VERSION 3.13)
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()
if(NOT FREERDP_DEFAULT_PROJECT_VERSION)
  set(FREERDP_DEFAULT_PROJECT_VERSION "1.0.0.0")
endif()

project(freerdp-qt-aad-helper VERSION ${FREERDP_DEFAULT_PROJECT_VERSION} LANGUAGES CXX)

find_package(Qt6 QUIET COMPONENTS Widgets WebEngineWidgets)
if(Qt6_FOUND)
  set(QT_AAD_AUTH_HELPER_LIBS Qt6::Widgets Qt6::WebEngineWidgets)
else()
  find_package(Qt5 5.12 REQUIRED COMPONENTS Widgets WebEngineWidgets)
  set(QT_AAD_AUTH_HELPER_LIBS Qt5::Widgets Qt5::WebEngineWidgets)
endif()

add_executable(${PROJECT_NAME} main.cpp redirect_watcher.hpp redirect_watcher.cpp)
set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "Client/Common")
set_target_properties(${PROJECT_NAME} PROPERTIES AUTOMOC ON)

target_link_libraries(${PROJECT_NAME} PRIVATE winpr ${QT_AAD_AUTH_HELPER_LIBS})

installwithrpath(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT client)

if(BUILD_TESTING_INTERNAL OR BUILD_TESTING)
  add_subdirectory(test)
endif()

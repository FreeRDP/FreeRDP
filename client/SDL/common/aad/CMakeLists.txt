# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP SDL Client
#
# Copyright 2024 Armin Novak <anovak@thincast.com>
# Copyright 2024 Thincast Technologies GmbH
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
set(WITH_WEBVIEW_DEFAULT OFF)

option(WITH_WEBVIEW "Build with WebView support for AAD login popup browser" ${WITH_WEBVIEW_DEFAULT})
if(WITH_WEBVIEW)
  option(WITH_WEBVIEW_QT "Build with QtWebEngine support for AAD login broweser popup" OFF)

  set(SRCS sdl_webview.hpp webview_impl.hpp sdl_webview.cpp)
  set(LIBS winpr)

  if(WITH_WEBVIEW_QT)
    find_package(Qt5 COMPONENTS WebEngineWidgets REQUIRED)

    list(APPEND SRCS qt/webview_impl.cpp)

    list(APPEND LIBS Qt5::WebEngineWidgets)
  else()
    list(APPEND SRCS wrapper/webview.h wrapper/webview_impl.cpp)

    if(APPLE)
      find_library(WEBKIT Webkit REQUIRED)
      list(APPEND LIBS ${WEBKIT})
    elseif(NOT WIN32)
      find_package(PkgConfig REQUIRED)
      pkg_check_modules(WEBVIEW_GTK webkit2gtk-4.1)
      if(NOT WEBVIEW_GTK_FOUND)
        pkg_check_modules(WEBVIEW_GTK webkit2gtk-4.0 REQUIRED)
      endif()
      include_directories(SYSTEM ${WEBVIEW_GTK_INCLUDE_DIRS})
      list(APPEND LIBS ${WEBVIEW_GTK_LIBRARIES})
    endif()
  endif()
else()
  set(SRCS dummy.cpp)
endif()

cleaning_configure_file(sdl_config.hpp.in sdl_config.hpp @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library(sdl-common-aad-view STATIC ${SRCS})
target_include_directories(sdl-common-aad-view PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(sdl-common-aad-view PRIVATE ${LIBS})
target_compile_definitions(sdl-common-aad-view PUBLIC ${DEFINITIONS})
if(WITH_WEBVIEW AND NOT WITH_WEBVIEW_QT)
  include(WebView2)
  target_link_webview2("sdl-common-aad-view")
endif()

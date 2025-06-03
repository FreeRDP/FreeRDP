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

option(WITH_WEBVIEW "Build with WebView support for AAD login popup browser" OFF)
if(WITH_WEBVIEW)
  set(SRCS sdl_webview.hpp webview_impl.hpp sdl_webview.cpp)
  set(LIBS winpr)

  include(FetchContent)

  set(FETCHCONTENT_SOURCE_DIR_WEBVIEW "${CMAKE_CURRENT_SOURCE_DIR}/../../../../external/webview")
  if(IS_DIRECTORY "${FETCHCONTENT_SOURCE_DIR_WEBVIEW}")
    message("Using existing source from ${FETCHCONTENT_SOURCE_DIR_WEBVIEW}")
  else()
    unset(FETCHCONTENT_SOURCE_DIR_WEBVIEW)
  endif()

  set(WEBVIEW_BUILD_DOCS OFF CACHE INTERNAL "fetchcontent default")
  set(WEBVIEW_BUILD_SHARED_LIBRARY OFF CACHE INTERNAL "fetchcontent default")
  set(WEBVIEW_BUILD_STATIC_LIBRARY ON CACHE INTERNAL "fetchcontent default")
  set(WEBVIEW_BUILD_TESTS OFF CACHE INTERNAL "fetchcontent default")
  set(WEBVIEW_BUILD_EXAMPLES OFF CACHE INTERNAL "fetchcontent default")
  FetchContent_Declare(webview GIT_REPOSITORY https://github.com/akallabeth/webview GIT_TAG navigation-listener SYSTEM)
  FetchContent_MakeAvailable(webview)

  list(APPEND SRCS webview_impl.cpp)

  list(APPEND LIBS webview::core)

  include_directories(${CMAKE_CURRENT_BINARY_DIR})

  add_library(sdl-common-aad-view STATIC ${SRCS})
  set_property(TARGET sdl-common-aad-view PROPERTY FOLDER "Client/SDL/Common")

  target_include_directories(sdl-common-aad-view PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
  target_link_libraries(sdl-common-aad-view PRIVATE ${LIBS})
  target_compile_definitions(sdl-common-aad-view PUBLIC ${DEFINITIONS})
else()
  add_library(sdl-common-aad-view STATIC dummy.cpp)
endif()

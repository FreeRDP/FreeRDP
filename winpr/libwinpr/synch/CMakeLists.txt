# WinPR: Windows Portable Runtime
# libwinpr-synch cmake build script
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

winpr_module_add(
  address.c
  barrier.c
  critical.c
  event.c
  init.c
  mutex.c
  pollset.c
  pollset.h
  semaphore.c
  sleep.c
  synch.h
  timer.c
  wait.c
)

if(EMSCRIPTEN)
  # The JavaScript side of pollset.c, for the executable's link: a link item, so that it also
  # reaches the executable through static libraries (link options of their private dependencies
  # don't).
  set(_js pollset_emscripten.js)
  set(_js_dir "${CMAKE_INSTALL_LIBDIR}/winpr${WINPR_VERSION_MAJOR}")
  winpr_library_add_public(
    "--js-library=$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${_js_dir}>/${_js}"
  )
  # so that editing the JavaScript recompiles this file, and so relinks the executables
  set_source_files_properties(pollset.c PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${_js})
  if(NOT BUILD_SHARED_LIBS)
    install(FILES ${_js} DESTINATION ${_js_dir})
  endif()
endif()

if(EPOLLSHIM)
  winpr_library_add_private(PkgConfig::EPOLLSHIM)
endif()

if(BUILD_TESTING)
  add_subdirectory(test)
endif()

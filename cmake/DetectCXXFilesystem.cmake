# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script
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

include(CheckCXXSourceCompiles)

set(FREERDP_CXX_FILESYSTEM_TEST_SOURCE
    "
#ifndef __has_include
#define __has_include(x) 0
#endif

#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#error Could not find system header \"<filesystem>\" or \"<experimental/filesystem>\"
#endif

int main()
{
	fs::path path{\".\"};
	return fs::exists(path) ? 0 : 0;
}
"
)

set(FREERDP_CXX_FILESYSTEM_LIBRARIES "")
check_cxx_source_compiles("${FREERDP_CXX_FILESYSTEM_TEST_SOURCE}" FREERDP_CXX_FILESYSTEM_LINKS_WITHOUT_LIB)

if(NOT FREERDP_CXX_FILESYSTEM_LINKS_WITHOUT_LIB)
  set(_FREERDP_CXX_FILESYSTEM_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES}")

  set(CMAKE_REQUIRED_LIBRARIES stdc++fs)
  check_cxx_source_compiles("${FREERDP_CXX_FILESYSTEM_TEST_SOURCE}" FREERDP_CXX_FILESYSTEM_LINKS_WITH_STDCXXFS)
  if(FREERDP_CXX_FILESYSTEM_LINKS_WITH_STDCXXFS)
    set(FREERDP_CXX_FILESYSTEM_LIBRARIES stdc++fs)
  else()
    set(CMAKE_REQUIRED_LIBRARIES c++fs)
    check_cxx_source_compiles("${FREERDP_CXX_FILESYSTEM_TEST_SOURCE}" FREERDP_CXX_FILESYSTEM_LINKS_WITH_CXXFS)
    if(FREERDP_CXX_FILESYSTEM_LINKS_WITH_CXXFS)
      set(FREERDP_CXX_FILESYSTEM_LIBRARIES c++fs)
    endif()
  endif()

  set(CMAKE_REQUIRED_LIBRARIES "${_FREERDP_CXX_FILESYSTEM_REQUIRED_LIBRARIES}")
endif()

if(NOT FREERDP_CXX_FILESYSTEM_LINKS_WITHOUT_LIB AND NOT FREERDP_CXX_FILESYSTEM_LIBRARIES)
  message(FATAL_ERROR "C++ filesystem support is required but could not be linked")
endif()

add_library(freerdp-cxx-filesystem INTERFACE)
if(FREERDP_CXX_FILESYSTEM_LIBRARIES)
  target_link_libraries(freerdp-cxx-filesystem INTERFACE ${FREERDP_CXX_FILESYSTEM_LIBRARIES})
endif()

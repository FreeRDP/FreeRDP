# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP C++ filesystem link helper
#
# Copyright 2026
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

include_guard(GLOBAL)

include(CheckCXXSourceCompiles)
include(CMakePushCheckState)

set(FREERDP_CXX_FILESYSTEM_SOURCE
    [=[
#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#error Could not find system header "<filesystem>" or "<experimental/filesystem>"
#endif

int main(int argc, char** argv)
{
	fs::path path = argc > 1 ? argv[1] : ".";
	return fs::exists(path) ? 0 : 0;
}
]=]
)

cmake_push_check_state(RESET)
check_cxx_source_compiles("${FREERDP_CXX_FILESYSTEM_SOURCE}" FREERDP_CXX_FILESYSTEM_WITHOUT_LIB)
cmake_pop_check_state()

if(FREERDP_CXX_FILESYSTEM_WITHOUT_LIB)
  set(FREERDP_CXX_FILESYSTEM_LIBRARIES "" CACHE STRING "Libraries required for C++ filesystem support" FORCE)
else()
  foreach(FREERDP_CXX_FILESYSTEM_LIBRARY stdc++fs c++fs)
    string(MAKE_C_IDENTIFIER "${FREERDP_CXX_FILESYSTEM_LIBRARY}" FREERDP_CXX_FILESYSTEM_LIBRARY_ID)

    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_LIBRARIES "${FREERDP_CXX_FILESYSTEM_LIBRARY}")
    check_cxx_source_compiles(
      "${FREERDP_CXX_FILESYSTEM_SOURCE}" FREERDP_CXX_FILESYSTEM_WITH_${FREERDP_CXX_FILESYSTEM_LIBRARY_ID}
    )
    cmake_pop_check_state()

    if(FREERDP_CXX_FILESYSTEM_WITH_${FREERDP_CXX_FILESYSTEM_LIBRARY_ID})
      set(FREERDP_CXX_FILESYSTEM_LIBRARIES "${FREERDP_CXX_FILESYSTEM_LIBRARY}"
          CACHE STRING "Libraries required for C++ filesystem support" FORCE
      )
      break()
    endif()
  endforeach()
endif()

mark_as_advanced(FREERDP_CXX_FILESYSTEM_LIBRARIES)

if(NOT FREERDP_CXX_FILESYSTEM_WITHOUT_LIB AND NOT FREERDP_CXX_FILESYSTEM_LIBRARIES)
  message(FATAL_ERROR "C++ filesystem support requires an extra library, but neither stdc++fs nor c++fs works")
endif()

function(freerdp_link_cxx_filesystem TARGET VISIBILITY)
  if(FREERDP_CXX_FILESYSTEM_LIBRARIES)
    target_link_libraries(${TARGET} ${VISIBILITY} ${FREERDP_CXX_FILESYSTEM_LIBRARIES})
  endif()
endfunction()

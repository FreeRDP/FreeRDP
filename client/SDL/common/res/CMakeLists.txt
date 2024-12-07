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
set(FACTORY_SRCS "")
set(FACTORY_HDR "")
set(FACTORY_CLASSES "")

include(ConvertFileToHexArray)

macro(convert_to_bin FILE FILE_TYPE)
  get_filename_component(FILE_NAME ${FILE} NAME)
  string(REGEX REPLACE "[^a-zA-Z0-9]" "_" TARGET_NAME ${FILE_NAME})

  set(FILE_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR}/bin)

  set(FILENAME ${FILE_NAME})
  set(CLASSNAME ${TARGET_NAME})
  set(CLASSTYPE ${FILE_TYPE})

  file_to_hex_array("${FILE}" FILEDATA)

  cleaning_configure_file(resource.hpp.in ${FILE_BIN_DIR}/${TARGET_NAME}.hpp @ONLY)

  cleaning_configure_file(resource.cpp.in ${FILE_BIN_DIR}/${TARGET_NAME}.cpp @ONLY)

  list(APPEND FACTORY_HDR ${FILE_BIN_DIR}/${TARGET_NAME}.hpp)
  list(APPEND FACTORY_SRCS ${FILE_BIN_DIR}/${TARGET_NAME}.cpp)

  list(APPEND FACTORY_CLASSES ${TARGET_NAME})
endmacro()

set(SRCS sdl_resource_manager.cpp sdl_resource_manager.hpp)

set(RES_SVG_FILES ${CMAKE_SOURCE_DIR}/resources/FreeRDP_Icon.svg ${CMAKE_SOURCE_DIR}/resources/icon_info.svg
                  ${CMAKE_SOURCE_DIR}/resources/icon_warning.svg ${CMAKE_SOURCE_DIR}/resources/icon_error.svg
)

set(RES_FONT_FILES ${CMAKE_SOURCE_DIR}/resources/font/OpenSans-VariableFont_wdth,wght.ttf)

option(SDL_USE_COMPILED_RESOURCES "Compile in images/fonts" ON)

if(SDL_USE_COMPILED_RESOURCES)
  list(APPEND SRCS sdl_resource_file.cpp sdl_resource_file.hpp)

  include_directories(${CMAKE_CURRENT_SOURCE_DIR})

  if(WITH_SDL_IMAGE_DIALOGS)
    foreach(FILE ${RES_SVG_FILES})
      convert_to_bin("${FILE}" "images")
    endforeach()
  endif()

  foreach(FILE ${RES_FONT_FILES})
    convert_to_bin("${FILE}" "fonts")
  endforeach()
  add_compile_definitions(SDL_USE_COMPILED_RESOURCES)

  set(FINIT ${CMAKE_CURRENT_BINARY_DIR}/resource-init.cpp)
  list(APPEND FACTORY_SRCS ${FINIT})

  write_file(${FINIT} "#include <sdl_resource_manager.hpp>")
  foreach(HDR ${FACTORY_HDR})
    write_file(${FINIT} "#include <${HDR}>" APPEND)
  endforeach()

  write_file(${FINIT} "void SDLResourceManager::init() {" APPEND)
  foreach(CLASS ${FACTORY_CLASSES})
    write_file(${FINIT} "\t${CLASS}::init();" APPEND)
  endforeach()
  write_file(${FINIT} "}" APPEND)
else()
  option(SDL_USE_VENDOR_PRODUCT_CONFIG_DIR "Use <vendor>/<product> path for resources" OFF)
  set(SDL_RESOURCE_ROOT ${CMAKE_INSTALL_FULL_DATAROOTDIR})
  if(SDL_USE_VENDOR_PRODUCT_CONFIG_DIR)
    string(APPEND SDL_RESOURCE_ROOT "/${VENDOR}")
  endif()
  string(APPEND SDL_RESOURCE_ROOT "/${PRODUCT}")

  if(WITH_BINARY_VERSIONING)
    string(APPEND SDL_RESOURCE_ROOT "${FREERDP_VERSION_MAJOR}")
  endif()

  add_compile_definitions(SDL_RESOURCE_ROOT="${SDL_RESOURCE_ROOT}")

  if(WITH_SDL_IMAGE_DIALOGS)
    install(FILES ${RES_SVG_FILES} DESTINATION ${SDL_RESOURCE_ROOT}/images)
  endif()

  install(FILES ${RES_FONT_FILES} DESTINATION ${SDL_RESOURCE_ROOT}/fonts)
endif()

add_library(sdl-common-client-res STATIC ${RES_FILES} ${SRCS} ${FACTORY_HDR} ${FACTORY_SRCS})
set_property(TARGET sdl-common-client-res PROPERTY POSITION_INDEPENDENT_CODE ON)

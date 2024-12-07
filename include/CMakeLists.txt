# FreeRDP: A Remote Desktop Protocol Implementation
# include headers cmake build script
#
# Copyright 2011 O.S. Systems Software Ltda.
# Copyright 2011 Otavio Salvador <otavio@ossystems.com.br>
# Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

option(WITH_OPAQUE_SETTINGS "Hide rdpSettings struct definition, only allow getter/setter access" OFF)

# prepare paths for C
file(TO_NATIVE_PATH "${FREERDP_DATA_PATH}" NATIVE_FREERDP_DATA_PATH)
file(TO_NATIVE_PATH "${FREERDP_KEYMAP_PATH}" NATIVE_FREERDP_KEYMAP_PATH)
file(TO_NATIVE_PATH "${FREERDP_PLUGIN_PATH}" NATIVE_FREERDP_PLUGIN_PATH)
file(TO_NATIVE_PATH "${FREERDP_INSTALL_PREFIX}" NATIVE_FREERDP_INSTALL_PREFIX)
file(TO_NATIVE_PATH "${FREERDP_LIBRARY_PATH}" NATIVE_FREERDP_LIBRARY_PATH)
file(TO_NATIVE_PATH "${FREERDP_ADDIN_PATH}" NATIVE_FREERDP_ADDIN_PATH)
file(TO_NATIVE_PATH "${FREERDP_PROXY_PLUGINDIR}" NATIVE_FREERDP_PROXY_PLUGINDIR)

if(WIN32)
  string(REPLACE "\\" "\\\\" NATIVE_FREERDP_DATA_PATH "${NATIVE_FREERDP_DATA_PATH}")
  string(REPLACE "\\" "\\\\" NATIVE_FREERDP_KEYMAP_PATH "${NATIVE_FREERDP_KEYMAP_PATH}")
  string(REPLACE "\\" "\\\\" NATIVE_FREERDP_PLUGIN_PATH "${NATIVE_FREERDP_PLUGIN_PATH}")
  string(REPLACE "\\" "\\\\" NATIVE_FREERDP_INSTALL_PREFIX "${NATIVE_FREERDP_INSTALL_PREFIX}")
  string(REPLACE "\\" "\\\\" NATIVE_FREERDP_LIBRARY_PATH "${NATIVE_FREERDP_LIBRARY_PATH}")
  string(REPLACE "\\" "\\\\" NATIVE_FREERDP_ADDIN_PATH "${NATIVE_FREERDP_ADDIN_PATH}")
  string(REPLACE "\\" "\\\\" NATIVE_FREERDP_PROXY_PLUGINDIR "${NATIVE_FREERDP_PROXY_PLUGINDIR}")
endif()

cleaning_configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/freerdp/version.h)
cleaning_configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/config/build-config.h.in ${CMAKE_CURRENT_BINARY_DIR}/freerdp/build-config.h
)
cleaning_configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/freerdp/config.h)
cleaning_configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/config/buildflags.h.in ${CMAKE_CURRENT_BINARY_DIR}/freerdp/buildflags.h
)

file(STRINGS freerdp/settings_types_private.h SETTINGS_KEYS REGEX "ALIGN64[ \ta-zA-Z0-9]*")

set(SETTINGS_KEYS_BOOL "")
set(SETTINGS_KEYS_INT16 "")
set(SETTINGS_KEYS_UINT16 "")
set(SETTINGS_KEYS_INT32 "")
set(SETTINGS_KEYS_UINT32 "")
set(SETTINGS_KEYS_INT64 "")
set(SETTINGS_KEYS_UINT64 "")
set(SETTINGS_KEYS_STRING "")
set(SETTINGS_KEYS_POINTER "")

foreach(KEY ${SETTINGS_KEYS})
  string(STRIP "${KEY}" TRIMMED_KEY)
  string(REGEX MATCH "^SETTINGS_DEPRECATED\\(ALIGN64[ \t ]+BOOL[ \t ]+" IS_BOOL "${TRIMMED_KEY}")
  string(REGEX MATCH "^SETTINGS_DEPRECATED\\(ALIGN64[ \t ]+INT16[ \t ]+" IS_INT16 "${TRIMMED_KEY}")
  string(REGEX MATCH "^SETTINGS_DEPRECATED\\(ALIGN64[ \t ]+UINT16[ \t ]+" IS_UINT16 "${TRIMMED_KEY}")
  string(REGEX MATCH "^SETTINGS_DEPRECATED\\(ALIGN64[ \t ]+INT32[ \t ]+" IS_INT32 "${TRIMMED_KEY}")
  string(REGEX MATCH "^SETTINGS_DEPRECATED\\(ALIGN64[ \t ]+UINT32[ \t ]+" IS_UINT32 "${TRIMMED_KEY}")
  string(REGEX MATCH "^SETTINGS_DEPRECATED\\(ALIGN64[ \t ]+INT64[ \t ]+" IS_INT64 "${TRIMMED_KEY}")
  string(REGEX MATCH "^SETTINGS_DEPRECATED\\(ALIGN64[ \t ]+UINT64[ \t ]+" IS_UINT64 "${TRIMMED_KEY}")
  string(REGEX MATCH "^SETTINGS_DEPRECATED\\(ALIGN64[ \t ]+(char|CHAR)[ \t ]*\\*[ \t ]+" IS_STRING "${TRIMMED_KEY}")

  string(REGEX REPLACE ".+/\\*" "" index "${TRIMMED_KEY}")
  string(REGEX REPLACE "[ \t/\\*]" "" index "${index}")

  if(index MATCHES "^[0-9]+$")
    string(REGEX REPLACE "^SETTINGS_DEPRECATED\\(ALIGN64[ \t ]+[a-zA-Z0-9_\\*]+[ \t ]+" "" VALUE ${TRIMMED_KEY})
    string(STRIP "${VALUE}" VALUE)
    string(FIND "${VALUE}" ")" SEMICOLON)
    string(SUBSTRING "${VALUE}" 0 ${SEMICOLON} KEY_VALUE)

    if(IS_BOOL)
      set(KEY_VALUE "FreeRDP_${KEY_VALUE} = ${index}")
      list(APPEND SETTINGS_KEYS_BOOL ${KEY_VALUE})
    elseif(IS_INT16)
      set(KEY_VALUE "FreeRDP_${KEY_VALUE} = ${index}")
      list(APPEND SETTINGS_KEYS_INT16 ${KEY_VALUE})
    elseif(IS_UINT16)
      set(KEY_VALUE "FreeRDP_${KEY_VALUE} = ${index}")
      list(APPEND SETTINGS_KEYS_UINT16 ${KEY_VALUE})
    elseif(IS_INT32)
      set(KEY_VALUE "FreeRDP_${KEY_VALUE} = ${index}")
      list(APPEND SETTINGS_KEYS_INT32 ${KEY_VALUE})
    elseif(IS_UINT32)
      set(KEY_VALUE "FreeRDP_${KEY_VALUE} = ${index}")
      list(APPEND SETTINGS_KEYS_UINT32 ${KEY_VALUE})
    elseif(IS_INT64)
      set(KEY_VALUE "FreeRDP_${KEY_VALUE} = ${index}")
      list(APPEND SETTINGS_KEYS_INT64 ${KEY_VALUE})
    elseif(IS_UINT64)
      set(KEY_VALUE "FreeRDP_${KEY_VALUE} = ${index}")
      list(APPEND SETTINGS_KEYS_UINT64 ${KEY_VALUE})
    elseif(IS_STRING)
      string(SUBSTRING "${VALUE}" 0 ${SEMICOLON} KEY_VALUE)
      set(KEY_VALUE "FreeRDP_${KEY_VALUE} = ${index}")
      list(APPEND SETTINGS_KEYS_STRING ${KEY_VALUE})
    else()
      set(KEY_VALUE "FreeRDP_${KEY_VALUE} = ${index}")
      list(APPEND SETTINGS_KEYS_POINTER ${KEY_VALUE})
    endif()
  endif()
endforeach()

list(APPEND SETTINGS_KEYS_BOOL "FreeRDP_BOOL_UNUSED = -1")
list(APPEND SETTINGS_KEYS_INT16 "FreeRDP_INT16_UNUSED = -1")
list(APPEND SETTINGS_KEYS_UINT16 "FreeRDP_UINT16_UNUSED = -1")
list(APPEND SETTINGS_KEYS_INT32 "FreeRDP_INT32_UNUSED = -1")
list(APPEND SETTINGS_KEYS_UINT32 "FreeRDP_UINT32_UNUSED = -1")
list(APPEND SETTINGS_KEYS_INT64 "FreeRDP_INT64_UNUSED = -1")
list(APPEND SETTINGS_KEYS_UINT64 "FreeRDP_UINT64_UNUSED = -1")
list(APPEND SETTINGS_KEYS_STRING "FreeRDP_STRING_UNUSED = -1")
list(APPEND SETTINGS_KEYS_POINTER "FreeRDP_POINTER_UNUSED = -1")

string(REPLACE ";" ",\n\t" SETTINGS_KEYS_BOOL "${SETTINGS_KEYS_BOOL}")
string(REPLACE ";" ",\n\t" SETTINGS_KEYS_INT16 "${SETTINGS_KEYS_INT16}")
string(REPLACE ";" ",\n\t" SETTINGS_KEYS_UINT16 "${SETTINGS_KEYS_UINT16}")
string(REPLACE ";" ",\n\t" SETTINGS_KEYS_INT32 "${SETTINGS_KEYS_INT32}")
string(REPLACE ";" ",\n\t" SETTINGS_KEYS_UINT32 "${SETTINGS_KEYS_UINT32}")
string(REPLACE ";" ",\n\t" SETTINGS_KEYS_INT64 "${SETTINGS_KEYS_INT64}")
string(REPLACE ";" ",\n\t" SETTINGS_KEYS_UINT64 "${SETTINGS_KEYS_UINT64}")
string(REPLACE ";" ",\n\t" SETTINGS_KEYS_STRING "${SETTINGS_KEYS_STRING}")
string(REPLACE ";" ",\n\t" SETTINGS_KEYS_POINTER "${SETTINGS_KEYS_POINTER}")

cleaning_configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/config/settings_keys.h.in ${CMAKE_CURRENT_BINARY_DIR}/freerdp/settings_keys.h
)

file(GLOB_RECURSE PUBLIC_COMMON_HEADERS LIST_DIRECTORIES false "freerdp/*.h")
file(GLOB_RECURSE PUBLIC_COMMON_BIN_HEADERS LIST_DIRECTORIES false "${CMAKE_CURRENT_BINARY_DIR}/freerdp/*.h")
list(SORT PUBLIC_COMMON_HEADERS)

if(WITH_SERVER)
  set(PUBLIC_SERVER_HEADERS ${PUBLIC_COMMON_HEADERS})
  list(FILTER PUBLIC_SERVER_HEADERS INCLUDE REGEX ".*freerdp/server.*")

  set(PUBLIC_PROXY_HEADERS ${PUBLIC_SERVER_HEADERS})
  list(FILTER PUBLIC_SERVER_HEADERS EXCLUDE REGEX ".*freerdp/server/proxy.*")
  list(FILTER PUBLIC_PROXY_HEADERS INCLUDE REGEX ".*freerdp/server/proxy.*")
  if(WITH_SERVER)
    set_property(TARGET freerdp-server APPEND PROPERTY SOURCES ${PUBLIC_SERVER_HEADERS})
  endif()
  if(WITH_PROXY)
    set_property(TARGET freerdp-server-proxy APPEND PROPERTY SOURCES ${PUBLIC_PROXY_HEADERS})
  endif()
endif()

if(WITH_CLIENT_COMMON)
  set(PUBLIC_CLIENT_HEADERS ${PUBLIC_COMMON_HEADERS})
  list(FILTER PUBLIC_CLIENT_HEADERS INCLUDE REGEX ".*freerdp/client.*")
  set_property(TARGET freerdp-client APPEND PROPERTY SOURCES ${PUBLIC_CLIENT_HEADERS})
endif()

if(WITH_SERVER)
  list(FILTER PUBLIC_COMMON_HEADERS EXCLUDE REGEX ".*freerdp/server.*")
endif()

list(FILTER PUBLIC_COMMON_HEADERS EXCLUDE REGEX ".*freerdp/client.*")
list(APPEND PUBLIC_COMMON_HEADERS ${PUBLIC_COMMON_BIN_HEADERS})
set_property(TARGET freerdp APPEND PROPERTY SOURCES ${PUBLIC_COMMON_HEADERS})

add_library(freerdp-headers INTERFACE)
target_sources(freerdp-headers INTERFACE ${PUBLIC_COMMON_HEADERS})

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/freerdp DESTINATION ${FREERDP_INCLUDE_DIR} FILES_MATCHING PATTERN "*.h")

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/freerdp DESTINATION ${FREERDP_INCLUDE_DIR} FILES_MATCHING PATTERN "*.h")

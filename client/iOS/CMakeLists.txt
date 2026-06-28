# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP X11 Client
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

cmake_minimum_required(VERSION 3.13)

if(NOT FREERDP_DEFAULT_PROJECT_VERSION)
  set(FREERDP_DEFAULT_PROJECT_VERSION "1.0.0.0")
endif()

project(iFreeRDP VERSION ${FREERDP_DEFAULT_PROJECT_VERSION})

message("project ${PROJECT_NAME} is using version ${PROJECT_VERSION}")

list(APPEND CMAKE_MODULE_PATH ${PROJECT_CURRENT_SOURCE_DIR}/../../cmake/)
include(CommonConfigOptions)

include(WarnUnmaintained)
warn_unmaintained(${PROJECT_NAME} "-DWITH_CLIENT_IOS=OFF")

set(MODULE_NAME "iFreeRDP")
set(MODULE_PREFIX "IFREERDP_CLIENT")
set(APP_TYPE MACOSX_BUNDLE)
set(FREERDP_IOS_BUNDLE_IDENTIFIER "com.freerdp.ifreerdp" CACHE STRING "iFreeRDP iOS bundle identifier")
set(FREERDP_IOS_DEVELOPMENT_TEAM "" CACHE STRING "iFreeRDP iOS development team identifier")
set(FREERDP_IOS_CODE_SIGN_STYLE "" CACHE STRING "iFreeRDP iOS code signing style")

set(IOS_CLIENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(IOS_CLIENT_ADDITIONS_DIR ${IOS_CLIENT_DIR}/Additions)
set(IOS_CLIENT_CONTROLLERS_DIR ${IOS_CLIENT_DIR}/Controllers)
set(IOS_CLIENT_FREERDP_DIR ${IOS_CLIENT_DIR}/FreeRDP)
set(IOS_CLIENT_MISC_DIR ${IOS_CLIENT_DIR}/Misc)
set(IOS_CLIENT_MODELS_DIR ${IOS_CLIENT_DIR}/Models)
set(IOS_CLIENT_VIEWS_DIR ${IOS_CLIENT_DIR}/Views)
set(IOS_CLIENT_RESOURCES_DIR ${IOS_CLIENT_DIR}/Resources)

include_directories(SYSTEM ${IOS_CLIENT_DIR})
include_directories(SYSTEM ${IOS_CLIENT_ADDITIONS_DIR})
include_directories(SYSTEM ${IOS_CLIENT_CONTROLLERS_DIR})
include_directories(SYSTEM ${IOS_CLIENT_FREERDP_DIR})
include_directories(SYSTEM ${IOS_CLIENT_MISC_DIR})
include_directories(SYSTEM ${IOS_CLIENT_MODELS_DIR})
include_directories(SYSTEM ${IOS_CLIENT_VIEWS_DIR})
include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})

# Add sources
set(${MODULE_PREFIX}_SRCS AppDelegate.m AppDelegate.h SceneDelegate.m SceneDelegate.h main.m)

file(GLOB IOS_CLIENT_ADDITIONS_SRCS ${IOS_CLIENT_ADDITIONS_DIR}/*.m)
file(GLOB IOS_CLIENT_ADDITIONS_HDRS ${IOS_CLIENT_ADDITIONS_DIR}/*.h)

file(GLOB IOS_CLIENT_CONTROLLERS_SRCS ${IOS_CLIENT_CONTROLLERS_DIR}/*.m)
file(GLOB IOS_CLIENT_CONTROLLERS_HDRS ${IOS_CLIENT_CONTROLLERS_DIR}/*.h)

file(GLOB IOS_CLIENT_FREERDP_SRCS ${IOS_CLIENT_FREERDP_DIR}/*.m)
file(GLOB IOS_CLIENT_FREERDP_HDRS ${IOS_CLIENT_FREERDP_DIR}/*.h)

file(GLOB IOS_CLIENT_MISC_SRCS ${IOS_CLIENT_MISC_DIR}/*.m)
file(GLOB IOS_CLIENT_MISC_HDRS ${IOS_CLIENT_MISC_DIR}/*.h)

file(GLOB IOS_CLIENT_MODELS_SRCS ${IOS_CLIENT_MODELS_DIR}/*.m)
file(GLOB IOS_CLIENT_MODELS_HDRS ${IOS_CLIENT_MODELS_DIR}/*.h)

file(GLOB IOS_CLIENT_VIEWS_SRCS CONFIGURE_DEPENDS ${IOS_CLIENT_VIEWS_DIR}/*.m)
file(GLOB IOS_CLIENT_VIEWS_HDRS CONFIGURE_DEPENDS ${IOS_CLIENT_VIEWS_DIR}/*.h)

# add resources
file(GLOB IOS_CLIENT_RESOURCES_XIBS ${IOS_CLIENT_RESOURCES_DIR}/*.xib)
file(GLOB IOS_CLIENT_RESOURCES_PNGS ${IOS_CLIENT_RESOURCES_DIR}/*.png)

# Specify source grouping
source_group(Additions FILES ${IOS_CLIENT_ADDITIONS_SRCS} ${IOS_CLIENT_ADDITIONS_HDRS})
source_group(Controllers FILES ${IOS_CLIENT_CONTROLLERS_SRCS} ${IOS_CLIENT_CONTROLLERS_HDRS})
source_group(FreeRDP FILES ${IOS_CLIENT_FREERDP_SRCS} ${IOS_CLIENT_FREERDP_HDRS})
source_group(Misc FILES ${IOS_CLIENT_MISC_SRCS} ${IOS_CLIENT_MISC_HDRS})
source_group(Models FILES ${IOS_CLIENT_MODELS_SRCS} ${IOS_CLIENT_MODELS_HDRS})
source_group(Views FILES ${IOS_CLIENT_RESOURCES_XIBS} ${IOS_CLIENT_VIEWS_SRCS} ${IOS_CLIENT_VIEWS_HDRS})
source_group(
  Resources FILES ${IOS_CLIENT_RESOURCES_PNGS} ${IOS_CLIENT_RESOURCES_DIR}/about_page
                  ${IOS_CLIENT_RESOURCES_DIR}/en.lproj
)

# import libraries
find_library(FOUNDATION_FRAMEWORK Foundation)
find_library(COREGRAPHICS_FRAMEWORK CoreGraphics)
find_library(SECURITY_FRAMEWORK Security)
find_library(UIKIT_FRAMEWORK UIKit)
find_library(SYSTEMCONFIGURATION_FRAMEWORK SystemConfiguration)
find_library(METAL_FRAMEWORK Metal)
find_library(QUARTZCORE_FRAMEWORK QuartzCore)
find_library(WEBKIT_FRAMEWORK WebKit)
find_library(GAMECONTROLLER_FRAMEWORK GameController)

mark_as_advanced(
  FOUNDATION_FRAMEWORK
  COREGRAPHICS_FRAMEWORK
  METAL_FRAMEWORK
  QUARTZCORE_FRAMEWORK
  SECURITY_FRAMEWORK
  UIKIT_FRAMEWORK
  SYSTEMCONFIGURATION_FRAMEWORK
  WEBKIT_FRAMEWORK
  GAMECONTROLLER_FRAMEWORK
)
set(EXTRA_LIBS
    ${FOUNDATION_FRAMEWORK}
    ${COREGRAPHICS_FRAMEWORK}
    ${METAL_FRAMEWORK}
    ${QUARTZCORE_FRAMEWORK}
    ${SECURITY_FRAMEWORK}
    ${UIKIT_FRAMEWORK}
    ${SYSTEMCONFIGURATION_FRAMEWORK}
    ${WEBKIT_FRAMEWORK}
    ${GAMECONTROLLER_FRAMEWORK}
)

set(${MODULE_NAME}_RESOURCES ${IOS_CLIENT_RESOURCES_XIBS})
set(${MODULE_NAME}_RESOURCES ${${MODULE_NAME}_RESOURCES} ${IOS_CLIENT_RESOURCES_PNGS})
set(${MODULE_NAME}_RESOURCES ${${MODULE_NAME}_RESOURCES} ${IOS_CLIENT_RESOURCES_DIR}/about_page
                             ${IOS_CLIENT_RESOURCES_DIR}/en.lproj
)
set(${MODULE_NAME}_RESOURCES ${${MODULE_NAME}_RESOURCES} ${IOS_CLIENT_DIR}/Defaults.plist)

add_executable(
  ${MODULE_NAME}
  ${APP_TYPE}
  ${${MODULE_PREFIX}_SRCS}
  ${IOS_CLIENT_ADDITIONS_SRCS}
  ${IOS_CLIENT_ADDITIONS_HDRS}
  ${IOS_CLIENT_CONTROLLERS_SRCS}
  ${IOS_CLIENT_CONTROLLERS_HDRS}
  ${IOS_CLIENT_FREERDP_SRCS}
  ${IOS_CLIENT_FREERDP_HDRS}
  ${IOS_CLIENT_MISC_SRCS}
  ${IOS_CLIENT_MISC_HDRS}
  ${IOS_CLIENT_MODELS_SRCS}
  ${IOS_CLIENT_MODELS_HDRS}
  ${IOS_CLIENT_VIEWS_SRCS}
  ${IOS_CLIENT_VIEWS_HDRS}
  ${${MODULE_NAME}_RESOURCES}
)

if(WITH_BINARY_VERSIONING)
  set_target_properties(${MODULE_NAME} PROPERTIES OUTPUT_NAME "${MODULE_NAME}${PROJECT_VERSION_MAJOR}")
endif()
set_target_properties(${MODULE_NAME} PROPERTIES RESOURCE "${${MODULE_NAME}_RESOURCES}")

set(EXECUTABLE_NAME "\${EXECUTABLE_NAME}")

set_target_properties(${MODULE_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${IOS_CLIENT_DIR}/iFreeRDP.plist)
set_target_properties(${MODULE_NAME} PROPERTIES XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "${IOS_TARGET_SDK}")
set_target_properties(${MODULE_NAME} PROPERTIES XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "gnu++0x")
set_target_properties(${MODULE_NAME} PROPERTIES XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
set_target_properties(${MODULE_NAME} PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC NO)
# for Ninja build (CI Server) (turn off ARC)
target_compile_options(${MODULE_NAME} PRIVATE -fno-objc-arc)
set_target_properties(${MODULE_NAME} PROPERTIES XCODE_ATTRIBUTE_GCC_C_LANGUAGE_STANDARD "gnu99")
set_target_properties(
  ${MODULE_NAME} PROPERTIES XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${FREERDP_IOS_BUNDLE_IDENTIFIER}"
)
set_target_properties(${MODULE_NAME} PROPERTIES XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2")
set_target_properties(${MODULE_NAME} PROPERTIES XCODE_ATTRIBUTE_COMBINE_HIDPI_IMAGES "NO")
set_target_properties(${MODULE_NAME} PROPERTIES XCODE_ATTRIBUTE_SKIP_INSTALL NO)
set_target_properties(${MODULE_NAME} PROPERTIES XCODE_ATTRIBUTE_INSTALL_PATH "/Applications")
set_target_properties(${MODULE_NAME} PROPERTIES XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS YES)
set_target_properties(${MODULE_NAME} PROPERTIES XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/Frameworks")
if(FREERDP_IOS_DEVELOPMENT_TEAM)
  set_target_properties(${MODULE_NAME} PROPERTIES XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${FREERDP_IOS_DEVELOPMENT_TEAM}")
endif()
if(FREERDP_IOS_CODE_SIGN_STYLE)
  set_target_properties(${MODULE_NAME} PROPERTIES XCODE_ATTRIBUTE_CODE_SIGN_STYLE "${FREERDP_IOS_CODE_SIGN_STYLE}")
endif()
if(CODE_SIGN_IDENTITY)
  set_target_properties(${MODULE_NAME} PROPERTIES XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${CODE_SIGN_IDENTITY}")
endif()

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} ${EXTRA_LIBS})

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} freerdp-client)

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} winpr freerdp)

if(OPENSSL_FOUND)
  set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
endif()

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

if(IOS)
  if(BUILD_SHARED_LIBS)
    foreach(_ios_runtime_target freerdp-client freerdp winpr)
      if(TARGET ${_ios_runtime_target})
        set_target_properties(
          ${_ios_runtime_target} PROPERTIES BUILD_WITH_INSTALL_NAME_DIR YES INSTALL_NAME_DIR "@rpath"
        )
        if(FREERDP_IOS_DEVELOPMENT_TEAM)
          set_target_properties(
            ${_ios_runtime_target} PROPERTIES XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${FREERDP_IOS_DEVELOPMENT_TEAM}"
          )
        endif()
        if(FREERDP_IOS_CODE_SIGN_STYLE)
          set_target_properties(
            ${_ios_runtime_target} PROPERTIES XCODE_ATTRIBUTE_CODE_SIGN_STYLE "${FREERDP_IOS_CODE_SIGN_STYLE}"
          )
        endif()
      endif()
    endforeach()

    set(_ios_bundle_frameworks_dir "$<TARGET_BUNDLE_CONTENT_DIR:${MODULE_NAME}>/Frameworks")
    add_custom_command(
      TARGET ${MODULE_NAME}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "${_ios_bundle_frameworks_dir}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:freerdp-client>"
              "${_ios_bundle_frameworks_dir}/$<TARGET_FILE_NAME:freerdp-client>"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:freerdp-client>"
              "${_ios_bundle_frameworks_dir}/$<TARGET_SONAME_FILE_NAME:freerdp-client>"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:freerdp>"
              "${_ios_bundle_frameworks_dir}/$<TARGET_FILE_NAME:freerdp>"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:freerdp>"
              "${_ios_bundle_frameworks_dir}/$<TARGET_SONAME_FILE_NAME:freerdp>"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:winpr>"
              "${_ios_bundle_frameworks_dir}/$<TARGET_FILE_NAME:winpr>"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:winpr>"
              "${_ios_bundle_frameworks_dir}/$<TARGET_SONAME_FILE_NAME:winpr>"
      COMMENT "Embedding FreeRDP iOS runtime libraries"
    )
  endif()

  # Embed + code-sign the dynamic FFmpeg framework with Xcode's native "Embed
  # Frameworks" build phase (no custom codesign script). The FreeRDP runtime
  # dylibs copied above are signed by their own Xcode targets (the
  # XCODE_ATTRIBUTE_DEVELOPMENT_TEAM set on them above).
  if(AVCODEC_LIBRARIES MATCHES "\\.framework/")
    get_filename_component(_ios_ffmpeg_framework "${AVCODEC_LIBRARIES}" DIRECTORY)
    set_target_properties(
      ${MODULE_NAME} PROPERTIES XCODE_EMBED_FRAMEWORKS "${_ios_ffmpeg_framework}"
                                XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY ON
    )
  endif()
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Client/iOS")

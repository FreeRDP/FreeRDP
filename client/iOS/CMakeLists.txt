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

if(POLICY CMP0114)
  cmake_policy(SET CMP0114 NEW)
endif()

if(NOT FREERDP_DEFAULT_PROJECT_VERSION)
  set(FREERDP_DEFAULT_PROJECT_VERSION "1.0.0.0")
endif()

project(iFreeRDP VERSION ${FREERDP_DEFAULT_PROJECT_VERSION})

message("project ${PROJECT_NAME} is using version ${PROJECT_VERSION}")

get_filename_component(FREERDP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

list(APPEND CMAKE_MODULE_PATH ${FREERDP_SOURCE_DIR}/cmake/)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/)
include(CommonConfigOptions)
include(ProjectCStandard)
include(ProjectCXXStandard)

set(CMAKE_C_STANDARD 23 CACHE STRING "project default" FORCE)

# Cross compilation settings:
#
# * Set CMAKE_SYSROOT as main search base
# * Disable looking for includes/libraries/packages in host system directories
set(CMAKE_SYSROOT ${CMAKE_OSX_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
message("sysroot: ${CMAKE_SYSROOT}")
message("target:  ${CMAKE_OSX_DEPLOYMENT_TARGET}")

include(GNUInstallDirs)
#set(CMAKE_INSTALL_LIBDIR Frameworks)
#set(CMAKE_INSTALL_BINDIR MacOS)

set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "project default")

set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++" CACHE STRING "project default")
set(CMAKE_XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2" CACHE STRING "project default")
set(CMAKE_XCODE_ATTRIBUTE_COMBINE_HIDPI_IMAGES "NO" CACHE STRING "project default")
set(CMAKE_XCODE_ATTRIBUTE_SKIP_INSTALL NO CACHE BOOL "project default")
set(CMAKE_INSTALL_PATH "/Applications" CACHE STRING "project default")
set(CMAKE_XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS YES CACHE BOOL "project default")
set(CMAKE_XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/Frameworks" CACHE STRING "project default")
set(CMAKE_XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.freerdp.ifreerdp" CACHE STRING "project default")

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

# Dependencies
include(ExternalDeps)

# Add sources
set(SRCS
    Additions/OrderedDictionary.h
    Additions/OrderedDictionary.m
    Additions/Toast+UIView.h
    Additions/Toast+UIView.m
    Additions/TSXAdditions.h
    Additions/TSXAdditions.m
    AppDelegate.h
    AppDelegate.m
    Controllers/AboutController.h
    Controllers/AboutController.m
    Controllers/AdvancedBookmarkEditorController.h
    Controllers/AdvancedBookmarkEditorController.m
    Controllers/AppSettingsController.h
    Controllers/AppSettingsController.m
    Controllers/BookmarkEditorController.h
    Controllers/BookmarkEditorController.m
    Controllers/BookmarkGatewaySettingsController.h
    Controllers/BookmarkGatewaySettingsController.m
    Controllers/BookmarkListController.h
    Controllers/BookmarkListController.m
    Controllers/CredentialsEditorController.h
    Controllers/CredentialsEditorController.m
    Controllers/CredentialsInputController.h
    Controllers/CredentialsInputController.m
    Controllers/EditorBaseController.h
    Controllers/EditorBaseController.m
    Controllers/EditorSelectionController.h
    Controllers/EditorSelectionController.m
    Controllers/EncryptionController.h
    Controllers/EncryptionController.m
    Controllers/MainTabBarController.h
    Controllers/MainTabBarController.m
    Controllers/PerformanceEditorController.h
    Controllers/PerformanceEditorController.m
    Controllers/RDPSessionViewController.h
    Controllers/RDPSessionViewController.m
    Controllers/ScreenSelectionController.h
    Controllers/ScreenSelectionController.m
    Controllers/VerifyCertificateController.h
    Controllers/VerifyCertificateController.m
    FreeRDP/ios_cliprdr.h
    FreeRDP/ios_cliprdr.m
    FreeRDP/ios_freerdp_events.h
    FreeRDP/ios_freerdp_events.m
    FreeRDP/ios_freerdp.h
    FreeRDP/ios_freerdp.m
    FreeRDP/ios_freerdp_ui.h
    FreeRDP/ios_freerdp_ui.m
    main.m
    Misc/Reachability.h
    Misc/Reachability.m
    Misc/SFHFKeychainUtils.h
    Misc/SFHFKeychainUtils.m
    Misc/TSXTypes.h
    Misc/Utils.h
    Misc/Utils.m
    Models/Bookmark.h
    Models/Bookmark.m
    Models/ConnectionParams.h
    Models/ConnectionParams.m
    Models/Encryptor.h
    Models/Encryptor.m
    Models/GlobalDefaults.h
    Models/GlobalDefaults.m
    Models/RDPKeyboard.h
    Models/RDPKeyboard.m
    Models/RDPSession.h
    Models/RDPSession.m
    SceneDelegate.h
    SceneDelegate.m
    Views/AdvancedKeyboardView.h
    Views/AdvancedKeyboardView.m
    Views/BlockAlertView.h
    Views/BlockAlertView.m
    Views/BlockBackground.h
    Views/BlockBackground.m
    Views/BlockUI.h
    Views/BookmarkTableCell.h
    Views/BookmarkTableCell.m
    Views/EditButtonTableViewCell.h
    Views/EditButtonTableViewCell.m
    Views/EditFlagTableViewCell.h
    Views/EditFlagTableViewCell.m
    Views/EditSecretTextTableViewCell.h
    Views/EditSecretTextTableViewCell.m
    Views/EditSelectionTableViewCell.h
    Views/EditSelectionTableViewCell.m
    Views/EditSubEditTableViewCell.h
    Views/EditSubEditTableViewCell.m
    Views/EditTextTableViewCell.h
    Views/EditTextTableViewCell.m
    Views/RDPCursor.h
    Views/RDPCursor.m
    Views/RDPSessionToolbar.h
    Views/RDPSessionToolbar.m
    Views/RDPSessionView.h
    Views/RDPSessionView.m
    Views/SessionTableCell.h
    Views/SessionTableCell.m
)

# add resources
set(IOS_CLIENT_RESOURCES_XIBS
    Resources/BookmarkListView.xib
    Resources/BookmarkTableViewCell.xib
    Resources/CredentialsInputView.xib
    Resources/EditButtonTableViewCell.xib
    Resources/EditFlagTableViewCell.xib
    Resources/EditSecretTextTableViewCell.xib
    Resources/EditSelectionTableViewCell.xib
    Resources/EditSubEditTableViewCell.xib
    Resources/EditTextTableViewCell.xib
    Resources/MainWindow.xib
    Resources/RDPConnectingView.xib
    Resources/RDPSessionView.xib
    Resources/SessionTableViewCell.xib
    Resources/VerifyCertificateView.xib
)

set(IOS_CLIENT_RESOURCES_PNGS
    Resources/about_page/background_transparent.png
    Resources/about_page/FreeRDP_Logo.png
    Resources/alert-black-button@2x.png
    Resources/alert-black-button.png
    Resources/alert-gray-button@2x.png
    Resources/alert-gray-button.png
    Resources/alert-red-button@2x.png
    Resources/alert-red-button.png
    Resources/alert-window@2x.png
    Resources/alert-window-landscape@2x.png
    Resources/alert-window-landscape.png
    Resources/alert-window.png
    Resources/cancel_button_background.png
    Resources/Default@2x.png
    Resources/Default-568h@2x.png
    Resources/Default-Landscape@2x~ipad.png
    Resources/Default-Landscape~ipad.png
    Resources/Default.png
    Resources/Default-Portrait@2x~ipad.png
    Resources/Default-Portrait~ipad.png
    Resources/Icon@2x.png
    Resources/Icon-72@2x.png
    Resources/Icon-72.png
    Resources/icon_accessory_star_off.png
    Resources/icon_accessory_star_on.png
    Resources/icon_key_arrow_down.png
    Resources/icon_key_arrow_left.png
    Resources/icon_key_arrow_right.png
    Resources/icon_key_arrows.png
    Resources/icon_key_arrow_up.png
    Resources/icon_key_backspace.png
    Resources/icon_key_menu.png
    Resources/icon_key_return.png
    Resources/icon_key_win.png
    Resources/Icon.png
    Resources/keyboard_button_background.png
    Resources/tabbar_icon_about.png
    Resources/tabbar_icon_settings.png
    Resources/toolbar_icon_disconnect.png
    Resources/toolbar_icon_extkeyboad.png
    Resources/toolbar_icon_home.png
    Resources/toolbar_icon_keyboard.png
    Resources/toolbar_icon_touchpointer.png
    Resources/toolbar_icon_win.png
    Resources/touch_pointer_active.png
    Resources/touch_pointer_default.png
    Resources/touch_pointer_extkeyboard.png
    Resources/touch_pointer_keyboard.png
    Resources/touch_pointer_lclick.png
    Resources/touch_pointer_rclick.png
    Resources/touch_pointer_reset.png
    Resources/touch_pointer_scroll.png
)

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
find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
find_library(COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
find_library(SECURITY_FRAMEWORK Security REQUIRED)
find_library(UIKIT_FRAMEWORK UIKit REQUIRED)
find_library(SYSTEMCONFIGURATION_FRAMEWORK SystemConfiguration REQUIRED)
find_library(METAL_FRAMEWORK Metal REQUIRED)
find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
find_library(WEBKIT_FRAMEWORK WebKit REQUIRED)
find_library(GAMECONTROLLER_FRAMEWORK GameController REQUIRED)

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

set(RESOURCES ${IOS_CLIENT_RESOURCES_XIBS} ${IOS_CLIENT_RESOURCES_PNGS} ${IOS_CLIENT_RESOURCES_DIR}/about_page
              ${IOS_CLIENT_RESOURCES_DIR}/en.lproj ${IOS_CLIENT_DIR}/Defaults.plist
)

add_executable(
  ${PROJECT_NAME} MACOSX_BUNDLE
  ${SRCS}
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
  ${RESOURCES}
)

add_dependencies(${PROJECT_NAME} freerdp)

set_target_properties(
  ${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${IOS_CLIENT_DIR}/iFreeRDP.plist FOLDER "Client/iOS"
                             RESOURCE "${RESOURCES}"
)

# for Ninja build (CI Server) (turn off ARC)
target_compile_options(${PROJECT_NAME} PRIVATE -fno-objc-arc)

target_link_libraries(${PROJECT_NAME} ${EXTRA_LIBS} freerdp3-lib freerdp-client3-lib winpr3-lib)

# Link by name; found via the deps search path from ExternalDeps.cmake.
if(WITH_OPENSSL)
  target_link_libraries(${PROJECT_NAME} ssl crypto)
endif()

set(_ios_embed_frameworks "")
if(TARGET freerdp3-lib)
  get_target_property(_ios_freerdp_runtime_location freerdp3-lib IMPORTED_LOCATION)
  list(APPEND _ios_embed_frameworks "${_ios_freerdp_runtime_location}")
endif()
if(TARGET freerdp-client3-lib)
  get_target_property(_ios_freerdp_client_runtime_location freerdp-client3-lib IMPORTED_LOCATION)
  list(APPEND _ios_embed_frameworks "${_ios_freerdp_client_runtime_location}")
endif()
if(TARGET winpr3-lib)
  get_target_property(_ios_winpr_runtime_location winpr3-lib IMPORTED_LOCATION)
  list(APPEND _ios_embed_frameworks "${_ios_winpr_runtime_location}")
endif()

if(AVCODEC_LIBRARIES MATCHES "\\.framework/")
  get_filename_component(_ios_ffmpeg_framework "${AVCODEC_LIBRARIES}" DIRECTORY)
  list(APPEND _ios_embed_frameworks "${_ios_ffmpeg_framework}")
endif()

if(_ios_embed_frameworks)
  set_target_properties(
    ${PROJECT_NAME} PROPERTIES XCODE_EMBED_FRAMEWORKS "${_ios_embed_frameworks}"
                               XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY ON
  )
endif()

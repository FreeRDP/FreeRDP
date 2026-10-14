# Use the target platform, including when cross-compiling from a non-Apple host.
set(_videotoolbox_platform_supported OFF)
set(_videotoolbox_minimum_version "")
if(APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(_videotoolbox_platform_supported ON)
  # CMSampleBufferCreateReady and kVTDecompressionPropertyKey_RealTime require 10.10.
  set(_videotoolbox_minimum_version "10.10")
elseif(APPLE AND CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(_videotoolbox_platform_supported ON)
  set(_videotoolbox_minimum_version "8.0")
endif()

set(_videotoolbox_default ${_videotoolbox_platform_supported})
if(_videotoolbox_platform_supported AND CMAKE_OSX_DEPLOYMENT_TARGET)
  if(CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS _videotoolbox_minimum_version)
    set(_videotoolbox_default OFF)
  endif()
endif()

option(WITH_APPLE_VIDEOTOOLBOX "Use Apple VideoToolbox directly for H.264 decoding" ${_videotoolbox_default})
if(WITH_APPLE_VIDEOTOOLBOX)
  if(NOT _videotoolbox_platform_supported)
    message(FATAL_ERROR "WITH_APPLE_VIDEOTOOLBOX requires a macOS or iOS target")
  elseif(NOT _videotoolbox_default)
    message(
      FATAL_ERROR "WITH_APPLE_VIDEOTOOLBOX requires ${CMAKE_SYSTEM_NAME} ${_videotoolbox_minimum_version} or later"
    )
  endif()
endif()

# codec

option(WITH_CURSOR_DUMP "Dump mouse cursor data to binary directory" OFF)
set(YUV_TILE_SIZE "512" CACHE STRING "YUV decoder per thread tile width/height")
add_compile_definitions(YUV_TILE_SIZE=${YUV_TILE_SIZE})

if(WITH_CURSOR_DUMP)
  add_compile_definitions(WITH_CURSOR_DUMP)
  add_compile_definitions(CURSOR_DUMP_DIR="${CMAKE_CURRENT_BINARY_DIR}")
endif()

set(CODEC_SRCS
    bulk.c
    bulk.h
    dsp.c
    color.c
    color.h
    audio.c
    planar.c
    bitmap.c
    interleaved.c
    progressive.c
    rfx_bitstream.h
    rfx_constants.h
    rfx_decode.c
    rfx_decode.h
    rfx_differential.h
    rfx_dwt.c
    rfx_dwt.h
    rfx_encode.c
    rfx_encode.h
    rfx_quantization.c
    rfx_quantization.h
    rfx_rlgr.c
    rfx_rlgr.h
    rfx_types.h
    rfx.c
    region.c
    nsc.c
    nsc_encode.c
    nsc_encode.h
    nsc_types.h
    ncrush.c
    xcrush.c
    mppc.c
    zgfx.c
    clear.c
    jpeg.c
    h264.c
    yuv.c
    video.c
)

if(WITH_GFX_AV1)
  include(WarnExperimental)
  warn_experimental("AV1 codec support for GFX channel" "-DWITH_GFX_AV1=OFF")
  list(APPEND CODEC_SRCS av1.c)
  if(EMSCRIPTEN)
    # in the browser, decode AV1 with WebCodecs (dav1d stays as the fallback)
    option(WITH_AV1_WEBCODECS "Decode AV1 with the browser's WebCodecs API" ON)
    if(WITH_AV1_WEBCODECS)
      add_compile_definitions(WITH_AV1_WEBCODECS)
      list(APPEND CODEC_SRCS av1_webcodecs.c av1_webcodecs.h)
      # The JavaScript side, for the executable's link: a link item, so that it also reaches the
      # executable through static libraries (link options of their private dependencies don't).
      freerdp_library_add_public(
        "--js-library=$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${FREERDP_PLUGIN_PATH}>/av1_webcodecs.js"
      )
      # so that editing the JavaScript recompiles this file, and so relinks the executables
      set_source_files_properties(
        av1_webcodecs.c PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/av1_webcodecs.js
      )
      if(NOT BUILD_SHARED_LIBS)
        install(FILES av1_webcodecs.js DESTINATION ${FREERDP_PLUGIN_PATH})
      endif()
    endif()
  endif()
endif()

if(WITH_GFX_AV2)
  include(WarnExperimental)
  warn_experimental("AV2 codec support for GFX channel" "-DWITH_GFX_AV2=OFF")
  list(APPEND CODEC_SRCS av2.c)
endif()

set(CODEC_SSE3_SRCS sse/rfx_sse2.c sse/rfx_sse2.h sse/nsc_sse2.c sse/nsc_sse2.h)

set(CODEC_NEON_SRCS neon/rfx_neon.c neon/rfx_neon.h neon/nsc_neon.c neon/nsc_neon.h)

# Append initializers
set(CODEC_LIBS "")
list(APPEND CODEC_SRCS ${CODEC_SSE3_SRCS})
list(APPEND CODEC_SRCS ${CODEC_NEON_SRCS})

include(CompilerDetect)
include(DetectIntrinsicSupport)

if(WITH_SIMD)
  set_simd_source_file_properties("sse3" ${CODEC_SSE3_SRCS})
  set_simd_source_file_properties("neon" ${CODEC_NEON_SRCS})
endif()

if(WITH_DSP_FFMPEG)
  set(CODEC_SRCS ${CODEC_SRCS} dsp_ffmpeg.c dsp_ffmpeg.h)
  include_directories(SYSTEM ${FFMPEG_INCLUDE_DIRS})
  list(APPEND CODEC_LIBS ${FFMPEG_LIBRARIES})
  freerdp_pc_add_requires_private("libavfilter;libavformat;libavcodec;libavutil;libswresample")
endif(WITH_DSP_FFMPEG)

if(WITH_SOXR)
  list(APPEND CODEC_LIBS ${SOXR_LIBRARIES})
  include_directories(SYSTEM ${SOXR_INCLUDE_DIR})
  freerdp_pc_add_requires_private("soxr")
endif(WITH_SOXR)

if(GSM_FOUND)
  list(APPEND CODEC_LIBS ${GSM_LIBRARIES})
  include_directories(SYSTEM ${GSM_INCLUDE_DIRS})
  freerdp_pc_add_library_private("gsm")
endif()

if(LAME_FOUND)
  list(APPEND CODEC_LIBS ${LAME_LIBRARIES})
  include_directories(SYSTEM ${LAME_INCLUDE_DIRS})
  freerdp_pc_add_requires_private("lame")
endif()

if(WITH_FDK_AAC)
  list(APPEND CODEC_SRCS dsp_fdk_impl.c dsp_fdk_impl.h dsp_fdk_aac.c dsp_fdk_aac.h)
  freerdp_pc_add_requires_private("fdk-aac")
endif()

if(FAAD2_FOUND)
  list(APPEND CODEC_LIBS ${FAAD2_LIBRARIES})
  include_directories(SYSTEM ${FAAD2_INCLUDE_DIRS})
  freerdp_pc_add_requires_private("faad2")
endif()

if(FAAC_FOUND)
  list(APPEND CODEC_LIBS ${FAAC_LIBRARIES})
  include_directories(SYSTEM ${FAAC_INCLUDE_DIRS})
  freerdp_pc_add_requires_private("faac")
endif()

if(WITH_AOM AND AOM_FOUND)
  add_compile_definitions(WITH_LIBAOM)
  list(APPEND CODEC_LIBS aom)
  freerdp_pc_add_requires_private("aom")
endif()

if(WITH_DAV1D AND DAV1D_FOUND)
  add_compile_definitions(WITH_DAV1D)
  list(APPEND CODEC_LIBS dav1d)
  freerdp_pc_add_requires_private("dav1d")
endif()

if(WITH_GFX_AV1)
  if(WITH_DAV1D AND DAV1D_FOUND)
    message(STATUS "AV1 decoder: dav1d")
  elseif(WITH_AOM AND AOM_FOUND)
    message(STATUS "AV1 decoder: libaom")
  else()
    message(STATUS "AV1 decoder: none (need -DWITH_DAV1D=ON or -DWITH_AOM=ON)")
  endif()
endif()

if(WITH_GFX_AV2)
  if(WITH_AVM AND AVM_FOUND)
    add_compile_definitions(WITH_LIBAVM)
    list(APPEND CODEC_LIBS avm)
    include_directories(SYSTEM ${AVM_INCLUDE_DIRS})
    freerdp_pc_add_requires_private("avm")
    # a libavm built with CONFIG_AV2_ENCODER=0 decodes only (a client, a browser): it has
    # neither the encoder's headers nor its functions, and AV2 encoding is then unavailable
    option(WITH_AVM_ENCODER "libavm has its encoder (OFF for a decoder-only libavm)" ON)
    if(WITH_AVM_ENCODER)
      add_compile_definitions(WITH_LIBAVM_ENCODER)
    endif()
  endif()

  # libavm, the reference, decodes when present: dav2d is the decoder without it
  if(WITH_AVM AND AVM_FOUND)
    message(STATUS "AV2 decoder: libavm")
  elseif(WITH_DAV2D AND DAV2D_FOUND)
    add_compile_definitions(WITH_DAV2D)
    list(APPEND CODEC_LIBS dav2d)
    include_directories(SYSTEM ${DAV2D_INCLUDE_DIRS})
    freerdp_pc_add_requires_private("dav2d")
    message(STATUS "AV2 decoder: dav2d")
  else()
    message(STATUS "AV2 decoder: none (need -DWITH_DAV2D=ON or -DWITH_AVM=ON)")
  endif()
endif()

if(WITH_YUV AND YUV_FOUND)
  add_compile_definitions(WITH_LIBYUV)
  list(APPEND CODEC_LIBS yuv)
  freerdp_pc_add_requires_private("yuv")
endif()

if(WITH_APPLE_VIDEOTOOLBOX)
  list(APPEND CODEC_SRCS h264_videotoolbox.c)
  find_library(VIDEOTOOLBOX_FRAMEWORK VideoToolbox REQUIRED)
  find_library(COREMEDIA_FRAMEWORK CoreMedia REQUIRED)
  find_library(COREVIDEO_FRAMEWORK CoreVideo REQUIRED)
  find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
  list(APPEND CODEC_LIBS ${VIDEOTOOLBOX_FRAMEWORK} ${COREMEDIA_FRAMEWORK} ${COREVIDEO_FRAMEWORK}
       ${COREFOUNDATION_FRAMEWORK}
  )
endif()

if(WITH_H264_WEBCODECS)
  list(APPEND CODEC_SRCS h264_webcodecs.c)
  # The JavaScript side, for the executable's link: a link item, so that it also reaches the
  # executable through static libraries (link options of their private dependencies don't).
  freerdp_library_add_public(
    "--js-library=$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${FREERDP_PLUGIN_PATH}>/h264_webcodecs.js"
  )
  # so that editing the JavaScript recompiles this file, and so relinks the executables
  set_source_files_properties(h264_webcodecs.c PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/h264_webcodecs.js)
  if(NOT BUILD_SHARED_LIBS)
    install(FILES h264_webcodecs.js DESTINATION ${FREERDP_PLUGIN_PATH})
  endif()
endif()

if(WITH_OPENH264)
  set(CODEC_SRCS ${CODEC_SRCS} h264_openh264.c)
  include_directories(SYSTEM ${OPENH264_INCLUDE_DIR})
  if(NOT WITH_OPENH264_LOADING)
    list(APPEND CODEC_LIBS ${OPENH264_LIBRARIES})
    freerdp_pc_add_requires_private("openh264")
  endif(NOT WITH_OPENH264_LOADING)
endif()

if(WITH_SWSCALE)
  if(WITH_SWSCALE_LOADING)
    set(CODEC_SRCS ${CODEC_SRCS} image_ffmpeg.c image_ffmpeg.h)
  else()
    include_directories(SYSTEM ${SWSCALE_INCLUDE_DIRS})
  endif(WITH_SWSCALE_LOADING)
endif()

if(WITH_DSP_EXPERIMENTAL)
  include(WarnExperimental)
  warn_experimental("Experimental encoders/decoders for audio channels" "-DWITH_DSP_EXPERIMENTAL=OFF")
endif()

if(WITH_VIDEO_FFMPEG)
  if(WITH_FFMPEG_HWACCEL)
    include(WarnExperimental)
    warn_experimental("Hardware encoder/decoder support for FFMPEG" "-DWITH_FFMPEG_HWACCEL=OFF")
  endif()

  set(CODEC_SRCS ${CODEC_SRCS} h264_ffmpeg.c)
  include_directories(SYSTEM ${FFMPEG_INCLUDE_DIRS})
  list(APPEND CODEC_LIBS ${FFMPEG_LIBRARIES})
  freerdp_pc_add_requires_private("libavfilter;libavformat;libavcodec;libavutil")
endif()

if(WIN32 AND WITH_MEDIA_FOUNDATION)
  include(WarnExperimental)
  warn_experimental("Experimental encoders/decoders for H264" "-DWITH_MEDIA_FOUNDATION=OFF")
  set(CODEC_SRCS ${CODEC_SRCS} h264_mf.c)
endif()

if(ANDROID AND WITH_MEDIACODEC)
  include(WarnExperimental)
  warn_experimental("Experimental encoders/decoders for H264 and AAC" "-DWITH_MEDIACODEC=OFF")
  list(APPEND CODEC_SRCS h264_mediacodec.c dsp_mediacodec.c)

  find_library(MEDIACODEC mediandk REQUIRED)
  list(APPEND CODEC_LIBS ${MEDIACODEC})
endif()

add_library(freerdp-codecs OBJECT ${CODEC_SRCS})
freerdp_library_add(${CODEC_LIBS})
freerdp_object_library_add(freerdp-codecs)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()

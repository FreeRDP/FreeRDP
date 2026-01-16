# codec

option(WITH_CURSOR_DUMP "Dump mouse cursor data to binary directory" OFF)
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
)

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
    set(CODEC_SRCS ${CODEC_SRCS} swscale.c swscale.h)
  else()
    include_directories(SYSTEM ${SWSCALE_INCLUDE_DIRS})
  endif(WITH_SWSCALE_LOADING)
endif()

if(WITH_VIDEO_FFMPEG)
  set(CODEC_SRCS ${CODEC_SRCS} h264_ffmpeg.c)
  include_directories(SYSTEM ${FFMPEG_INCLUDE_DIRS})
  list(APPEND CODEC_LIBS ${FFMPEG_LIBRARIES})
  freerdp_pc_add_requires_private("libavfilter;libavformat;libavcodec;libavutil")
endif()

if(WIN32 AND WITH_MEDIA_FOUNDATION)
  set(CODEC_SRCS ${CODEC_SRCS} h264_mf.c)
endif()

if(ANDROID AND WITH_MEDIACODEC)
  list(APPEND CODEC_SRCS h264_mediacodec.c)

  find_library(MEDIACODEC mediandk REQUIRED)
  list(APPEND CODEC_LIBS ${MEDIACODEC})
endif()

add_library(freerdp-codecs OBJECT ${CODEC_SRCS})
freerdp_library_add(${CODEC_LIBS})
freerdp_object_library_add(freerdp-codecs)

if(BUILD_TESTING_INTERNAL OR BUILD_TESTING)
  add_subdirectory(test)
endif()

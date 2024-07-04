# codec

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
	yuv.c)

set(CODEC_SSE2_SRCS
	sse/rfx_sse2.c
	sse/rfx_sse2.h
	sse/nsc_sse2.c
	sse/nsc_sse2.h
)

set(CODEC_NEON_SRCS
	neon/rfx_neon.c
	neon/rfx_neon.h
	neon/nsc_neon.c
	neon/nsc_neon.h
)

# Append initializers
set(CODEC_LIBS "")
list(APPEND CODEC_SRCS ${CODEC_SSE2_SRCS})
list(APPEND CODEC_SRCS ${CODEC_NEON_SRCS})

include(CompilerDetect)

if(WITH_SSE2)
    check_symbol_exists("__i586__"     ""  X86_i586)
    check_symbol_exists("__i686__"     ""  X86_i686)
    check_symbol_exists("__X86__"     ""  X86_X86)
    check_symbol_exists("_X86_"     ""  X86_X862)
    check_symbol_exists("__I86__"     ""  X86_I86)
    check_symbol_exists("__IA32__"     ""  X86_IA32)
    check_symbol_exists("_M_IX86"  ""  X86_M_IX86)
    check_symbol_exists("__amd64"  ""  X86_AMD64)
    check_symbol_exists("__amd64__"  ""  X86_AMD642)
    check_symbol_exists("__x86_64"  ""  X86_X86_64)
    check_symbol_exists("__x86_64__"  ""  X86_X86_642)
    check_symbol_exists("_M_X64"  ""  X86_X64)
    check_symbol_exists("__ia64"  ""  X86_IA64)
    check_symbol_exists("__ia64__"  ""  X86_IA642)
    check_symbol_exists("_M_IA64"  ""  X86_M_IA64)

    if (X86_i586 OR X86_i686 OR X86_X86 OR X86_X862 OR X86_I86 OR X86_IA32 OR
            X86_M_IX86 OR X86_AMD64 OR X86_AMD642 OR X86_X86_64 OR X86_X86_642 OR
            X86_X64 OR X86_IA64 OR X86_IA642 OR X86_M_IA64)
	if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
		if (CODEC_SSE2_SRCS)
			set_source_files_properties(${CODEC_SSE2_SRCS} PROPERTIES COMPILE_FLAGS "-msse2" )
		endif()
	endif()

	if(MSVC)
		if (CODEC_SSE2_SRCS)
			set_source_files_properties(${CODEC_SSE2_SRCS} PROPERTIES COMPILE_FLAGS "/arch:SSE2" )
		endif()
        endif()
    endif()
endif()
if(WITH_NEON)
    check_symbol_exists("_M_ARM64"     ""  MSVC_ARM64)
    check_symbol_exists("__aarch64__"  ""  ARCH_ARM64)

    if (MSVC_ARM64 OR ARCH_ARM64)
        if (CODEC_SSE2_SRCS)
                set_source_files_properties(${CODEC_NEON_SRCS} PROPERTIES COMPILE_FLAGS "-mfpu=neon" )
        endif()
    endif()
endif()

if (WITH_DSP_FFMPEG)
    set(CODEC_SRCS
        ${CODEC_SRCS}
		dsp_ffmpeg.c
		dsp_ffmpeg.h)
		include_directories(${FFMPEG_INCLUDE_DIRS})
		list(APPEND CODEC_LIBS ${FFMPEG_LIBRARIES})
endif (WITH_DSP_FFMPEG)

if (WITH_SOXR)
	list(APPEND CODEC_LIBS ${SOXR_LIBRARIES})
    include_directories(${SOXR_INCLUDE_DIR})
endif(WITH_SOXR)

if(GSM_FOUND)
	list(APPEND CODEC_LIBS ${GSM_LIBRARIES})
    include_directories(${GSM_INCLUDE_DIRS})
endif()

if(LAME_FOUND)
	list(APPEND CODEC_LIBS ${LAME_LIBRARIES})
    include_directories(${LAME_INCLUDE_DIRS})
endif()

if(FAAD2_FOUND)
	list(APPEND CODEC_LIBS ${FAAD2_LIBRARIES})
    include_directories(${FAAD2_INCLUDE_DIRS})
endif()

if(FAAC_FOUND)
	list(APPEND CODEC_LIBS ${FAAC_LIBRARIES})
    include_directories(${FAAC_INCLUDE_DIRS})
endif()

if(WITH_OPENH264)
	set(CODEC_SRCS ${CODEC_SRCS} h264_openh264.c)
	include_directories(${OPENH264_INCLUDE_DIR})
	if (NOT WITH_OPENH264_LOADING)
		list(APPEND CODEC_LIBS ${OPENH264_LIBRARIES})
	endif (NOT WITH_OPENH264_LOADING)
endif()

if(WITH_VIDEO_FFMPEG)
	set(CODEC_SRCS ${CODEC_SRCS} h264_ffmpeg.c)
	include_directories(${FFMPEG_INCLUDE_DIRS})
	list(APPEND CODEC_LIBS ${FFMPEG_LIBRARIES})
endif()

if(WIN32 AND WITH_MEDIA_FOUNDATION)
	set(CODEC_SRCS ${CODEC_SRCS} h264_mf.c)
endif()

if(ANDROID AND WITH_MEDIACODEC)
	list(APPEND CODEC_SRCS h264_mediacodec.c)

	find_library(MEDIACODEC mediandk REQUIRED)
	list(APPEND CODEC_LIBS ${MEDIACODEC})
endif()

add_library(freerdp-codecs OBJECT
	${CODEC_SRCS}
)
freerdp_library_add(${CODEC_LIBS})
freerdp_object_library_add(freerdp-codecs)

if(BUILD_TESTING)
	add_subdirectory(test)
endif()

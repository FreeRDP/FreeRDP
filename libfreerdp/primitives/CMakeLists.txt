# primitives

set(PRIMITIVES_SRCS
	prim_add.c
	prim_add.h
	prim_andor.c
	prim_andor.h
	prim_alphaComp.c
	prim_alphaComp.h
	prim_colors.c
	prim_colors.h
	prim_copy.c
	prim_copy.h
	prim_set.c
	prim_set.h
	prim_shift.c
	prim_shift.h
	prim_sign.c
	prim_sign.h
	prim_YUV.c
	prim_YUV.h
	prim_YCoCg.c
	prim_YCoCg.h
	primitives.c
	prim_internal.h)

set(PRIMITIVES_SSE2_SRCS
	sse/prim_colors_sse2.c
	sse/prim_set_sse2.c
	)

set(PRIMITIVES_SSE3_SRCS
	sse/prim_add_sse3.c
	sse/prim_alphaComp_sse3.c
	sse/prim_andor_sse3.c
	sse/prim_shift_sse3.c
	)

set(PRIMITIVES_SSSE3_SRCS
	sse/prim_YUV_ssse3.c
	sse/prim_sign_ssse3.c
	sse/prim_YCoCg_ssse3.c
	)

set(PRIMITIVES_SSE4_1_SRCS
	sse/prim_copy_sse4_1.c
	)

set(PRIMITIVES_SSE4_2_SRCS
	)

set(PRIMITIVES_AVX2_SRCS
	sse/prim_copy_avx2.c
	)

set(PRIMITIVES_NEON_SRCS
	neon/prim_colors_neon.c
	neon/prim_YCoCg_neon.c
	neon/prim_YUV_neon.c
	)

set(PRIMITIVES_OPENCL_SRCS
	opencl/prim_YUV_opencl.c
)

if (WITH_OPENCL)
    set(FILENAME "opencl/primitives.cl")
    set_source_files_properties(${FILENAME} PROPERTIES HEADER_FILE_ONLY ON)
    list(APPEND PRIMITIVES_OPENCL_SRCS ${FILENAME})

    include(ConvertFileToHexArray)
    file_to_hex_array(${FILENAME} FILEDATA)

    set(HDR_FILE "${CMAKE_CURRENT_BINARY_DIR}/opencl/primitives-opencl-program.h")
    cleaning_configure_file("${CMAKE_CURRENT_SOURCE_DIR}/opencl/primitives.h.in" ${HDR_FILE} @ONLY)
    list(APPEND PRIMITIVES_OPENCL_SRCS ${HDR_FILE})

    include_directories(${CMAKE_CURRENT_BINARY_DIR}/opencl)
    freerdp_include_directory_add(${OpenCL_INCLUDE_DIRS})
    freerdp_library_add(OpenCL::OpenCL)
endif()

set(PRIMITIVES_OPT_SRCS
	${PRIMITIVES_NEON_SRCS}
	${PRIMITIVES_SSE2_SRCS}
	${PRIMITIVES_SSE3_SRCS}
	${PRIMITIVES_SSSE3_SRCS}
	${PRIMITIVES_SSE4_1_SRCS}
	${PRIMITIVES_SSE4_2_SRCS}
	${PRIMITIVES_OPENCL_SRCS})

if (WITH_AVX2)
	list(APPEND PRIMITIVES_OPT_SRCS
		${PRIMITIVES_AVX2_SRCS}
	)
endif()

set(PRIMITIVES_SRCS ${PRIMITIVES_SRCS} ${PRIMITIVES_OPT_SRCS})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(freerdp-primitives OBJECT
	${PRIMITIVES_SRCS}
)

include(CompilerDetect)
include (DetectIntrinsicSupport)
if(WITH_SSE2)
    if (HAVE_SSE_OR_AVX)
	if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
		if(PRIMITIVES_SSE2_SRCS)
			set_source_files_properties(${PRIMITIVES_SSE2_SRCS} PROPERTIES COMPILE_FLAGS "-msse2" )
		endif()
		if (PRIMITIVES_SSE3_SRCS)
			set_source_files_properties(${PRIMITIVES_SSE3_SRCS} PROPERTIES COMPILE_FLAGS "-msse3" )
		endif()
		if (PRIMITIVES_SSSE3_SRCS)
			set_source_files_properties(${PRIMITIVES_SSSE3_SRCS} PROPERTIES COMPILE_FLAGS "-mssse3" )
		endif()
		if (PRIMITIVES_SSE4_1_SRCS)
			set_source_files_properties(${PRIMITIVES_SSE4_1_SRCS} PROPERTIES COMPILE_FLAGS "-msse4.1" )
		endif()
		if (PRIMITIVES_SSE4_2_SRCS)
			set_source_files_properties(${PRIMITIVES_SSE4_2_SRCS} PROPERTIES COMPILE_FLAGS "-msse4.2" )
		endif()
		if (PRIMITIVES_AVX2_SRCS AND WITH_AVX2)
			set_source_files_properties(${PRIMITIVES_AVX2_SRCS} PROPERTIES COMPILE_FLAGS "-mavx2" )
		endif()
	endif()

	if(MSVC)
		set_source_files_properties(${PRIMITIVES_OPT_SRCS} PROPERTIES COMPILE_FLAGS "/arch:SSE2")
        endif()
    endif()
elseif(WITH_NEON)
    # aarch64 requires NEON by default, no flag required
    if (HAVE_NEON AND NOT HAVE_NEON64)
        if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
            if (NOT MSVC_ARM64 AND NOT ARCH_ARM64)
                set_source_files_properties(${PRIMITIVES_OPT_SRCS} PROPERTIES COMPILE_FLAGS "-mfpu=neon")
            endif()
        endif()
        # TODO: Add MSVC equivalent
    endif()
endif()

freerdp_object_library_add(freerdp-primitives)

if(BUILD_TESTING AND NOT WIN32 AND NOT APPLE)
	add_subdirectory(test)
endif()

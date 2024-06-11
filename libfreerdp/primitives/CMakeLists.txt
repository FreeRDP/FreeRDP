# primitives

set(PRIMITIVES_SRCS
	prim_add.c
	prim_andor.c
	prim_alphaComp.c
	prim_colors.c
	prim_copy.c
	prim_set.c
	prim_shift.c
	prim_sign.c
	prim_YUV.c
	prim_YCoCg.c
	primitives.c
	prim_internal.h)

if (WITH_SSE2 OR WITH_NEON)
	set(PRIMITIVES_SSE2_SRCS
		prim_colors_opt.c
		prim_set_opt.c)

	set(PRIMITIVES_SSE3_SRCS
		prim_add_opt.c
		prim_alphaComp_opt.c
		prim_andor_opt.c
		prim_shift_opt.c)

	set(PRIMITIVES_SSSE3_SRCS
		prim_sign_opt.c
		prim_YCoCg_opt.c)

	if (WITH_SSE2)
		set(PRIMITIVES_SSSE3_SRCS ${PRIMITIVES_SSSE3_SRCS}
			prim_YUV_ssse3.c)
	endif()

	if (WITH_NEON)
		set(PRIMITIVES_SSSE3_SRCS ${PRIMITIVES_SSSE3_SRCS}
			prim_YUV_neon.c)
	endif()
endif()

if (WITH_OPENCL)
	set(PRIMITIVES_OPENCL_SRCS prim_YUV_opencl.c)

		freerdp_include_directory_add(${OpenCL_INCLUDE_DIRS})
		freerdp_library_add(OpenCL::OpenCL)

endif()

set(PRIMITIVES_OPT_SRCS
	${PRIMITIVES_SSE2_SRCS}
	${PRIMITIVES_SSE3_SRCS}
	${PRIMITIVES_SSSE3_SRCS}
	${PRIMITIVES_OPENCL_SRCS})

set(PRIMITIVES_SRCS ${PRIMITIVES_SRCS} ${PRIMITIVES_OPT_SRCS})

add_library(freerdp-primitives OBJECT
	${PRIMITIVES_SRCS}
)

if(WITH_SSE2)
	if(CMAKE_COMPILER_IS_GNUCC OR ${CMAKE_C_COMPILER_ID} STREQUAL "Clang")
		if(PRIMITIVES_SSE2_SRCS)
			set_source_files_properties(${PRIMITIVES_SSE2_SRCS} PROPERTIES COMPILE_FLAGS "-msse2" )
		endif()
		if (PRIMITIVES_SSE3_SRCS)
			set_source_files_properties(${PRIMITIVES_SSE3_SRCS} PROPERTIES COMPILE_FLAGS "-msse3" )
		endif()
		if (PRIMITIVES_SSSE3_SRCS)
			set_source_files_properties(${PRIMITIVES_SSSE3_SRCS} PROPERTIES COMPILE_FLAGS "-mssse3" )
		endif()
	endif()

	if(MSVC)
		set_source_files_properties(${PRIMITIVES_OPT_SRCS} PROPERTIES COMPILE_FLAGS "/arch:SSE2")
	endif()
elseif(WITH_NEON)
	if(CMAKE_COMPILER_IS_GNUCC OR ${CMAKE_C_COMPILER_ID} STREQUAL "Clang")
		if (NOT MSVC_ARM64 AND NOT ARCH_ARM64)
			set_source_files_properties(${PRIMITIVES_OPT_SRCS} PROPERTIES COMPILE_FLAGS "-mfpu=neon")
		endif()
	endif()
	# TODO: Add MSVC equivalent
endif()

freerdp_object_library_add(freerdp-primitives)

if(BUILD_TESTING AND NOT WIN32 AND NOT APPLE)
	add_subdirectory(test)
endif()

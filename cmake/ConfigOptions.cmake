include(CMakeDependentOption)

if((CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i686|x86|AMD64") AND (CMAKE_SIZEOF_VOID_P EQUAL 4))
	set(TARGET_ARCH "x86")
elseif((CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64") AND (CMAKE_SIZEOF_VOID_P EQUAL 8))
	set(TARGET_ARCH "x64")
elseif((CMAKE_SYSTEM_PROCESSOR MATCHES "i386") AND (CMAKE_SIZEOF_VOID_P EQUAL 8) AND (APPLE))
	# Mac is weird like that.
	set(TARGET_ARCH "x64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm*")
	set(TARGET_ARCH "ARM")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "sparc")
	set(TARGET_ARCH "sparc")
endif()

option(WITH_MANPAGES "Generate manpages." ON)
option(WITH_PROFILER "Compile profiler." OFF)
option(WITH_GPROF "Compile with GProf profiler." OFF)

if((TARGET_ARCH MATCHES "x86|x64") AND (NOT DEFINED WITH_SSE2))
	option(WITH_SSE2 "Enable SSE2 optimization." ON)
else()
	option(WITH_SSE2 "Enable SSE2 optimization." OFF)
endif()

if(TARGET_ARCH MATCHES "ARM")
	if (NOT DEFINED WITH_NEON)
		option(WITH_NEON "Enable NEON optimization." ON)
	else()
		option(WITH_NEON "Enable NEON optimization." OFF)
	endif()
else()
	if(NOT APPLE)
		option(WITH_IPP "Use Intel Performance Primitives." OFF)
	endif()
endif()

option(WITH_JPEG "Use JPEG decoding." OFF)

if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(CMAKE_COMPILER_IS_CLANG 1)
endif()

if(NOT WIN32)
	CMAKE_DEPENDENT_OPTION(WITH_VALGRIND_MEMCHECK "Compile with valgrind helpers." OFF
		"NOT WITH_SANITIZE_ADDRESS; NOT WITH_SANITIZE_MEMORY; NOT WITH_SANITIZE_THREAD" OFF)
	CMAKE_DEPENDENT_OPTION(WITH_SANITIZE_ADDRESS "Compile with gcc/clang address sanitizer." OFF
		"NOT WITH_VALGRIND_MEMCHECK; NOT WITH_SANITIZE_MEMORY; NOT WITH_SANITIZE_THREAD" OFF)
	CMAKE_DEPENDENT_OPTION(WITH_SANITIZE_MEMORY "Compile with gcc/clang memory sanitizer." OFF
        "NOT WITH_VALGRIND_MEMCHECK; NOT WITH_SANITIZE_ADDRESS; NOT WITH_SANITIZE_THREAD" OFF)
	CMAKE_DEPENDENT_OPTION(WITH_SANITIZE_THREAD "Compile with gcc/clang thread sanitizer." OFF
		"NOT WITH_VALGRIND_MEMCHECK; NOT WITH_SANITIZE_ADDRESS; NOT WITH_SANITIZE_MEMORY" OFF)
else()
	if(NOT UWP)
		option(WITH_MEDIA_FOUNDATION "Enable H264 media foundation decoder." OFF)
	endif()
endif()

if(WIN32 AND NOT UWP)
	option(WITH_WINMM "Use Windows Multimedia" ON)
	option(WITH_WIN8 "Use Windows 8 libraries" OFF)
endif()

option(BUILD_TESTING "Build unit tests" OFF)
CMAKE_DEPENDENT_OPTION(TESTS_WTSAPI_EXTRA "Build extra WTSAPI tests (interactive)" OFF "BUILD_TESTING" OFF)
CMAKE_DEPENDENT_OPTION(BUILD_COMM_TESTS "Build comm related tests (require comm port)" OFF "BUILD_TESTING" OFF)

option(WITH_SAMPLE "Build sample code" OFF)

option(WITH_CLIENT_COMMON "Build client common library" ON)
CMAKE_DEPENDENT_OPTION(WITH_CLIENT "Build client binaries" ON "WITH_CLIENT_COMMON" OFF)

option(WITH_SERVER "Build server binaries" OFF)

option(WITH_CHANNELS "Build virtual channel plugins" ON)

option(FREERDP_UNIFIED_BUILD "Build WinPR, uwac, RdTk and FreeRDP in one go" ON)

CMAKE_DEPENDENT_OPTION(WITH_CLIENT_CHANNELS "Build virtual channel plugins" ON
	"WITH_CLIENT_COMMON;WITH_CHANNELS" OFF)

CMAKE_DEPENDENT_OPTION(WITH_MACAUDIO "Enable OSX sound backend" ON "APPLE;NOT IOS" OFF)

if(WITH_SERVER AND WITH_CHANNELS)
	option(WITH_SERVER_CHANNELS "Build virtual channel plugins" ON)
endif()

option(WITH_THIRD_PARTY "Build third-party components" OFF)

option(WITH_CLIENT_INTERFACE "Build clients as a library with an interface" OFF)
option(WITH_SERVER_INTERFACE "Build servers as a library with an interface" ON)

option(WITH_DEBUG_ALL "Print all debug messages." OFF)

if(WITH_DEBUG_ALL)
    message(WARNING "WITH_DEBUG_ALL=ON, the build will be slow and might leak sensitive information, do not use with release builds!")
	set(DEFAULT_DEBUG_OPTION "ON")
else()
	set(DEFAULT_DEBUG_OPTION "OFF")
endif()

option(WITH_DEBUG_CERTIFICATE "Print certificate related debug messages." ${DEFAULT_DEBUG_OPTION})
if(WITH_DEBUG_CERTIFICATE)
    message(WARNING "WITH_DEBUG_CERTIFICATE=ON, the build might leak sensitive information, do not use with release builds!")
endif()
option(WITH_DEBUG_CAPABILITIES "Print capability negotiation debug messages." ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_CHANNELS "Print channel manager debug messages." ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_CLIPRDR "Print clipboard redirection debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_RDPGFX "Print RDPGFX debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_DVC "Print dynamic virtual channel debug messages." ${DEFAULT_DEBUG_OPTION})
CMAKE_DEPENDENT_OPTION(WITH_DEBUG_TSMF "Print TSMF virtual channel debug messages." ${DEFAULT_DEBUG_OPTION} "CHANNEL_TSMF" OFF)
option(WITH_DEBUG_KBD "Print keyboard related debug messages." ${DEFAULT_DEBUG_OPTION})
if(WITH_DEBUG_KBD)
    message(WARNING "WITH_DEBUG_KBD=ON, the build might leak sensitive information, do not use with release builds!")
endif()
option(WITH_DEBUG_LICENSE "Print license debug messages." ${DEFAULT_DEBUG_OPTION})
if(WITH_DEBUG_LICENSE)
    message(WARNING "WITH_DEBUG_LICENSE=ON, the build might leak sensitive information, do not use with release builds!")
endif()
option(WITH_DEBUG_NEGO "Print negotiation related debug messages." ${DEFAULT_DEBUG_OPTION})
if(WITH_DEBUG_NEGO)
    message(WARNING "WITH_DEBUG_NEGO=ON, the build might leak sensitive information, do not use with release builds!")
endif()
option(WITH_DEBUG_NLA "Print authentication related debug messages." ${DEFAULT_DEBUG_OPTION})
if(WITH_DEBUG_NLA)
    message(WARNING "WITH_DEBUG_NLA=ON, the build might leak sensitive information, do not use with release builds!")
endif()
option(WITH_DEBUG_TSG "Print Terminal Server Gateway debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_RAIL "Print RemoteApp debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_RDP "Print RDP debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_RDPEI "Print input virtual channel debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_REDIR "Redirection debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_RDPDR "Rdpdr debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_RFX "Print RemoteFX debug messages." ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_SCARD "Print smartcard debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_SND "Print rdpsnd debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_SVC "Print static virtual channel debug messages." ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_TRANSPORT "Print transport debug messages." ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_TIMEZONE "Print timezone debug messages." ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_WND "Print window order debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_X11_CLIPRDR "Print X11 clipboard redirection debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_X11_LOCAL_MOVESIZE "Print X11 Client local movesize debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_X11 "Print X11 Client debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_XV "Print XVideo debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_RINGBUFFER "Enable Ringbuffer debug messages" ${DEFAULT_DEBUG_OPTION})

option(WITH_DEBUG_SYMBOLS "Pack debug symbols to installer" OFF)
option(WITH_CCACHE "Use ccache support if available" ON)
option(WITH_CLANG_FORMAT "Detect clang-format. run 'cmake --build . --target clangformat' to format." ON)

option(WITH_DSP_EXPERIMENTAL "Enable experimental sound encoder/decoder formats" OFF)

option(WITH_FFMPEG "Enable FFMPEG for audio/video encoding/decoding" OFF)
CMAKE_DEPENDENT_OPTION(WITH_DSP_FFMPEG "Use FFMPEG for audio encoding/decoding" OFF
	"WITH_FFMPEG" OFF)
CMAKE_DEPENDENT_OPTION(WITH_VIDEO_FFMPEG "Use FFMPEG for video encoding/decoding" ON
	"WITH_FFMPEG" OFF)
CMAKE_DEPENDENT_OPTION(WITH_VAAPI "Use FFMPEG VAAPI" OFF
	"WITH_VIDEO_FFMPEG" OFF)

option(USE_VERSION_FROM_GIT_TAG "Extract FreeRDP version from git tag." ON)

option(WITH_CAIRO    "Use CAIRO image library for screen resizing" OFF)
option(WITH_SWSCALE  "Use SWScale image library for screen resizing" OFF)

if (ANDROID)
	include(ConfigOptionsAndroid)
endif(ANDROID)

if (IOS)
	include(ConfigOptionsiOS)
endif(IOS)

option(BUILD_FUZZERS "Use BUILD_FUZZERS to build fuzzing tests" OFF)

if (BUILD_FUZZERS)
    if (NOT OSS_FUZZ)
        add_compile_options(-fsanitize=fuzzer-no-link)
    endif ()

    if (OSS_FUZZ AND NOT DEFINED ENV{LIB_FUZZING_ENGINE})
        message(SEND_ERROR
            "OSS-Fuzz builds require the environment variable "
            "LIB_FUZZING_ENGINE to be set. If you are seeing this "
            "warning, it points to a deeper problem in the ossfuzz "
            "build setup.")
    endif ()

    if (CMAKE_COMPILER_IS_GNUCC)
        message(FATAL_ERROR
            "\n"
            "Fuzzing is unsupported with GCC compiler. Use Clang:\n"
            " $ CC=clang CXX=clang++ cmake . <...> -DBUILD_FUZZERS=ON && make -j\n"
            "\n")
    endif ()

    set(BUILD_TESTING ON)

    # A special target with fuzzer and sanitizer flags.
    add_library(fuzzer_config INTERFACE)

    target_compile_options(
        fuzzer_config
        INTERFACE
            $<$<NOT:$<BOOL:${OSS_FUZZ}>>:
            -fsanitize=fuzzer
            >
            $<$<BOOL:${OSS_FUZZ}>:
            ${CXX}
            ${CXXFLAGS}
            >
        )
    target_link_libraries(
        fuzzer_config
        INTERFACE
            $<$<NOT:$<BOOL:${OSS_FUZZ}>>:
            -fsanitize=fuzzer
            >
            $<$<BOOL:${OSS_FUZZ}>:
            $ENV{LIB_FUZZING_ENGINE}
            >
    )
endif()

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
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "e2k")
  set(TARGET_ARCH "e2k")
endif()

if(NOT OPENBSD AND NOT WIN32)
  set(MANPAGE_DEF ON)
endif()
option(WITH_MANPAGES "Generate manpages." ${MANPAGE_DEF})
option(WITH_PROFILER "Compile profiler." OFF)
option(WITH_GPROF "Compile with GProf profiler." OFF)

option(WITH_JPEG "Use JPEG decoding." OFF)

include(CompilerDetect)

if(NOT WIN32)
  cmake_dependent_option(
    WITH_VALGRIND_MEMCHECK "Compile with valgrind helpers." OFF
    "NOT WITH_SANITIZE_ADDRESS; NOT WITH_SANITIZE_MEMORY; NOT WITH_SANITIZE_THREAD" OFF
  )
  cmake_dependent_option(
    WITH_SANITIZE_ADDRESS "Compile with gcc/clang address sanitizer." OFF
    "NOT WITH_VALGRIND_MEMCHECK; NOT WITH_SANITIZE_MEMORY; NOT WITH_SANITIZE_THREAD" OFF
  )
  cmake_dependent_option(
    WITH_SANITIZE_MEMORY "Compile with gcc/clang memory sanitizer." OFF
    "NOT WITH_VALGRIND_MEMCHECK; NOT WITH_SANITIZE_ADDRESS; NOT WITH_SANITIZE_THREAD" OFF
  )
  cmake_dependent_option(
    WITH_SANITIZE_THREAD "Compile with gcc/clang thread sanitizer." OFF
    "NOT WITH_VALGRIND_MEMCHECK; NOT WITH_SANITIZE_ADDRESS; NOT WITH_SANITIZE_MEMORY" OFF
  )
else()
  if(NOT UWP)
    option(WITH_MEDIA_FOUNDATION "Enable H264 media foundation decoder." OFF)
  endif()
endif()

if(WIN32 AND NOT UWP)
  option(WITH_WINMM "Use Windows Multimedia" ON)
  option(WITH_WIN8 "Use Windows 8 libraries" OFF)
endif()

option(BUILD_TESTING "Build unit tests (compatible with packaging)" OFF)
cmake_dependent_option(
  BUILD_TESTING_INTERNAL "Build unit tests (CI only, not for packaging!)" OFF "NOT BUILD_TESTING" OFF
)
cmake_dependent_option(TESTS_WTSAPI_EXTRA "Build extra WTSAPI tests (interactive)" OFF "BUILD_TESTING_INTERNAL" OFF)
cmake_dependent_option(BUILD_COMM_TESTS "Build comm related tests (require comm port)" OFF "BUILD_TESTING_INTERNAL" OFF)

option(WITH_SAMPLE "Build sample code" ON)

option(WITH_CLIENT_COMMON "Build client common library" ON)
cmake_dependent_option(WITH_CLIENT "Build client binaries" ON "WITH_CLIENT_COMMON" OFF)
cmake_dependent_option(WITH_CLIENT_SDL "[experimental] Build SDL client " ON "WITH_CLIENT" OFF)

option(WITH_SERVER "Build server binaries" ON)

option(WITH_CHANNELS "Build virtual channel plugins" ON)

option(FREERDP_UNIFIED_BUILD "Build WinPR, uwac, RdTk and FreeRDP in one go" ON)

cmake_dependent_option(WITH_CLIENT_CHANNELS "Build virtual channel plugins" ON "WITH_CLIENT_COMMON;WITH_CHANNELS" OFF)

cmake_dependent_option(WITH_MACAUDIO "Enable OSX sound backend" ON "APPLE;NOT IOS" OFF)

if(WITH_SERVER AND WITH_CHANNELS)
  option(WITH_SERVER_CHANNELS "Build virtual channel plugins" ON)
endif()

option(WITH_THIRD_PARTY "Build third-party components" OFF)

option(WITH_CLIENT_INTERFACE "Build clients as a library with an interface" OFF)
cmake_dependent_option(
  CLIENT_INTERFACE_SHARED "Build clients as a shared library with an interface" OFF "WITH_CLIENT_INTERFACE" OFF
)
option(WITH_SERVER_INTERFACE "Build servers as a library with an interface" ON)

option(WITH_DEBUG_ALL "Print all debug messages." OFF)

if(WITH_DEBUG_ALL)
  message(
    WARNING
      "WITH_DEBUG_ALL=ON, the build will be slow and might leak sensitive information, do not use with release builds!"
  )
  set(DEFAULT_DEBUG_OPTION ON CACHE INTERNAL "debug default")
else()
  set(DEFAULT_DEBUG_OPTION OFF CACHE INTERNAL "debug default")
endif()

option(WITH_DEBUG_CERTIFICATE "Print certificate related debug messages." ${DEFAULT_DEBUG_OPTION})
if(WITH_DEBUG_CERTIFICATE)
  message(
    WARNING "WITH_DEBUG_CERTIFICATE=ON, the build might leak sensitive information, do not use with release builds!"
  )
endif()
option(WITH_DEBUG_CAPABILITIES "Print capability negotiation debug messages." ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_CHANNELS "Print channel manager debug messages." ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_CLIPRDR "Print clipboard redirection debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_CODECS "Print codec debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_RDPGFX "Print RDPGFX debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_DVC "Print dynamic virtual channel debug messages." ${DEFAULT_DEBUG_OPTION})
cmake_dependent_option(
  WITH_DEBUG_TSMF "Print TSMF virtual channel debug messages." ${DEFAULT_DEBUG_OPTION} "CHANNEL_TSMF" OFF
)
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
option(WITH_DEBUG_X11_LOCAL_MOVESIZE "Print X11 Client local movesize debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_X11 "Print X11 Client debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_XV "Print XVideo debug messages" ${DEFAULT_DEBUG_OPTION})
option(WITH_DEBUG_RINGBUFFER "Enable Ringbuffer debug messages" ${DEFAULT_DEBUG_OPTION})

option(WITH_DEBUG_SYMBOLS "Pack debug symbols to installer" OFF)
option(WITH_CCACHE "Use ccache support if available" ON)
option(WITH_CLANG_FORMAT "Detect clang-format. run 'cmake --build . --target clangformat' to format." ON)

option(WITH_DSP_EXPERIMENTAL "Enable experimental sound encoder/decoder formats" OFF)

option(WITH_FFMPEG "Enable FFMPEG for audio/video encoding/decoding" ON)
cmake_dependent_option(WITH_DSP_FFMPEG "Use FFMPEG for audio encoding/decoding" ON "WITH_FFMPEG" OFF)
cmake_dependent_option(WITH_VIDEO_FFMPEG "Use FFMPEG for video encoding/decoding" ON "WITH_FFMPEG" OFF)
cmake_dependent_option(WITH_VAAPI "Use FFMPEG VAAPI" OFF "WITH_VIDEO_FFMPEG" OFF)

option(USE_VERSION_FROM_GIT_TAG "Extract FreeRDP version from git tag." ON)

option(WITH_CAIRO "Use CAIRO image library for screen resizing" OFF)
option(WITH_SWSCALE "Use SWScale image library for screen resizing" ON)

if(ANDROID)
  include(ConfigOptionsAndroid)
endif(ANDROID)

if(IOS)
  include(ConfigOptionsiOS)
endif(IOS)

if(UNIX AND NOT APPLE)
  find_package(ALSA)
  find_package(PulseAudio)
  find_package(OSS)
  option(WITH_ALSA "use alsa for sound" ${ALSA_FOUND})
  option(WITH_PULSE "use alsa for sound" ${PULSE_FOUND})
  option(WITH_OSS "use alsa for sound" ${OSS_FOUND})
endif()

if(OPENBSD)
  find_package(SNDIO)
  option(WITH_SNDIO "use SNDIO for sound" ${SNDIO_FOUND # OpenBSD
         endif () }
  )
endif()

option(BUILD_FUZZERS "Use BUILD_FUZZERS to build fuzzing tests" OFF)

if(BUILD_FUZZERS)
  if(NOT OSS_FUZZ)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=fuzzer-no-link")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=fuzzer-no-link")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=fuzzer-no-link")
  endif()

  if(OSS_FUZZ AND NOT DEFINED ENV{LIB_FUZZING_ENGINE})
    message(SEND_ERROR "OSS-Fuzz builds require the environment variable "
                       "LIB_FUZZING_ENGINE to be set. If you are seeing this "
                       "warning, it points to a deeper problem in the ossfuzz " "build setup."
    )
  endif()

  if(CMAKE_COMPILER_IS_GNUCC)
    message(FATAL_ERROR "\n" "Fuzzing is unsupported with GCC compiler. Use Clang:\n"
                        " $ CC=clang CXX=clang++ cmake . <...> -DBUILD_FUZZERS=ON && make -j\n" "\n"
    )
  endif()

  set(BUILD_TESTING_INTERNAL ON CACHE BOOL "fuzzer default" FORCE)

  if(BUILD_SHARED_LIBS STREQUAL "OFF")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(CMAKE_CXX_FLAGS "-static ${CMAKE_CXX_FLAGS}")
  endif()

  # A special target with fuzzer and sanitizer flags.
  add_library(fuzzer_config INTERFACE)

  target_compile_options(
    fuzzer_config
    INTERFACE $<$<NOT:$<BOOL:${OSS_FUZZ}>>:
              -fsanitize=fuzzer
              >
              $<$<BOOL:${OSS_FUZZ}>:
              ${CXX}
              ${CXXFLAGS}
              >
  )
  target_link_libraries(
    fuzzer_config INTERFACE $<$<NOT:$<BOOL:${OSS_FUZZ}>>: -fsanitize=fuzzer > $<$<BOOL:${OSS_FUZZ}>:
                            $ENV{LIB_FUZZING_ENGINE} >
  )
endif()

option(
  WITH_FULL_CONFIG_PATH
  "Use <appdata>/Vendor/Product instead of <appdata>/product (lowercase, only if vendor equals product) as config directory"
  OFF
)

# Configuration settings for manpages
if(NOT WITH_FULL_CONFIG_PATH AND "${VENDOR}" STREQUAL "${PRODUCT}")
  string(TOLOWER "${VENDOR}" VENDOR_PRODUCT)
else()
  set(VENDOR_PRODUCT "${VENDOR}/${PRODUCT}")
endif()

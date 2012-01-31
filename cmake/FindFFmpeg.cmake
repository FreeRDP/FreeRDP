# - Try to find FFmpeg
# Using Pkg-config if available for path
#
#  FFMPEG_FOUND        - all required ffmpeg components found on system
#  FFMPEG_INCLUDE_DIRS  - combined include directories
#  FFMPEG_LIBRARIES    - combined libraries to link

include(FindPkgConfig)

if (PKG_CONFIG_FOUND)
	pkg_check_modules(AVCODEC libavcodec)
	pkg_check_modules(AVUTIL libavutil)
endif ( PKG_CONFIG_FOUND )

# avcodec
find_path(AVCODEC_INCLUDE_DIR avcodec.h PATHS ${AVCODEC_INCLUDE_DIRS}
          PATH_SUFFIXES libavcodec )
find_library(AVCODEC_LIBRARY avcodec PATHS ${AVCODEC_LIBRARY_DIRS})

# avutil
find_path(AVUTIL_INCLUDE_DIR avutil.h PATHS ${AVUTIL_INCLUDE_DIRS}
          PATH_SUFFIXES libavutil )
find_library(AVUTIL_LIBRARY avutil PATHS ${AVUTIL_LIBRARY_DIRS})

if(AVCODEC_INCLUDE_DIR AND AVCODEC_LIBRARY)
	set(AVCODEC_FOUND TRUE)
endif()

if(AVUTIL_INCLUDE_DIR AND AVUTIL_LIBRARY)
	set(AVUTIL_FOUND TRUE)
endif()


FIND_PACKAGE_HANDLE_STANDARD_ARGS(FFmpeg DEFAULT_MSG AVUTIL_FOUND AVCODEC_FOUND)

if(FFMPEG_FOUND)
	set(FFMPEG_INCLUDE_DIRS ${AVCODEC_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR})
	set(FFMPEG_LIBRARIES ${AVCODEC_LIBRARY} ${AVUTIL_LIBRARY})
endif()

mark_as_advanced(FFMPEG_INCLUDE_DIRS FFMPEG_LIBRARYS)

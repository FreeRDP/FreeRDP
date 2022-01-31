# Find the FUSE includes and library
#
#  FUSE_INCLUDE_DIR - where to find fuse.h, etc.
#  FUSE_LIBRARIES   - List of libraries when using FUSE.
#  FUSE_FOUND       - True if FUSE lib is found.

unset(FUSE_LIBRARIES CACHE)
unset(FUSE_INCLUDE_DIR CACHE)
unset(FUSE_API_VERSION CACHE)

include(FindPackageHandleStandardArgs)

macro(find_fuse2)
    find_path(FUSE_INCLUDE_DIR fuse/fuse_lowlevel.h
            /usr/local/include/osxfuse
            /usr/local/include
            /usr/include
    )
    if (APPLE)
        SET(FUSE_NAMES libosxfuse.dylib fuse)
    else ()
        SET(FUSE_NAMES fuse)
    endif ()
    FIND_LIBRARY(FUSE_LIBRARIES
            NAMES ${FUSE_NAMES}
            PATHS /lib64 /lib /usr/lib64 /usr/lib /usr/local/lib64 /usr/local/lib /usr/lib/x86_64-linux-gnu
            )

    if(FUSE_INCLUDE_DIR)
        file(READ "${FUSE_INCLUDE_DIR}/fuse/fuse_common.h" _FUSE_COMMON_H_CONTENTS)
        string(REGEX REPLACE "^(.*\n)?#define[ \t]+FUSE_MINOR_VERSION[ \t]+([0-9]+).*"
                "\\2" FUSE_MINOR_VERSION ${_FUSE_COMMON_H_CONTENTS})
        if( FUSE_MINOR_VERSION AND FUSE_MINOR_VERSION GREATER 5)
            set(FUSE_API_VERSION 26)
        else()
            set(FUSE_API_VERSION 21)
        endif()
    endif()

endmacro()

macro(find_fuse3)
    find_path(FUSE_INCLUDE_DIR fuse3/fuse_lowlevel.h
            /usr/local/include/osxfuse
            /usr/local/include
            /usr/include
    )
    if (APPLE)
        SET(FUSE_NAMES libosxfuse.dylib fuse3)
    else ()
        SET(FUSE_NAMES fuse3)
    endif ()
    FIND_LIBRARY(FUSE_LIBRARIES
            NAMES ${FUSE_NAMES}
            PATHS /lib64 /lib /usr/lib64 /usr/lib /usr/local/lib64 /usr/local/lib /usr/lib/x86_64-linux-gnu
            )

    if(FUSE_INCLUDE_DIR)
        file(READ "${FUSE_INCLUDE_DIR}/fuse3/fuse_common.h" _FUSE_COMMON_H_CONTENTS)
        string(REGEX REPLACE "^(.*\n)?#define[ \t]+FUSE_MINOR_VERSION[ \t]+([0-9]+).*"
                "\\2" FUSE_MINOR_VERSION ${_FUSE_COMMON_H_CONTENTS})
        if( FUSE_MINOR_VERSION GREATER_EQUAL 5)
            set(FUSE_API_VERSION 35)
        else()
            set(FUSE_API_VERSION 30)
        endif()
    endif()
endmacro()

if(WITH_FUSE3)
    find_fuse3()
elseif(WITH_FUSE2)
    find_fuse2()
else()
    find_fuse3()
    if (NOT FUSE_INCLUDE_DIR)
        find_fuse2()
    endif()
endif()


find_package_handle_standard_args ("FUSE" DEFAULT_MSG
    FUSE_LIBRARIES FUSE_INCLUDE_DIR FUSE_API_VERSION)


mark_as_advanced (FUSE_INCLUDE_DIR FUSE_LIBRARIES)

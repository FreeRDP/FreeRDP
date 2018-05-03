
find_path(LAME_INCLUDE_DIR lame/lame.h)

find_library(LAME_LIBRARY NAMES lame mp3lame)

find_package_handle_standard_args(LAME DEFAULT_MSG LAME_INCLUDE_DIR LAME_LIBRARY)

if (LAME_FOUND)
	set(LAME_LIBRARIES ${LAME_LIBRARY})
	set(LAME_INCLUDE_DIRS ${LAME_INCLUDE_DIR})
endif()

mark_as_advanced(LAME_INCLUDE_DIR LAME_LIBRARY)


find_path(FAAC_INCLUDE_DIR faac.h)

find_library(FAAC_LIBRARY faac)

find_package_handle_standard_args(FAAC DEFAULT_MSG FAAC_INCLUDE_DIR FAAC_LIBRARY)

if(FAAC_FOUND)
	set(FAAC_LIBRARIES ${FAAC_LIBRARY})
	set(FAAC_INCLUDE_DIRS ${FAAC_INCLUDE_DIR})
endif()

mark_as_advanced(FAAC_INCLUDE_DIR FAAC_LIBRARY)

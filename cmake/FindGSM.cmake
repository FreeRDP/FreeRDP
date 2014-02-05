
find_path(GSM_INCLUDE_DIR gsm/gsm.h)

find_library(GSM_LIBRARY gsm)

find_package_handle_standard_args(GSM DEFAULT_MSG GSM_INCLUDE_DIR GSM_LIBRARY)

if(GSM_FOUND)
	set(GSM_LIBRARIES ${GSM_LIBRARY})
	set(GSM_INCLUDE_DIRS ${GSM_INCLUDE_DIR})
endif()

mark_as_advanced(GSM_INCLUDE_DIR GSM_LIBRARY)

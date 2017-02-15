
find_library(PCSC_WINPR_LIBRARY
	NAMES libpcsc-winpr.a
	PATHS
		/opt/lib
		/usr/lib
		/usr/local/lib
	)

if(NOT ${PCSC_WINPR_LIBRARY} MATCHES ".*-NOTFOUND")
	set(PCSC_WINPR_FOUND 1)
	message(STATUS "Found PCSC-WinPR: ${PCSC_WINPR_LIBRARY}")
endif()

mark_as_advanced(PCSC_WINPR_LIBRARY)

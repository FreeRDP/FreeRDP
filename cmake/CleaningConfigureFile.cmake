# Little helper that adds the generated file to the
# files to be cleaned in the current directory.
#
# Handy if the generated files might have changed
#

function(cleaning_configure_file SRC DST)
	configure_file(${SRC} ${DST} ${ARGN})
	set_property(
 	        DIRECTORY
 	        APPEND
 	        PROPERTY ADDITIONAL_CLEAN_FILES
	       	${DST}
 	)
	set_property(
		DIRECTORY
	       	APPEND
	       	PROPERTY CMAKE_CONFIGURE_DEPENDS
		${SRC}
	       	${DST}
	)
endfunction()

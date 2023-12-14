include(GNUInstallDirs)
include(FindDocBookXSL)

function(install_freerdp_man manpage section)
 if(WITH_MANPAGES)
   install(FILES ${manpage} DESTINATION ${CMAKE_INSTALL_MANDIR}/man${section})
 endif()
endfunction()

function(generate_and_install_freerdp_man_from_xml manpage dependencies)
    if(WITH_MANPAGES)
	find_program(XSLTPROC_EXECUTABLE NAMES xsltproc REQUIRED)
	if (NOT DOCBOOKXSL_FOUND)
		message(FATAL_ERROR "docbook xsl not found but required for manpage generation")
	endif()

	# We need the variable ${MAN_TODAY} to contain the current date in ISO
	# format to replace it in the configure_file step.
	include(today)

	TODAY(MAN_TODAY)

	configure_file(${manpage}.xml.in ${manpage}.xml @ONLY IMMEDIATE)

	# Compile the helper tool with default compiler settings.
	# We need the include paths though.
	get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
	set(GENERATE_INCLUDES "")
	foreach(dir ${dirs})
		set(GENERATE_INCLUDES ${GENERATE_INCLUDES} -I${dir})
	endforeach(dir)

	set(dep_SRC)
	set(deb_BIN)
	foreach(dep ${dependencies})
		set(cur_SRC  ${CMAKE_CURRENT_SOURCE_DIR}/${dep})
		set(cur_BIN  ${CMAKE_CURRENT_BINARY_DIR}/${dep})
		list(APPEND dep_SRC ${cur_SRC})
		list(APPEND dep_BIN ${cur_BIN})

		add_custom_command(
			OUTPUT ${cur_BIN}
			COMMAND ${CMAKE_COMMAND} -E copy ${cur_SRC} ${cur_BIN}
			DEPENDS ${cur_SRC}
			VERBATIM
		)
	endforeach()

	add_custom_command(
		OUTPUT ${manpage}
				COMMAND ${CMAKE_C_COMPILER} ${GENERATE_INCLUDES}
                                        ${CMAKE_SOURCE_DIR}/client/common/man/generate_argument_docbook.c
					-o ${CMAKE_CURRENT_BINARY_DIR}/generate_argument_docbook
				COMMAND ${CMAKE_CURRENT_BINARY_DIR}/generate_argument_docbook
				COMMAND ${XSLTPROC_EXECUTABLE} ${DOCBOOKXSL_DIR}/manpages/docbook.xsl ${manpage}.xml
				WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
				DEPENDS
					${CMAKE_CURRENT_BINARY_DIR}/${manpage}.xml
					${dep_BIN}
				)

	add_custom_target(
		${manpage}.manpage ALL
		DEPENDS
			${manpage}
		)
	install_freerdp_man(${CMAKE_CURRENT_BINARY_DIR}/${manpage} 1)
    endif()
endfunction()

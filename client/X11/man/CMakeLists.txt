set(DEPS
        xfreerdp-channels.1.xml
        xfreerdp-examples.1.xml
        xfreerdp-shortcuts.1.xml
        xfreerdp-envvar.1.xml
    )

set(MANPAGE_NAME ${PROJECT_NAME})
if (WITH_BINARY_VERSIONING)
	set(MANPAGE_NAME ${PROJECT_NAME}${PROJECT_VERSION_MAJOR})
endif()
generate_and_install_freerdp_man_from_xml(${PROJECT_NAME}.1 ${MANPAGE_NAME}.1 ${DEPS})

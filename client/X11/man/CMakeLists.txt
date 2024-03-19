set(DEPS
        xfreerdp-channels.1.xml
        xfreerdp-examples.1.xml
        xfreerdp-shortcuts.1.xml
        xfreerdp-envvar.1.xml
    )

generate_and_install_freerdp_man_from_xml("xfreerdp" "1" "${FREERDP_API_VERSION}" ${DEPS})

set(DEPS
        sdl-freerdp-channels.1.xml
        sdl-freerdp-config.1.xml
        sdl-freerdp-examples.1.xml
        sdl-freerdp-envvar.1.xml
    )

generate_and_install_freerdp_man_from_xml(${PROJECT_NAME} "1" "${FREERDP_API_VERSION}" "${DEPS}")

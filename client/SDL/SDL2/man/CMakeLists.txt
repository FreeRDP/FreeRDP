set(DEPS
		../../common/man/sdl-freerdp-channels.1.xml
		../../common/man/sdl-freerdp-config.1.xml
		../../common/man/sdl-freerdp-examples.1.xml
		../../common/man/sdl-freerdp-envvar.1.xml
    )

generate_and_install_freerdp_man_from_xml(${MODULE_NAME} "1" "${DEPS}")

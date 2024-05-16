set(DEPS
		sdl3-freerdp-channels.1.xml
		sdl3-freerdp-config.1.xml
		sdl3-freerdp-examples.1.xml
		sdl3-freerdp-envvar.1.xml
    )

generate_and_install_freerdp_man_from_xml(${MODULE_NAME} "1" "${DEPS}")

set(DEPS
    ../../../common/man/freerdp-global-options.1 ../../common/man/sdl-freerdp-config.1
    ../../../common/man/freerdp-global-envvar.1 ../../../common/man/freerdp-global-config.1
    ../../common/man/sdl-freerdp-examples.1 ../../../common/man/freerdp-global-links.1
)

set(VAR_NAMES "VENDOR" "PRODUCT" "VENDOR_PRODUCT" "CMAKE_INSTALL_FULL_SYSCONFDIR")
generate_and_install_freerdp_man_from_xml(${MODULE_NAME} "1" "${DEPS}" "${VAR_NAMES}")

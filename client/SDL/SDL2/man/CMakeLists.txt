set(DEPS
    ../../../common/man/freerdp-global-options.1
    ../../../common/man/freerdp-global-envvar.1
    ../../common/man/sdl-freerdp-config.1
    ../../../common/man/freerdp-global-config.1
    ../../common/man/sdl-global-config.1
    ../../common/man/sdl-freerdp-examples.1
    ../../../common/man/freerdp-global-links.1
)

include(GetSysconfDir)
get_sysconf_dir("" SYSCONF_DIR)
set(SDL_WIKI_BASE_URL "https://wiki.libsdl.org/SDL2")
set(VAR_NAMES "VENDOR" "PRODUCT" "VENDOR_PRODUCT" "SYSCONF_DIR" "SDL_WIKI_BASE_URL")
generate_and_install_freerdp_man_from_xml(${MODULE_NAME} "1" "${DEPS}" "${VAR_NAMES}")

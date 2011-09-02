install(FILES aliases amiga ataritt empty evdev fujitsu hp ibm macintosh macosx sony sun xfree86 xfree98 xkb.pl DESTINATION share/freerdp/keymaps)
install(DIRECTORY digital_vndr DESTINATION share/freerdp/keymaps FILES_MATCHING PATTERN "*")
install(DIRECTORY sgi_vndr DESTINATION share/freerdp/keymaps FILES_MATCHING PATTERN "*")

include(ExternalProject)

ExternalProject_Add(
  openh264
  DOWNLOAD_COMMAND
	  URL      https://github.com/cisco/openh264/archive/refs/tags/v2.5.0.tar.gz
	  URL_HASH SHA256=94c8ca364db990047ec4ec3481b04ce0d791e62561ef5601443011bdc00825e3
  CONFIGURE_COMMAND
	meson setup <BINARY_DIR> -Dprefix=<INSTALL_DIR>
  BUILD_COMMAND
  	ninja -C <BINARY_DIR>
  INSTALL_COMMAND
  	ninja -C <BINARY_DIR> install
)


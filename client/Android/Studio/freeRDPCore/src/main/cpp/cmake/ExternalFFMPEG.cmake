#include(FetchContent)
#ExternalProject_Add(
#  openh264
#  URL      https://github.com/cisco/openh264/archive/refs/tags/v2.5.0.tar.gz
#  URL_HASH SHA256=94c8ca364db990047ec4ec3481b04ce0d791e62561ef5601443011bdc00825e3
#  -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
#)


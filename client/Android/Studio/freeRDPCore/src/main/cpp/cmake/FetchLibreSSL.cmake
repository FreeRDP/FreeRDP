include(FetchContent)
    
set(LIBRESSL_APPS OFF CACHE INTERNAL "fetch content")
set(LIBRESSL_TESTS OFF CACHE INTERNAL "fetch content")
FetchContent_Declare(
  libressl
  URL      https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-4.0.0.tar.gz
  URL_HASH SHA256=4d841955f0acc3dfc71d0e3dd35f283af461222350e26843fea9731c0246a1e4
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)

FetchContent_MakeAvailable(libressl)

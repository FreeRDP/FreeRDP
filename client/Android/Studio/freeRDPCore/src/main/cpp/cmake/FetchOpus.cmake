include(FetchContent)

FetchContent_Declare(
  opus
  URL      https://downloads.xiph.org/releases/opus/opus-1.5.2.tar.gz
  URL_HASH SHA256=65c1d2f78b9f2fb20082c38cbe47c951ad5839345876e46941612ee87f9a7ce1
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)

FetchContent_MakeAvailable(opus)

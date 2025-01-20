include(FetchContent)

set(ENABLE_CJSON_UNINSTALL OFF CACHE INTERNAL "fetch content")
set(ENABLE_CJSON_TEST OFF CACHE INTERNAL "fetch content")
FetchContent_Declare(
  cJSON
  URL      https://github.com/DaveGamble/cJSON/archive/refs/tags/v1.7.18.tar.gz
  URL_HASH SHA256=3aa806844a03442c00769b83e99970be70fbef03735ff898f4811dd03b9f5ee5
)

FetchContent_MakeAvailable(cJSON)

#include <freerdp/assistance.h>

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  size_t *sz = &Size;
  freerdp_assistance_hex_string_to_bin((void*)Data, sz);
  return 0;
}

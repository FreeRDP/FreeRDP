#include <freerdp/assistance.h>

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  size_t* sz = &Size;
  freerdp_assistance_bin_to_hex_string((void*)Data, *sz);
  return 0;
}

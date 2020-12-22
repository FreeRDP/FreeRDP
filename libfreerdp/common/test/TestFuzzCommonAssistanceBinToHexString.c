#include <freerdp/assistance.h>

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	char* pass = freerdp_assistance_bin_to_hex_string((void*)Data, Size);
	free(pass);
	return 0;
}

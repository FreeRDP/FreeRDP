#include <freerdp/assistance.h>

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	BYTE* pass = freerdp_assistance_hex_string_to_bin((void*)Data, &Size);
	free(pass);

	return 0;
}

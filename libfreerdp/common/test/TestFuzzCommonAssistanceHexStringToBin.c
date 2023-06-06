#include <freerdp/assistance.h>

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	char* buf = calloc(Size + 1, sizeof(char));
	if (buf == NULL)
		return 0;
	memcpy(buf, Data, Size);
	buf[Size] = '\0';

	BYTE* pass = freerdp_assistance_hex_string_to_bin((void*)buf, &Size);
	free(pass);
	free(buf);

	return 0;
}

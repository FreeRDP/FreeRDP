#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	bool pc = true;
	BYTE tag = "";
	er_read_application_tag((wStream*)Data, tag, &Size);

	return EXIT_SUCCESS;
}

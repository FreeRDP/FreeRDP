#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	bool pc = true;
	BYTE tag = "";
	ber_read_contextual_tag((wStream*)Data, tag, &Size, pc);

	return EXIT_SUCCESS;
}

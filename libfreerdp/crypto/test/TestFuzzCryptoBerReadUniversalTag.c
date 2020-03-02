#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	bool tag = true;
	bool pc = true;
	ber_read_universal_tag((wStream*)Data, tag, pc);

	return EXIT_SUCCESS;
}

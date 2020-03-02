#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	er_read_sequence_tag((wStream*)Data, Size);

	return EXIT_SUCCESS;
}

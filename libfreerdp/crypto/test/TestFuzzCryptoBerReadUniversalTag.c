#include <freerdp/crypto/ber.h>
#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	bool tag = true;
	bool pc = true;
	wStream* s = Stream_New((BYTE*)Data, Size);
	ber_read_universal_tag(s, tag, pc);
	Stream_Free(s, FALSE);

	return EXIT_SUCCESS;
}

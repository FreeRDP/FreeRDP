#include <freerdp/crypto/ber.h>
#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	wStream* s = Stream_New((BYTE*)Data, Size);
	ber_read_length(s, &Size);
	Stream_Free(s, FALSE);

	return 0;
}

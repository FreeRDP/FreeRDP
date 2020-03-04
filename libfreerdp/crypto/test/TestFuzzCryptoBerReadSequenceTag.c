#include <freerdp/crypto/ber.h>
#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	bool rc = false;
	wStream* s = Stream_New((BYTE*)Data, Size);
	rc = ber_read_sequence_tag(s, &Size);
	Stream_Free(s, FALSE);

	return rc;
}

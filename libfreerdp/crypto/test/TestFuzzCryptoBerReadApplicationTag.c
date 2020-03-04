#include <freerdp/crypto/ber.h>
#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	bool pc = true, rc = false;
	BYTE tag;
	wStream* s = Stream_New((BYTE*)Data, Size);
	rc = ber_read_application_tag(s, tag, &Size);
	Stream_Free(s, FALSE);

	return rc;
}

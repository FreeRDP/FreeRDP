#include <freerdp/crypto/er.h>
#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	BYTE tag = '0';
	bool rc = false;
	bool pc = true;
	wStream* s = Stream_New((BYTE*)Data, Size);
	rc = er_read_universal_tag(s, tag, pc);
	Stream_Free(s, FALSE);

	return rc;
}

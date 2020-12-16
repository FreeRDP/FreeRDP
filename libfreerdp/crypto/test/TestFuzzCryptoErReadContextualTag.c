#include <freerdp/crypto/er.h>
#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	bool pc = true, rc = false;
	BYTE tag = '0';
	wStream* s = Stream_New((BYTE*)Data, Size);
	er_read_contextual_tag(s, tag, (int*)Size, pc);
	Stream_Free(s, FALSE);

	return 0;
}

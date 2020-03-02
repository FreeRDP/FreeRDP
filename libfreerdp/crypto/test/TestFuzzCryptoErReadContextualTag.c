#include <freerdp/crypto/er.h>
#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	bool pc = true;
	BYTE tag = "";
	wStream* s = Stream_New((BYTE*)Data, Size);
	er_read_contextual_tag(s, tag, &Size, pc);
	Stream_Free(s, FALSE);

	return EXIT_SUCCESS;
}

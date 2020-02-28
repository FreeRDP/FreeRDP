#include <freerdp/utils/passphrase.h>

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	freerdp_passphrase_read(NULL, (char*)Data, Size, 0);
	return 0;
}

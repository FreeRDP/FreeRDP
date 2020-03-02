#include <freerdp/utils/passphrase.h>

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	freerdp_passphrase_read(NULL, (char*)Data, Size, 1);
	return 0;
}

#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	crypto_cert_read((BYTE*)Data, (UINT32)Size);

	return EXIT_SUCCESS;
}

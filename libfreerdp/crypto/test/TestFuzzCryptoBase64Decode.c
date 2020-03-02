#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	int ntlmTokenLength = 0;
	BYTE* ntlmTokenData = NULL;
	crypto_base64_decode((char*)Data, (int)Size, &ntlmTokenData, &ntlmTokenLength);

	return EXIT_SUCCESS;
}

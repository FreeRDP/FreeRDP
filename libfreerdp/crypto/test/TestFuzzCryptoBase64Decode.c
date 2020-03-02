#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	int ntlmTokenLength = 0;
	BYTE* ntlmTokenData = NULL;
	char* data = (char*)Data;
	if (Size > UINTPTR_MAX) {
	    data[UINTPTR_MAX] = '\0';
	}
	crypto_base64_decode(data, UINTPTR_MAX, &ntlmTokenData, &ntlmTokenLength);

	return EXIT_SUCCESS;
}

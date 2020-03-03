#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	int ntlmTokenLength = 0;
	BYTE* ntlmTokenData = NULL;
	char* data = (char*)Data;
	if (Size > INT_MAX) {
	    data[INT_MAX] = '\0';
	}
	crypto_base64_decode(data, INT_MAX, &ntlmTokenData, &ntlmTokenLength);

	return EXIT_SUCCESS;
}

#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	int ntlmTokenLength = 0;
	BYTE* ntlmTokenData = NULL;
	char* data = (char*)Data;
	int data_size = INT_MAX;
	if (Size > data_size) {
	    data[data_size] = '\0';
	} else {
	    data_size = Size;
	}
	crypto_base64_decode(data, data_size, &ntlmTokenData, &ntlmTokenLength);
	free(ntlmTokenData);

	return 0;
}

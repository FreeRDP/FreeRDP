#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	uint32_t data_size = UINT32_MAX;
	BYTE* data = (BYTE*)Data;
	if (Size > data_size) {
		data[data_size] = '\0';
	} else {
		data_size = Size;
	}
	crypto_cert_read(data, data_size);

	return 0;
}

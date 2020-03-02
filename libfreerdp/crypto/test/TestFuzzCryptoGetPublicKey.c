#include <freerdp/crypto/crypto.h>

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	CryptoCert cert = malloc(sizeof(*cert));
	crypto_cert_get_public_key(cert, (BYTE**)&Data, (DWORD*)Size);
	return 0;
}

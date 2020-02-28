#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	X509* certificate = (X509*)Data;
	/*
	int crypto_rsa_private_encrypt(const BYTE* input, int length, UINT32 key_length,
	                               const BYTE* modulus, const BYTE* private_exponent, BYTE* output)
	int crypto_rsa_private_decrypt(const BYTE* input, int length, UINT32 key_length,
	                               const BYTE* modulus, const BYTE* private_exponent, BYTE* output)
	*/
	return 0;
}

#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	BYTE* crypt_client_random = NULL;
	rdpSettings* settings;
	UINT32 key_len;
	BYTE* mod, *exp;

	settings = freerdp_settings_new(0);
	key_len = settings->RdpServerCertificate->cert_info.ModulusLength;
	mod = settings->RdpServerCertificate->cert_info.Modulus;
	exp = settings->RdpServerCertificate->cert_info.exponent;
	crypt_client_random = calloc(key_len + 8, 1);
	if (!crypt_client_random)
		return 0;
	crypto_rsa_public_encrypt((BYTE*)Data, (int)Size, key_len, mod, exp, crypt_client_random);
	crypto_rsa_public_decrypt((BYTE*)Data, (int)Size, key_len, mod, exp, crypt_client_random);
	freerdp_settings_free(settings);

	return 0;
}

#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	X509* certificate = (X509*)Data;
	crypto_cert_get_email(certificate);
	return 0;
}

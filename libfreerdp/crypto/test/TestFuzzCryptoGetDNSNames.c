#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	X509* certificate = (X509*)Data;
	int count = 5;
	crypto_cert_get_dns_names(certificate, &count, 0);
	return 0;
}

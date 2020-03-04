#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	char** dns_names = 0;
	int dns_names_count = 0;
	int* dns_names_lengths = NULL;
	// FIXME: remove casting of garbage to X509 obj
	X509* certificate = (X509*)Data;
	dns_names = crypto_cert_get_dns_names(certificate, &dns_names_count, &dns_names_lengths);
	if (dns_names) {
		crypto_cert_dns_names_free(dns_names_count, dns_names_lengths, dns_names);
	}
	X509_free(certificate);

	return 0;
}

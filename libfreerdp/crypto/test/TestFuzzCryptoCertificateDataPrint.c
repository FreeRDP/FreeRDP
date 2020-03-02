#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	rdpCertificate* certificate = NULL;
	rdpCertificateStore* certificate_store = NULL;
	rdpCertificateData* certificate_data = NULL;
	rdpSettings* settings;

	settings = freerdp_settings_new(0);
	certificate_store = certificate_store_new(settings);
	certificate_data = certificate_data_new("someurl", 3389, "subject", "issuer", "ff:11:22:dd");
	certificate_data_match(certificate_store, certificate_data);
	freerdp_settings_free(settings);

	return EXIT_SUCCESS;
}

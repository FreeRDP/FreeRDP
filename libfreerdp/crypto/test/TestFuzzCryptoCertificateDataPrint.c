#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	rdpCertificate* certificate = NULL;
	rdpCertificateStore* certificate_store = NULL;
	rdpSettings* settings;

	settings = freerdp_settings_new(0);
	certificate_store = certificate_store_new(settings);
	certificate_data_match(certificate_store, (rdpCertificateData*)Data);
	freerdp_settings_free(settings);

	return EXIT_SUCCESS;
}

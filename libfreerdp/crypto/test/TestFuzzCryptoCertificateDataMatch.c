#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	rdpCertificateStore* certificate_store = NULL;
	rdpCertificateData* certificate_data = NULL;
	rdpSettings* settings;
	int match = 0;

	settings = freerdp_settings_new(0);
	certificate_store = certificate_store_new(settings);
	if (!certificate_store)
	{
		fprintf(stderr, "Could not create certificate store!\n");
	}

	UINT16 port = 8080;
	char* garbage = (char*)Data;
	int size = (int)Size;
	// FIXME:
	garbage[size - 1] = '\0';
	certificate_data = certificate_data_new(garbage, port, garbage, garbage, garbage);
	match = certificate_data_match(certificate_store, certificate_data);
	certificate_data_free(certificate_data);
	freerdp_settings_free(settings);
	certificate_store_free(certificate_store);

	return match;
}

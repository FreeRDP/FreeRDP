#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	rdpCertificateStore* store = NULL;
	rdpSettings* settings;

	settings = freerdp_settings_new(0);
	store = certificate_store_new(settings);
        if (!store)
        {
                fprintf(stderr, "Could not create certificate store!\n");
        }
	certificate_data_match(store, (rdpCertificateData*)Data);
	freerdp_settings_free(settings);

	return EXIT_SUCCESS;
}

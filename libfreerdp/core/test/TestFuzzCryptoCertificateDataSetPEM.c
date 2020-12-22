#include <stddef.h>
#include <stdint.h>
#include <freerdp/crypto/certificate.h>

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	char* buf = calloc(Size + 1, sizeof(char));
	rdpCertificateData* cert_data = certificate_data_new("somehost", 1234);
	if (!buf || !cert_data)
		goto cleanup;

	memcpy(buf, Data, Size);
	if (!certificate_data_set_pem(cert_data, buf))
	{
		goto cleanup;
	}

cleanup:
	certificate_data_free(cert_data);
	free(buf);

	return 0;
}

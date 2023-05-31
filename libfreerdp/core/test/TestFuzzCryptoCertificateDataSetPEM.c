#include <stddef.h>
#include <stdint.h>
#include <freerdp/crypto/certificate_store.h>

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	rdpCertificateData* data = NULL;
	char* pem = calloc(Size + 1, sizeof(char));
	if (pem == NULL)
		goto cleanup;
	memcpy(pem, Data, Size);

	data = freerdp_certificate_data_new_from_pem("somehost", 1234, pem, Size);
	if (!data)
		goto cleanup;

cleanup:
	freerdp_certificate_data_free(data);
	free(pem);

	return 0;
}

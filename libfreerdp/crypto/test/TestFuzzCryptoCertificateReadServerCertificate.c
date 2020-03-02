#include <freerdp/crypto/crypto.h>

extern int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	rdpCertificate* certificate = NULL;
	certificate_read_server_certificate(certificate, (BYTE*)Data, Size);

	return EXIT_SUCCESS;
}

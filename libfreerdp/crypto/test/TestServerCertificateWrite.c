#include <stdio.h>
#include <string.h>

#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/stream.h>
#include <freerdp/crypto/certificate.h>

#include "../certificate.h"

/* Write the certificate into a stream that starts with capacity bytes and
 * return the bytes written (including the dwVersion field). */
WINPR_ATTR_MALLOC(free, 1)
static void* test_write_cert(const rdpCertificate* cert, UINT32 version, size_t capacity,
                             size_t* plen)
{
	void* data = nullptr;
	wStream* s = Stream_New(nullptr, capacity);
	if (!s)
		return nullptr;

	const SSIZE_T len = freerdp_certificate_write_server_cert(cert, version, s);
	if (len <= 0)
		goto fail;

	*plen = Stream_GetPosition(s);
	data = malloc(*plen);
	if (data)
		memcpy(data, Stream_Buffer(s), *plen);

fail:
	Stream_Free(s, TRUE);
	return data;
}

WINPR_ATTR_NODISCARD
static BOOL test_version(const rdpCertificate* cert, UINT32 version)
{
	BOOL rc = FALSE;
	size_t smallLen = 0;
	size_t largeLen = 0;

	void* small = test_write_cert(cert, version, 16u, &smallLen);
	void* large = test_write_cert(cert, version, 0x10000u, &largeLen);

	if (!small || !large)
	{
		(void)fprintf(stderr, "failed to write server certificate version 0x%08" PRIx32 "\n",
		              version);
	}
	else if ((smallLen != largeLen) || (memcmp(small, large, smallLen) != 0))
	{
		(void)fprintf(stderr,
		              "server certificate version 0x%08" PRIx32
		              " depends on the initial stream capacity\n",
		              version);
	}
	else
		rc = TRUE;

	free(small);
	free(large);
	return rc;
}

int TestServerCertificateWrite(int argc, char* argv[])
{
	int rc = -1;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	rdpCertificate* cert =
	    freerdp_certificate_new_from_file(TEST_SOURCE_DIR "/rsa_pkcs1_sha256_cert.pem");
	if (!cert)
	{
		(void)fprintf(stderr, "failed to load test certificate\n");
		return -1;
	}

	if (!test_version(cert, CERT_TEMPORARILY_ISSUED | CERT_CHAIN_VERSION_1))
		goto fail;
	if (!test_version(cert, CERT_CHAIN_VERSION_2))
		goto fail;

	rc = 0;
fail:
	freerdp_certificate_free(cert);
	return rc;
}

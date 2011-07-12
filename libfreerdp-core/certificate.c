/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Certificate Handling
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "certificate.h"

X509_CERT_CHAIN* certificate_new_x509_certificate_chain(uint32 count)
{
	X509_CERT_CHAIN* x509_cert_chain;

	x509_cert_chain = (X509_CERT_CHAIN*) xmalloc(sizeof(X509_CERT_CHAIN));

	x509_cert_chain->count = count;
	x509_cert_chain->array = (CERT_BLOB*) xzalloc(sizeof(CERT_BLOB) * count);

	return x509_cert_chain;
}

void certificate_free_x509_certificate_chain(X509_CERT_CHAIN* x509_cert_chain)
{
	int i;

	if (x509_cert_chain == NULL)
		return;

	for (i = 0; i < x509_cert_chain->count; i++)
	{
		if (x509_cert_chain->array[i].data != NULL)
			xfree(x509_cert_chain->array[i].data);
	}

	xfree(x509_cert_chain);
}

void certificate_read_server_proprietary_certificate(rdpCertificate* certificate, STREAM* s)
{
	printf("Server Proprietary Certificate\n");
}

void certificate_read_server_x509_certificate_chain(rdpCertificate* certificate, STREAM* s)
{
	int i;
	uint32 certLength;
	uint32 numCertBlobs;

	printf("\nServer X.509 Certificate Chain\n");

	stream_read_uint32(s, numCertBlobs); /* numCertBlobs */

	certificate->x509_cert_chain = certificate_new_x509_certificate_chain(numCertBlobs);

	for (i = 0; i < numCertBlobs; i++)
	{
		stream_read_uint32(s, certLength);

		printf("\nX.509 Certificate #%d, length:%d", i + 1, certLength);

		if (numCertBlobs - i == 2)
			printf(", License Server Certificate\n");
		else if (numCertBlobs - i == 1)
			printf(", Terminal Server Certificate\n");
		else
			printf("\n");

		certificate->x509_cert_chain->array[i].data = (uint8*) xmalloc(certLength);
		stream_read(s, certificate->x509_cert_chain->array[i].data, certLength);
		certificate->x509_cert_chain->array[i].length = certLength;

		freerdp_hexdump(certificate->x509_cert_chain->array[i].data, certificate->x509_cert_chain->array[i].length);
	}
}

void certificate_read_server_certificate(rdpCertificate* certificate, uint8* server_cert, int length)
{
	STREAM* s;
	uint32 dwVersion;

	s = stream_new(0);
	s->p = s->data = server_cert;

	if (length < 1)
	{
		printf("null server certificate\n");
		return;
	}

	stream_read_uint32(s, dwVersion); /* dwVersion (4 bytes) */

	switch (dwVersion & CERT_CHAIN_VERSION_MASK)
	{
		case CERT_CHAIN_VERSION_1:
			certificate_read_server_proprietary_certificate(certificate, s);
			break;

		case CERT_CHAIN_VERSION_2:
			certificate_read_server_x509_certificate_chain(certificate, s);
			break;

		default:
			printf("invalid certificate chain version:%d\n", dwVersion & CERT_CHAIN_VERSION_MASK);
			break;
	}
}

/**
 * Instantiate new certificate module.\n
 * @param rdp RDP module
 * @return new certificate module
 */

rdpCertificate* certificate_new(rdpRdp* rdp)
{
	rdpCertificate* certificate;

	certificate = (rdpCertificate*) xzalloc(sizeof(rdpCertificate));

	if (certificate != NULL)
	{
		certificate->rdp = rdp;
		certificate->x509_cert_chain = NULL;
	}

	return certificate;
}

/**
 * Free certificate module.
 * @param certificate certificate module to be freed
 */

void certificate_free(rdpCertificate* certificate)
{
	if (certificate != NULL)
	{
		certificate_free_x509_certificate_chain(certificate->x509_cert_chain);
		xfree(certificate);
	}
}


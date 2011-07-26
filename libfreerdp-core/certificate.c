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

/**
 *
 * X.509 Certificate Structure
 *
 * Certificate ::= SEQUENCE
 * {
 * 	tbsCertificate			TBSCertificate,
 * 	signatureAlgorithm		AlgorithmIdentifier,
 * 	signatureValue			BIT_STRING
 * }
 *
 * TBSCertificate ::= SEQUENCE
 * {
 * 	version			[0]	EXPLICIT Version DEFAULT v1,
 * 	serialNumber			CertificateSerialNumber,
 * 	signature			AlgorithmIdentifier,
 * 	issuer				Name,
 * 	validity			Validity,
 * 	subject				Name,
 * 	subjectPublicKeyInfo		SubjectPublicKeyInfo,
 * 	issuerUniqueID		[1]	IMPLICIT UniqueIdentifier OPTIONAL,
 * 	subjectUniqueId		[2]	IMPLICIT UniqueIdentifier OPTIONAL,
 * 	extensions		[3]	EXPLICIT Extensions OPTIONAL
 * }
 *
 * Version ::= INTEGER { v1(0), v2(1), v3(2) }
 *
 * CertificateSerialNumber ::= INTEGER
 *
 * AlgorithmIdentifier ::= SEQUENCE
 * {
 * 	algorithm			OBJECT_IDENTIFIER,
 * 	parameters			ANY DEFINED BY algorithm OPTIONAL
 * }
 *
 * Name ::= CHOICE { RDNSequence }
 *
 * RDNSequence ::= SEQUENCE OF RelativeDistinguishedName
 *
 * RelativeDistinguishedName ::= SET OF AttributeTypeAndValue
 *
 * AttributeTypeAndValue ::= SEQUENCE
 * {
 * 	type				AttributeType,
 * 	value				AttributeValue
 * }
 *
 * AttributeType ::= OBJECT_IDENTIFIER
 *
 * AttributeValue ::= ANY DEFINED BY AttributeType
 *
 * Validity ::= SEQUENCE
 * {
 * 	notBefore			Time,
 * 	notAfter			Time
 * }
 *
 * Time ::= CHOICE
 * {
 * 	utcTime				UTCTime,
 * 	generalTime			GeneralizedTime
 * }
 *
 * UniqueIdentifier ::= BIT_STRING
 *
 * SubjectPublicKeyInfo ::= SEQUENCE
 * {
 * 	algorithm			AlgorithmIdentifier,
 * 	subjectPublicKey		BIT_STRING
 * }
 *
 * RSAPublicKey ::= SEQUENCE
 * {
 * 	modulus				INTEGER
 * 	publicExponent			INTEGER
 * }
 *
 * Extensions ::= SEQUENCE SIZE (1..MAX) OF Extension
 *
 * Extension ::= SEQUENCE
 * {
 * 	extnID				OBJECT_IDENTIFIER
 * 	critical			BOOLEAN DEFAULT FALSE,
 * 	extnValue			OCTET_STRING
 * }
 *
 */

/**
 * Read X.509 Certificate
 * @param certificate certificate module
 * @param cert X.509 certificate
 */

void certificate_read_x509_certificate(CERT_BLOB* cert, CERT_INFO* info)
{
	STREAM* s;
	int length;
	uint8 padding;
	uint32 version;
	int modulus_length;
	int exponent_length;

	s = stream_new(0);
	s->p = s->data = cert->data;

	ber_read_sequence_tag(s, &length); /* Certificate (SEQUENCE) */

	ber_read_sequence_tag(s, &length); /* TBSCertificate (SEQUENCE) */

	/* Explicit Contextual Tag [0] */
	ber_read_contextual_tag(s, 0, &length, True);
	ber_read_integer(s, &version); /* version (INTEGER) */
	version++;

	/* serialNumber */
	ber_read_integer(s, NULL); /* CertificateSerialNumber (INTEGER) */

	/* signature */
	ber_read_sequence_tag(s, &length); /* AlgorithmIdentifier (SEQUENCE) */
	stream_seek(s, length);

	/* issuer */
	ber_read_sequence_tag(s, &length); /* Name (SEQUENCE) */
	stream_seek(s, length);

	/* validity */
	ber_read_sequence_tag(s, &length); /* Validity (SEQUENCE) */
	stream_seek(s, length);

	/* subject */
	ber_read_sequence_tag(s, &length); /* Name (SEQUENCE) */
	stream_seek(s, length);

	/* subjectPublicKeyInfo */
	ber_read_sequence_tag(s, &length); /* SubjectPublicKeyInfo (SEQUENCE) */

	/* subjectPublicKeyInfo::AlgorithmIdentifier */
	ber_read_sequence_tag(s, &length); /* AlgorithmIdentifier (SEQUENCE) */
	stream_seek(s, length);

	/* subjectPublicKeyInfo::subjectPublicKey */
	ber_read_bit_string(s, &length, &padding); /* BIT_STRING */

	/* RSAPublicKey (SEQUENCE) */
	ber_read_sequence_tag(s, &length); /* SEQUENCE */

	ber_read_integer_length(s, &modulus_length); /* modulus (INTEGER) */

	/* skip zero padding, if any */
	do
	{
		stream_peek_uint8(s, padding);

		if (padding == 0)
		{
			stream_seek(s, 1);
			modulus_length--;
		}
	}
	while (padding == 0);

	freerdp_blob_alloc(&info->modulus, modulus_length);
	stream_read(s, info->modulus.data, modulus_length);

	ber_read_integer_length(s, &exponent_length); /* publicExponent (INTEGER) */
	stream_read(s, &info->exponent[4 - exponent_length], exponent_length);
}

/**
 * Instantiate new X.509 Certificate Chain.
 * @param count certificate chain count
 * @return new X.509 certificate chain
 */

X509_CERT_CHAIN* certificate_new_x509_certificate_chain(uint32 count)
{
	X509_CERT_CHAIN* x509_cert_chain;

	x509_cert_chain = (X509_CERT_CHAIN*) xmalloc(sizeof(X509_CERT_CHAIN));

	x509_cert_chain->count = count;
	x509_cert_chain->array = (CERT_BLOB*) xzalloc(sizeof(CERT_BLOB) * count);

	return x509_cert_chain;
}

/**
 * Free X.509 Certificate Chain.
 * @param x509_cert_chain X.509 certificate chain to be freed
 */

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

/**
 * Read a Server Proprietary Certificate.\n
 * @param certificate certificate module
 * @param s stream
 */

void certificate_read_server_proprietary_certificate(rdpCertificate* certificate, STREAM* s)
{
	printf("Server Proprietary Certificate\n");
}

/**
 * Read an X.509 Certificate Chain.\n
 * @param certificate certificate module
 * @param s stream
 */

void certificate_read_server_x509_certificate_chain(rdpCertificate* certificate, STREAM* s)
{
	int i;
	uint32 certLength;
	uint32 numCertBlobs;

	DEBUG_CERTIFICATE("Server X.509 Certificate Chain");

	stream_read_uint32(s, numCertBlobs); /* numCertBlobs */

	certificate->x509_cert_chain = certificate_new_x509_certificate_chain(numCertBlobs);

	for (i = 0; i < numCertBlobs; i++)
	{
		stream_read_uint32(s, certLength);

		DEBUG_CERTIFICATE("\nX.509 Certificate #%d, length:%d", i + 1, certLength);

		certificate->x509_cert_chain->array[i].data = (uint8*) xmalloc(certLength);
		stream_read(s, certificate->x509_cert_chain->array[i].data, certLength);
		certificate->x509_cert_chain->array[i].length = certLength;

		if (numCertBlobs - i == 2)
		{
			CERT_INFO cert_info;
			DEBUG_CERTIFICATE("License Server Certificate");
			certificate_read_x509_certificate(&certificate->x509_cert_chain->array[i], &cert_info);
			DEBUG_LICENSE("modulus length:%d", cert_info.modulus.length);
		}
		else if (numCertBlobs - i == 1)
		{
			DEBUG_CERTIFICATE("Terminal Server Certificate");
			certificate_read_x509_certificate(&certificate->x509_cert_chain->array[i], &certificate->cert_info);
			DEBUG_CERTIFICATE("modulus length:%d", certificate->cert_info.modulus.length);
		}
	}
}

/**
 * Read a Server Certificate.\n
 * @param certificate certificate module
 * @param server_cert server certificate
 * @param length certificate length
 */

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


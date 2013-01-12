/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Certificate Handling
 *
 * Copyright 2011 Jiten Pathy
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <winpr/crt.h>

#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <freerdp/utils/file.h>

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

BOOL certificate_read_x509_certificate(rdpCertBlob* cert, rdpCertInfo* info)
{
	STREAM* s;
	int length;
	BYTE padding;
	UINT32 version;
	int modulus_length;
	int exponent_length;

	s = stream_new(0);
	stream_attach(s, cert->data, cert->length);

	if(!ber_read_sequence_tag(s, &length)) /* Certificate (SEQUENCE) */
		goto error1;

	if(!ber_read_sequence_tag(s, &length)) /* TBSCertificate (SEQUENCE) */
		goto error1;

	/* Explicit Contextual Tag [0] */
	if(!ber_read_contextual_tag(s, 0, &length, TRUE))
		goto error1;
	if(!ber_read_integer(s, &version)) /* version (INTEGER) */
		goto error1;
	version++;

	/* serialNumber */
	if(!ber_read_integer(s, NULL)) /* CertificateSerialNumber (INTEGER) */
		goto error1;

	/* signature */
	if(!ber_read_sequence_tag(s, &length) || stream_get_left(s) < length) /* AlgorithmIdentifier (SEQUENCE) */
		goto error1;
	stream_seek(s, length);

	/* issuer */
	if(!ber_read_sequence_tag(s, &length) || stream_get_left(s) < length) /* Name (SEQUENCE) */
		goto error1;
	stream_seek(s, length);

	/* validity */
	if(!ber_read_sequence_tag(s, &length) || stream_get_left(s) < length) /* Validity (SEQUENCE) */
		goto error1;
	stream_seek(s, length);

	/* subject */
	if(!ber_read_sequence_tag(s, &length) || stream_get_left(s) < length) /* Name (SEQUENCE) */
		goto error1;
	stream_seek(s, length);

	/* subjectPublicKeyInfo */
	if(!ber_read_sequence_tag(s, &length)) /* SubjectPublicKeyInfo (SEQUENCE) */
		goto error1;

	/* subjectPublicKeyInfo::AlgorithmIdentifier */
	if(!ber_read_sequence_tag(s, &length) || stream_get_left(s) < length) /* AlgorithmIdentifier (SEQUENCE) */
		goto error1;
	stream_seek(s, length);

	/* subjectPublicKeyInfo::subjectPublicKey */
	if(!ber_read_bit_string(s, &length, &padding)) /* BIT_STRING */
		goto error1;

	/* RSAPublicKey (SEQUENCE) */
	if(!ber_read_sequence_tag(s, &length)) /* SEQUENCE */
		goto error1;

	if(!ber_read_integer_length(s, &modulus_length)) /* modulus (INTEGER) */
		goto error1;

	/* skip zero padding, if any */
	do
	{
		if(stream_get_left(s) < padding)
			goto error1;
		stream_peek_BYTE(s, padding);

		if (padding == 0)
		{
			if(stream_get_left(s) < 1)
				goto error1;
			stream_seek(s, 1);
			modulus_length--;
		}
	}
	while (padding == 0);

	if(stream_get_left(s) < modulus_length)
		goto error1;
	info->ModulusLength = modulus_length;
	info->Modulus = (BYTE*) malloc(info->ModulusLength);
	stream_read(s, info->Modulus, info->ModulusLength);

	if(!ber_read_integer_length(s, &exponent_length)) /* publicExponent (INTEGER) */
		goto error2;
	if(stream_get_left(s) < exponent_length)
		goto error2;
	stream_read(s, &info->exponent[4 - exponent_length], exponent_length);
	crypto_reverse(info->Modulus, info->ModulusLength);
	crypto_reverse(info->exponent, 4);

	stream_detach(s);
	stream_free(s);
	return TRUE;

error2:
	free(info->Modulus);
error1:
	stream_detach(s);
	stream_free(s);
	return FALSE;
}

/**
 * Instantiate new X.509 Certificate Chain.
 * @param count certificate chain count
 * @return new X.509 certificate chain
 */

rdpX509CertChain* certificate_new_x509_certificate_chain(UINT32 count)
{
	rdpX509CertChain* x509_cert_chain;

	x509_cert_chain = (rdpX509CertChain*) malloc(sizeof(rdpX509CertChain));

	x509_cert_chain->count = count;
	x509_cert_chain->array = (rdpCertBlob*) malloc(sizeof(rdpCertBlob) * count);
	ZeroMemory(x509_cert_chain->array, sizeof(rdpCertBlob) * count);

	return x509_cert_chain;
}

/**
 * Free X.509 Certificate Chain.
 * @param x509_cert_chain X.509 certificate chain to be freed
 */

void certificate_free_x509_certificate_chain(rdpX509CertChain* x509_cert_chain)
{
	int i;

	if (x509_cert_chain != NULL)
	{
		for (i = 0; i < (int) x509_cert_chain->count; i++)
		{
			if (x509_cert_chain->array[i].data != NULL)
				free(x509_cert_chain->array[i].data);
		}

		free(x509_cert_chain->array);
		free(x509_cert_chain);
	}
}

static BOOL certificate_process_server_public_key(rdpCertificate* certificate, STREAM* s, UINT32 length)
{
	BYTE magic[4];
	UINT32 keylen;
	UINT32 bitlen;
	UINT32 datalen;
	UINT32 modlen;

	if(stream_get_left(s) < 20)
		return FALSE;
	stream_read(s, magic, 4);

	if (memcmp(magic, "RSA1", 4) != 0)
	{
		printf("gcc_process_server_public_key: magic error\n");
		return FALSE;
	}

	stream_read_UINT32(s, keylen);
	stream_read_UINT32(s, bitlen);
	stream_read_UINT32(s, datalen);
	stream_read(s, certificate->cert_info.exponent, 4);
	modlen = keylen - 8;

	if(stream_get_left(s) < modlen + 8)
		return FALSE;
	certificate->cert_info.ModulusLength = modlen;
	certificate->cert_info.Modulus = malloc(certificate->cert_info.ModulusLength);
	stream_read(s, certificate->cert_info.Modulus, certificate->cert_info.ModulusLength);
	/* 8 bytes of zero padding */
	stream_seek(s, 8);

	return TRUE;
}

static BOOL certificate_process_server_public_signature(rdpCertificate* certificate, BYTE* sigdata, int sigdatalen, STREAM* s, UINT32 siglen)
{
	int i, sum;
	CryptoMd5 md5ctx;
	BYTE sig[TSSK_KEY_LENGTH];
	BYTE encsig[TSSK_KEY_LENGTH + 8];
	BYTE md5hash[CRYPTO_MD5_DIGEST_LENGTH];

	md5ctx = crypto_md5_init();
	crypto_md5_update(md5ctx, sigdata, sigdatalen);
	crypto_md5_final(md5ctx, md5hash);

	stream_read(s, encsig, siglen);

	/* Last 8 bytes shall be all zero. */

	for (sum = 0, i = sizeof(encsig) - 8; i < sizeof(encsig); i++)
		sum += encsig[i];

	if (sum != 0)
	{
		printf("certificate_process_server_public_signature: invalid signature\n");
		//return FALSE;
	}

	siglen -= 8;

	crypto_rsa_public_decrypt(encsig, siglen, TSSK_KEY_LENGTH, tssk_modulus, tssk_exponent, sig);

	/* Verify signature. */
	if (memcmp(md5hash, sig, sizeof(md5hash)) != 0)
	{
		printf("certificate_process_server_public_signature: invalid signature\n");
		//return FALSE;
	}

	/*
	 * Verify rest of decrypted data:
	 * The 17th byte is 0x00.
	 * The 18th through 62nd bytes are each 0xFF.
	 * The 63rd byte is 0x01.
	 */

	for (sum = 0, i = 17; i < 62; i++)
		sum += sig[i];

	if (sig[16] != 0x00 || sum != 0xFF * (62 - 17) || sig[62] != 0x01)
	{
		printf("certificate_process_server_public_signature: invalid signature\n");
		//return FALSE;
	}

	return TRUE;
}

/**
 * Read a Server Proprietary Certificate.\n
 * @param certificate certificate module
 * @param s stream
 */

BOOL certificate_read_server_proprietary_certificate(rdpCertificate* certificate, STREAM* s)
{
	UINT32 dwSigAlgId;
	UINT32 dwKeyAlgId;
	UINT32 wPublicKeyBlobType;
	UINT32 wPublicKeyBlobLen;
	UINT32 wSignatureBlobType;
	UINT32 wSignatureBlobLen;
	BYTE* sigdata;
	int sigdatalen;

	if(stream_get_left(s) < 12)
		return FALSE;

	/* -4, because we need to include dwVersion */
	sigdata = stream_get_tail(s) - 4;
	stream_read_UINT32(s, dwSigAlgId);
	stream_read_UINT32(s, dwKeyAlgId);

	if (!(dwSigAlgId == SIGNATURE_ALG_RSA && dwKeyAlgId == KEY_EXCHANGE_ALG_RSA))
	{
		printf("certificate_read_server_proprietary_certificate: parse error 1\n");
		return FALSE;
	}

	stream_read_UINT16(s, wPublicKeyBlobType);

	if (wPublicKeyBlobType != BB_RSA_KEY_BLOB)
	{
		printf("certificate_read_server_proprietary_certificate: parse error 2\n");
		return FALSE;
	}

	stream_read_UINT16(s, wPublicKeyBlobLen);
	if(stream_get_left(s) < wPublicKeyBlobLen)
		return FALSE;

	if (!certificate_process_server_public_key(certificate, s, wPublicKeyBlobLen))
	{
		printf("certificate_read_server_proprietary_certificate: parse error 3\n");
		return FALSE;
	}

	if(stream_get_left(s) < 4)
		return FALSE;

	sigdatalen = stream_get_tail(s) - sigdata;
	stream_read_UINT16(s, wSignatureBlobType);

	if (wSignatureBlobType != BB_RSA_SIGNATURE_BLOB)
	{
		printf("certificate_read_server_proprietary_certificate: parse error 4\n");
		return FALSE;
	}

	stream_read_UINT16(s, wSignatureBlobLen);
	if(stream_get_left(s) < wSignatureBlobLen)
		return FALSE;

	if (wSignatureBlobLen != 72)
	{
		printf("certificate_process_server_public_signature: invalid signature length (got %d, expected %d)\n", wSignatureBlobLen, 64);
		return FALSE;
	}

	if (!certificate_process_server_public_signature(certificate, sigdata, sigdatalen, s, wSignatureBlobLen))
	{
		printf("certificate_read_server_proprietary_certificate: parse error 5\n");
		return FALSE;
	}

	return TRUE;
}

/**
 * Read an X.509 Certificate Chain.\n
 * @param certificate certificate module
 * @param s stream
 */

BOOL certificate_read_server_x509_certificate_chain(rdpCertificate* certificate, STREAM* s)
{
	int i;
	UINT32 certLength;
	UINT32 numCertBlobs;

	DEBUG_CERTIFICATE("Server X.509 Certificate Chain");

	if(stream_get_left(s) < 4)
		return FALSE;
	stream_read_UINT32(s, numCertBlobs); /* numCertBlobs */

	certificate->x509_cert_chain = certificate_new_x509_certificate_chain(numCertBlobs);

	for (i = 0; i < (int) numCertBlobs; i++)
	{
		if(stream_get_left(s) < 4)
			return FALSE;
		stream_read_UINT32(s, certLength);
		if(stream_get_left(s) < certLength)
			return FALSE;

		DEBUG_CERTIFICATE("\nX.509 Certificate #%d, length:%d", i + 1, certLength);

		certificate->x509_cert_chain->array[i].data = (BYTE*) malloc(certLength);
		stream_read(s, certificate->x509_cert_chain->array[i].data, certLength);
		certificate->x509_cert_chain->array[i].length = certLength;

		if (numCertBlobs - i == 2)
		{
			rdpCertInfo cert_info;
			DEBUG_CERTIFICATE("License Server Certificate");
			certificate_read_x509_certificate(&certificate->x509_cert_chain->array[i], &cert_info);
			DEBUG_LICENSE("modulus length:%d", (int) cert_info.ModulusLength);
			free(cert_info.Modulus);
		}
		else if (numCertBlobs - i == 1)
		{
			DEBUG_CERTIFICATE("Terminal Server Certificate");
			certificate_read_x509_certificate(&certificate->x509_cert_chain->array[i], &certificate->cert_info);
			DEBUG_CERTIFICATE("modulus length:%d", (int) certificate->cert_info.ModulusLength);
		}
	}

	return TRUE;
}

/**
 * Read a Server Certificate.\n
 * @param certificate certificate module
 * @param server_cert server certificate
 * @param length certificate length
 */

BOOL certificate_read_server_certificate(rdpCertificate* certificate, BYTE* server_cert, int length)
{
	STREAM* s;
	UINT32 dwVersion;
	BOOL ret = TRUE;

	if (length < 1)
	{
		DEBUG_CERTIFICATE("null server certificate\n");
		return FALSE;
	}

	if (length < 4)
		return FALSE;

	s = stream_new(0);
	stream_attach(s, server_cert, length);

	stream_read_UINT32(s, dwVersion); /* dwVersion (4 bytes) */

	switch (dwVersion & CERT_CHAIN_VERSION_MASK)
	{
		case CERT_CHAIN_VERSION_1:
			ret = certificate_read_server_proprietary_certificate(certificate, s);
			break;

		case CERT_CHAIN_VERSION_2:
			ret = certificate_read_server_x509_certificate_chain(certificate, s);
			break;

		default:
			printf("invalid certificate chain version:%d\n", dwVersion & CERT_CHAIN_VERSION_MASK);
			break;
	}

	free(s);
	return ret;
}

rdpRsaKey* key_new(const char* keyfile)
{
	FILE* fp;
	RSA* rsa;
	rdpRsaKey* key;

	key = (rdpRsaKey*) malloc(sizeof(rdpRsaKey));
	ZeroMemory(key, sizeof(rdpRsaKey));

	if (key == NULL)
		return NULL;

	fp = fopen(keyfile, "r");

	if (fp == NULL)
	{
		printf("unable to load RSA key from %s: %s.", keyfile, strerror(errno));
		free(key) ;
		return NULL;
	}

	rsa = PEM_read_RSAPrivateKey(fp, NULL, NULL, NULL);

	if (rsa == NULL)
	{
		ERR_print_errors_fp(stdout);
		fclose(fp);
		free(key) ;
		return NULL;
	}

	fclose(fp);

	switch (RSA_check_key(rsa))
	{
		case 0:
			RSA_free(rsa);
			printf("invalid RSA key in %s", keyfile);
			free(key) ;
			return NULL;

		case 1:
			/* Valid key. */
			break;

		default:
			ERR_print_errors_fp(stdout);
			RSA_free(rsa);
			free(key) ;
			return NULL;
	}

	if (BN_num_bytes(rsa->e) > 4)
	{
		RSA_free(rsa);
		printf("RSA public exponent too large in %s", keyfile);
		free(key) ;
		return NULL;
	}

	key->ModulusLength = BN_num_bytes(rsa->n);
	key->Modulus = (BYTE*) malloc(key->ModulusLength);
	BN_bn2bin(rsa->n, key->Modulus);
	crypto_reverse(key->Modulus, key->ModulusLength);

	key->PrivateExponentLength = BN_num_bytes(rsa->d);
	key->PrivateExponent = (BYTE*) malloc(key->PrivateExponentLength);
	BN_bn2bin(rsa->d, key->PrivateExponent);
	crypto_reverse(key->PrivateExponent, key->PrivateExponentLength);

	memset(key->exponent, 0, sizeof(key->exponent));
	BN_bn2bin(rsa->e, key->exponent + sizeof(key->exponent) - BN_num_bytes(rsa->e));
	crypto_reverse(key->exponent, sizeof(key->exponent));

	RSA_free(rsa);

	return key;
}

void key_free(rdpRsaKey* key)
{
	if (key != NULL)
	{
		free(key->Modulus);
		free(key->PrivateExponent);
		free(key);
	}
}

/**
 * Instantiate new certificate module.\n
 * @param rdp RDP module
 * @return new certificate module
 */

rdpCertificate* certificate_new()
{
	rdpCertificate* certificate;

	certificate = (rdpCertificate*) malloc(sizeof(rdpCertificate));
	ZeroMemory(certificate, sizeof(rdpCertificate));

	if (certificate != NULL)
	{
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

		if (certificate->cert_info.Modulus != NULL)
			free(certificate->cert_info.Modulus);

		free(certificate);
	}
}

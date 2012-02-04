/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <freerdp/utils/file.h>

static const char certificate_store_dir[] = "certs";
static const char certificate_known_hosts_file[] = "known_hosts";

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

void certificate_read_x509_certificate(rdpCertBlob* cert, rdpCertInfo* info)
{
	STREAM* s;
	int length;
	uint8 padding;
	uint32 version;
	int modulus_length;
	int exponent_length;

	s = stream_new(0);
	stream_attach(s, cert->data, cert->length);

	ber_read_sequence_tag(s, &length); /* Certificate (SEQUENCE) */

	ber_read_sequence_tag(s, &length); /* TBSCertificate (SEQUENCE) */

	/* Explicit Contextual Tag [0] */
	ber_read_contextual_tag(s, 0, &length, true);
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
	crypto_reverse(info->modulus.data, modulus_length);
	crypto_reverse(info->exponent, 4);

	stream_detach(s);
	stream_free(s);
}

/**
 * Instantiate new X.509 Certificate Chain.
 * @param count certificate chain count
 * @return new X.509 certificate chain
 */

rdpX509CertChain* certificate_new_x509_certificate_chain(uint32 count)
{
	rdpX509CertChain* x509_cert_chain;

	x509_cert_chain = (rdpX509CertChain*) xmalloc(sizeof(rdpX509CertChain));

	x509_cert_chain->count = count;
	x509_cert_chain->array = (rdpCertBlob*) xzalloc(sizeof(rdpCertBlob) * count);

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
				xfree(x509_cert_chain->array[i].data);
		}

		xfree(x509_cert_chain->array);
		xfree(x509_cert_chain);
	}
}

static boolean certificate_process_server_public_key(rdpCertificate* certificate, STREAM* s, uint32 length)
{
	uint8 magic[4];
	uint32 keylen;
	uint32 bitlen;
	uint32 datalen;
	uint32 modlen;

	stream_read(s, magic, 4);
	if (memcmp(magic, "RSA1", 4) != 0)
	{
		printf("gcc_process_server_public_key: magic error\n");
		return false;
	}

	stream_read_uint32(s, keylen);
	stream_read_uint32(s, bitlen);
	stream_read_uint32(s, datalen);
	stream_read(s, certificate->cert_info.exponent, 4);
	modlen = keylen - 8;
	freerdp_blob_alloc(&(certificate->cert_info.modulus), modlen);
	stream_read(s, certificate->cert_info.modulus.data, modlen);
	/* 8 bytes of zero padding */
	stream_seek(s, 8);

	return true;
}

static boolean certificate_process_server_public_signature(rdpCertificate* certificate, uint8* sigdata, int sigdatalen, STREAM* s, uint32 siglen)
{
	uint8 md5hash[CRYPTO_MD5_DIGEST_LENGTH];
	uint8 encsig[TSSK_KEY_LENGTH + 8];
	uint8 sig[TSSK_KEY_LENGTH];
	CryptoMd5 md5ctx;
	int i, sum;

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
		//return false;
	}

	siglen -= 8;

	crypto_rsa_public_decrypt(encsig, siglen, TSSK_KEY_LENGTH, tssk_modulus, tssk_exponent, sig);

	/* Verify signature. */
	if (memcmp(md5hash, sig, sizeof(md5hash)) != 0)
	{
		printf("certificate_process_server_public_signature: invalid signature\n");
		//return false;
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
		//return false;
	}

	return true;
}

/**
 * Read a Server Proprietary Certificate.\n
 * @param certificate certificate module
 * @param s stream
 */

boolean certificate_read_server_proprietary_certificate(rdpCertificate* certificate, STREAM* s)
{
	uint32 dwSigAlgId;
	uint32 dwKeyAlgId;
	uint32 wPublicKeyBlobType;
	uint32 wPublicKeyBlobLen;
	uint32 wSignatureBlobType;
	uint32 wSignatureBlobLen;
	uint8* sigdata;
	int sigdatalen;

	/* -4, because we need to include dwVersion */
	sigdata = stream_get_tail(s) - 4;
	stream_read_uint32(s, dwSigAlgId);
	stream_read_uint32(s, dwKeyAlgId);
	if (!(dwSigAlgId == SIGNATURE_ALG_RSA && dwKeyAlgId == KEY_EXCHANGE_ALG_RSA))
	{
		printf("certificate_read_server_proprietary_certificate: parse error 1\n");
		return false;
	}
	stream_read_uint16(s, wPublicKeyBlobType);
	if (wPublicKeyBlobType != BB_RSA_KEY_BLOB)
	{
		printf("certificate_read_server_proprietary_certificate: parse error 2\n");
		return false;
	}
	stream_read_uint16(s, wPublicKeyBlobLen);
	if (!certificate_process_server_public_key(certificate, s, wPublicKeyBlobLen))
	{
		printf("certificate_read_server_proprietary_certificate: parse error 3\n");
		return false;
	}
	sigdatalen = stream_get_tail(s) - sigdata;
	stream_read_uint16(s, wSignatureBlobType);
	if (wSignatureBlobType != BB_RSA_SIGNATURE_BLOB)
	{
		printf("certificate_read_server_proprietary_certificate: parse error 4\n");
		return false;
	}
	stream_read_uint16(s, wSignatureBlobLen);
	if (wSignatureBlobLen != 72)
	{
		printf("certificate_process_server_public_signature: invalid signature length (got %d, expected %d)\n", wSignatureBlobLen, 64);
		return false;
	}
	if (!certificate_process_server_public_signature(certificate, sigdata, sigdatalen, s, wSignatureBlobLen))
	{
		printf("certificate_read_server_proprietary_certificate: parse error 5\n");
		return false;
	}

	return true;
}

/**
 * Read an X.509 Certificate Chain.\n
 * @param certificate certificate module
 * @param s stream
 */

boolean certificate_read_server_x509_certificate_chain(rdpCertificate* certificate, STREAM* s)
{
	int i;
	uint32 certLength;
	uint32 numCertBlobs;

	DEBUG_CERTIFICATE("Server X.509 Certificate Chain");

	stream_read_uint32(s, numCertBlobs); /* numCertBlobs */

	certificate->x509_cert_chain = certificate_new_x509_certificate_chain(numCertBlobs);

	for (i = 0; i < (int) numCertBlobs; i++)
	{
		stream_read_uint32(s, certLength);

		DEBUG_CERTIFICATE("\nX.509 Certificate #%d, length:%d", i + 1, certLength);

		certificate->x509_cert_chain->array[i].data = (uint8*) xmalloc(certLength);
		stream_read(s, certificate->x509_cert_chain->array[i].data, certLength);
		certificate->x509_cert_chain->array[i].length = certLength;

		if (numCertBlobs - i == 2)
		{
			rdpCertInfo cert_info;
			DEBUG_CERTIFICATE("License Server Certificate");
			certificate_read_x509_certificate(&certificate->x509_cert_chain->array[i], &cert_info);
			DEBUG_LICENSE("modulus length:%d", cert_info.modulus.length);
			freerdp_blob_free(&cert_info.modulus);
		}
		else if (numCertBlobs - i == 1)
		{
			DEBUG_CERTIFICATE("Terminal Server Certificate");
			certificate_read_x509_certificate(&certificate->x509_cert_chain->array[i], &certificate->cert_info);
			DEBUG_CERTIFICATE("modulus length:%d", certificate->cert_info.modulus.length);
		}
	}

	return true;
}

/**
 * Read a Server Certificate.\n
 * @param certificate certificate module
 * @param server_cert server certificate
 * @param length certificate length
 */

boolean certificate_read_server_certificate(rdpCertificate* certificate, uint8* server_cert, int length)
{
	STREAM* s;
	uint32 dwVersion;

	s = stream_new(0);
	stream_attach(s, server_cert, length);

	if (length < 1)
	{
		printf("null server certificate\n");
		return false;
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

	xfree(s);
	return true;
}

rdpKey* key_new(const char* keyfile)
{
	rdpKey* key;
	RSA *rsa;
	FILE *fp;

	key = (rdpKey*) xzalloc(sizeof(rdpKey));

	if (key == NULL)
		return NULL;

	fp = fopen(keyfile, "r");

	if (fp == NULL)
	{
		printf("unable to load RSA key from %s: %s.", keyfile, strerror(errno));
		return NULL;
	}

	rsa = PEM_read_RSAPrivateKey(fp, NULL, NULL, NULL);

	if (rsa == NULL)
	{
		ERR_print_errors_fp(stdout);
		fclose(fp);
		return NULL;
	}

	fclose(fp);

	switch (RSA_check_key(rsa))
	{
		case 0:
			RSA_free(rsa);
			printf("invalid RSA key in %s", keyfile);
			return NULL;

		case 1:
			/* Valid key. */
			break;

		default:
			ERR_print_errors_fp(stdout);
			RSA_free(rsa);
			return NULL;
	}

	if (BN_num_bytes(rsa->e) > 4)
	{
		RSA_free(rsa);
		printf("RSA public exponent too large in %s", keyfile);
		return NULL;
	}

	freerdp_blob_alloc(&key->modulus, BN_num_bytes(rsa->n));
	BN_bn2bin(rsa->n, key->modulus.data);
	crypto_reverse(key->modulus.data, key->modulus.length);
	freerdp_blob_alloc(&key->private_exponent, BN_num_bytes(rsa->d));
	BN_bn2bin(rsa->d, key->private_exponent.data);
	crypto_reverse(key->private_exponent.data, key->private_exponent.length);
	memset(key->exponent, 0, sizeof(key->exponent));
	BN_bn2bin(rsa->e, key->exponent + sizeof(key->exponent) - BN_num_bytes(rsa->e));
	crypto_reverse(key->exponent, sizeof(key->exponent));

	RSA_free(rsa);

	return key;
}

void key_free(rdpKey* key)
{
	if (key != NULL)
	{
		freerdp_blob_free(&key->modulus);
		freerdp_blob_free(&key->private_exponent);
		xfree(key);
	}
}

void certificate_store_init(rdpCertificateStore* certificate_store)
{
	char* config_path;
	rdpSettings* settings;

	settings = certificate_store->settings;

	config_path = freerdp_get_config_path(settings);
	certificate_store->path = freerdp_construct_path(config_path, (char*) certificate_store_dir);

	if (freerdp_check_file_exists(certificate_store->path) == false)
	{
		freerdp_mkdir(certificate_store->path);
		printf("creating directory %s\n", certificate_store->path);
	}

	certificate_store->file = freerdp_construct_path(config_path, (char*) certificate_known_hosts_file);

	if (freerdp_check_file_exists(certificate_store->file) == false)
	{
		certificate_store->fp = fopen((char*) certificate_store->file, "w+");

		if (certificate_store->fp == NULL)
		{
			printf("certificate_store_open: error opening [%s] for writing\n", certificate_store->file);
			return;
		}

		fflush(certificate_store->fp);
	}
	else
	{
		certificate_store->fp = fopen((char*) certificate_store->file, "r+");
	}
}

int certificate_data_match(rdpCertificateStore* certificate_store, rdpCertificateData* certificate_data)
{
	FILE* fp;
	int length;
	char* data;
	char* pline;
	int match = 1;
	long int size;

	fp = certificate_store->fp;

	if (!fp)
		return match;

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (size < 1)
		return match;

	data = (char*) xmalloc(size + 2);

	if (fread(data, size, 1, fp) != 1)
	{
		xfree(data);
		return match;
	}

	data[size] = '\n';
	data[size + 1] = '\0';
	pline = strtok(data, "\n");

	while (pline != NULL)
	{
		length = strlen(pline);

		if (length > 0)
		{
			length = strcspn(pline, " \t");
			pline[length] = '\0';

			if (strcmp(pline, certificate_data->hostname) == 0)
			{
				pline = &pline[length + 1];

				if (strcmp(pline, certificate_data->fingerprint) == 0)
					match = 0;
				else
					match = -1;
				break;
			}
		}

		pline = strtok(NULL, "\n");
	}
	xfree(data);

	return match;
}

void certificate_data_print(rdpCertificateStore* certificate_store, rdpCertificateData* certificate_data)
{
	FILE* fp;

	/* reopen in append mode */
	fp = fopen(certificate_store->file, "a");

	if (!fp)
		return;

	fprintf(certificate_store->fp,"%s %s\n", certificate_data->hostname, certificate_data->fingerprint);
	fclose(fp);
}

rdpCertificateData* certificate_data_new(char* hostname, char* fingerprint)
{
	rdpCertificateData* certdata;

	certdata = (rdpCertificateData*) xzalloc(sizeof(rdpCertificateData));

	if (certdata != NULL)
	{
		certdata->hostname = xstrdup(hostname);
		certdata->fingerprint = xstrdup(fingerprint);
	}

	return certdata;
}

void certificate_data_free(rdpCertificateData* certificate_data)
{
	if (certificate_data != NULL)
	{
		xfree(certificate_data->hostname);
		xfree(certificate_data->fingerprint);
		xfree(certificate_data);
	}
}

rdpCertificateStore* certificate_store_new(rdpSettings* settings)
{
	rdpCertificateStore* certificate_store;

	certificate_store = (rdpCertificateStore*) xzalloc(sizeof(rdpCertificateStore));

	if (certificate_store != NULL)
	{
		certificate_store->settings = settings;
		certificate_store_init(certificate_store);
	}

	return certificate_store;
}

void certificate_store_free(rdpCertificateStore* certstore)
{
	if (certstore != NULL)
	{
		if (certstore->fp != NULL)
			fclose(certstore->fp);

		xfree(certstore->path);
		xfree(certstore->file);
		xfree(certstore);
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

	certificate = (rdpCertificate*) xzalloc(sizeof(rdpCertificate));

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

		if (certificate->cert_info.modulus.data != NULL)
			freerdp_blob_free(&(certificate->cert_info.modulus));

		xfree(certificate);
	}
}

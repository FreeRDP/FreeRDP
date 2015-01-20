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

#include "certificate.h"

#define TAG "com.freerdp.core"

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

static const char* certificate_read_errors[] =
{
	"Certificate tag",
	"TBSCertificate",
	"Explicit Contextual Tag [0]",
	"version",
	"CertificateSerialNumber",
	"AlgorithmIdentifier",
	"Issuer Name",
	"Validity",
	"Subject Name",
	"SubjectPublicKeyInfo Tag",
	"subjectPublicKeyInfo::AlgorithmIdentifier",
	"subjectPublicKeyInfo::subjectPublicKey",
	"RSAPublicKey Tag",
	"modulusLength",
	"zero padding",
	"modulusLength",
	"modulus",
	"publicExponent length",
	"publicExponent"
};


/**
 * Read X.509 Certificate
 * @param certificate certificate module
 * @param cert X.509 certificate
 */

BOOL certificate_read_x509_certificate(rdpCertBlob* cert, rdpCertInfo* info)
{
	wStream* s;
	int length;
	BYTE padding;
	UINT32 version;
	int modulus_length;
	int exponent_length;
	int error = 0;

	s = Stream_New(cert->data, cert->length);

	if (!s)
		return FALSE;

	info->Modulus = 0;

	if (!ber_read_sequence_tag(s, &length)) /* Certificate (SEQUENCE) */
		goto error1;

	error++;

	if (!ber_read_sequence_tag(s, &length)) /* TBSCertificate (SEQUENCE) */
		goto error1;

	error++;

	if (!ber_read_contextual_tag(s, 0, &length, TRUE))	/* Explicit Contextual Tag [0] */
		goto error1;

	error++;

	if (!ber_read_integer(s, &version)) /* version (INTEGER) */
		goto error1;

	error++;
	version++;

	/* serialNumber */
	if (!ber_read_integer(s, NULL)) /* CertificateSerialNumber (INTEGER) */
		goto error1;

	error++;

	/* signature */
	if (!ber_read_sequence_tag(s, &length) || !Stream_SafeSeek(s, length)) /* AlgorithmIdentifier (SEQUENCE) */
		goto error1;

	error++;

	/* issuer */
	if (!ber_read_sequence_tag(s, &length) || !Stream_SafeSeek(s, length)) /* Name (SEQUENCE) */
		goto error1;

	error++;

	/* validity */
	if (!ber_read_sequence_tag(s, &length) || !Stream_SafeSeek(s, length)) /* Validity (SEQUENCE) */
		goto error1;

	error++;

	/* subject */
	if (!ber_read_sequence_tag(s, &length) || !Stream_SafeSeek(s, length)) /* Name (SEQUENCE) */
		goto error1;

	error++;

	/* subjectPublicKeyInfo */
	if (!ber_read_sequence_tag(s, &length)) /* SubjectPublicKeyInfo (SEQUENCE) */
		goto error1;

	error++;

	/* subjectPublicKeyInfo::AlgorithmIdentifier */
	if (!ber_read_sequence_tag(s, &length) || !Stream_SafeSeek(s, length)) /* AlgorithmIdentifier (SEQUENCE) */
		goto error1;

	error++;

	/* subjectPublicKeyInfo::subjectPublicKey */
	if (!ber_read_bit_string(s, &length, &padding)) /* BIT_STRING */
		goto error1;

	error++;

	/* RSAPublicKey (SEQUENCE) */
	if (!ber_read_sequence_tag(s, &length)) /* SEQUENCE */
		goto error1;

	error++;

	if (!ber_read_integer_length(s, &modulus_length)) /* modulus (INTEGER) */
		goto error1;

	error++;

	/* skip zero padding, if any */
	do
	{
		if (Stream_GetRemainingLength(s) < 1)
			goto error1;

		Stream_Peek_UINT8(s, padding);

		if (padding == 0)
		{
			if (!Stream_SafeSeek(s, 1))
				goto error1;

			modulus_length--;
		}
	}
	while (padding == 0);

	error++;

	if (((int) Stream_GetRemainingLength(s)) < modulus_length)
		goto error1;

	info->ModulusLength = modulus_length;
	info->Modulus = (BYTE*) malloc(info->ModulusLength);

	if (!info->Modulus)
		goto error1;

	Stream_Read(s, info->Modulus, info->ModulusLength);
	error++;

	if (!ber_read_integer_length(s, &exponent_length)) /* publicExponent (INTEGER) */
		goto error2;

	error++;

	if ((((int) Stream_GetRemainingLength(s)) < exponent_length) || (exponent_length > 4))
		goto error2;

	Stream_Read(s, &info->exponent[4 - exponent_length], exponent_length);
	crypto_reverse(info->Modulus, info->ModulusLength);
	crypto_reverse(info->exponent, 4);
	Stream_Free(s, FALSE);
	return TRUE;
error2:
	free(info->Modulus);
	info->Modulus = 0;
error1:
	WLog_ERR(TAG, "error reading when reading certificate: part=%s error=%d", certificate_read_errors[error], error);
	Stream_Free(s, FALSE);
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

	if (!x509_cert_chain)
		return NULL;

	x509_cert_chain->count = count;
	x509_cert_chain->array = (rdpCertBlob*) calloc(count, sizeof(rdpCertBlob));

	if (!x509_cert_chain->array)
	{
		free(x509_cert_chain);
		return NULL;
	}

	return x509_cert_chain;
}

/**
 * Free X.509 Certificate Chain.
 * @param x509_cert_chain X.509 certificate chain to be freed
 */

void certificate_free_x509_certificate_chain(rdpX509CertChain* x509_cert_chain)
{
	int i;

	if (!x509_cert_chain)
		return;

	for (i = 0; i < (int)x509_cert_chain->count; i++)
	{
		if (x509_cert_chain->array[i].data)
			free(x509_cert_chain->array[i].data);
	}

	free(x509_cert_chain->array);
	free(x509_cert_chain);
}

static BOOL certificate_process_server_public_key(rdpCertificate* certificate, wStream* s, UINT32 length)
{
	BYTE magic[4];
	UINT32 keylen;
	UINT32 bitlen;
	UINT32 datalen;
	UINT32 modlen;

	if (Stream_GetRemainingLength(s) < 20)
		return FALSE;

	Stream_Read(s, magic, 4);

	if (memcmp(magic, "RSA1", 4) != 0)
	{
		WLog_ERR(TAG, "magic error");
		return FALSE;
	}

	Stream_Read_UINT32(s, keylen);
	Stream_Read_UINT32(s, bitlen);
	Stream_Read_UINT32(s, datalen);
	Stream_Read(s, certificate->cert_info.exponent, 4);
	modlen = keylen - 8;

	if (Stream_GetRemainingLength(s) < modlen + 8)	// count padding
		return FALSE;

	certificate->cert_info.ModulusLength = modlen;
	certificate->cert_info.Modulus = malloc(certificate->cert_info.ModulusLength);

	if (!certificate->cert_info.Modulus)
		return FALSE;

	Stream_Read(s, certificate->cert_info.Modulus, certificate->cert_info.ModulusLength);
	Stream_Seek(s, 8); /* 8 bytes of zero padding */

	return TRUE;
}

static BOOL certificate_process_server_public_signature(rdpCertificate* certificate,
		const BYTE* sigdata, int sigdatalen, wStream* s, UINT32 siglen)
{
	int i, sum;
	CryptoMd5 md5ctx;
	BYTE sig[TSSK_KEY_LENGTH];
	BYTE encsig[TSSK_KEY_LENGTH + 8];
	BYTE md5hash[CRYPTO_MD5_DIGEST_LENGTH];

	md5ctx = crypto_md5_init();

	if (!md5ctx)
		return FALSE;

	crypto_md5_update(md5ctx, sigdata, sigdatalen);
	crypto_md5_final(md5ctx, md5hash);
	Stream_Read(s, encsig, siglen);

	/* Last 8 bytes shall be all zero. */

	for (sum = 0, i = sizeof(encsig) - 8; i < sizeof(encsig); i++)
		sum += encsig[i];

	if (sum != 0)
	{
		WLog_ERR(TAG, "invalid signature");
		//return FALSE;
	}

	siglen -= 8;
	// TODO: check the result of decrypt
	crypto_rsa_public_decrypt(encsig, siglen, TSSK_KEY_LENGTH, tssk_modulus, tssk_exponent, sig);

	/* Verify signature. */
	if (memcmp(md5hash, sig, sizeof(md5hash)) != 0)
	{
		WLog_ERR(TAG, "invalid signature");
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
		WLog_ERR(TAG, "invalid signature");
		//return FALSE;
	}

	return TRUE;
}

/**
 * Read a Server Proprietary Certificate.\n
 * @param certificate certificate module
 * @param s stream
 */

BOOL certificate_read_server_proprietary_certificate(rdpCertificate* certificate, wStream* s)
{
	UINT32 dwSigAlgId;
	UINT32 dwKeyAlgId;
	UINT32 wPublicKeyBlobType;
	UINT32 wPublicKeyBlobLen;
	UINT32 wSignatureBlobType;
	UINT32 wSignatureBlobLen;
	BYTE* sigdata;
	int sigdatalen;

	if (Stream_GetRemainingLength(s) < 12)
		return FALSE;

	/* -4, because we need to include dwVersion */
	sigdata = Stream_Pointer(s) - 4;
	Stream_Read_UINT32(s, dwSigAlgId);
	Stream_Read_UINT32(s, dwKeyAlgId);

	if (!((dwSigAlgId == SIGNATURE_ALG_RSA) && (dwKeyAlgId == KEY_EXCHANGE_ALG_RSA)))
	{
		WLog_ERR(TAG, "unsupported signature or key algorithm, dwSigAlgId=%d dwKeyAlgId=%d",
				 dwSigAlgId, dwKeyAlgId);
		return FALSE;
	}

	Stream_Read_UINT16(s, wPublicKeyBlobType);

	if (wPublicKeyBlobType != BB_RSA_KEY_BLOB)
	{
		WLog_ERR(TAG, "unsupported public key blob type %d", wPublicKeyBlobType);
		return FALSE;
	}

	Stream_Read_UINT16(s, wPublicKeyBlobLen);

	if (Stream_GetRemainingLength(s) < wPublicKeyBlobLen)
		return FALSE;

	if (!certificate_process_server_public_key(certificate, s, wPublicKeyBlobLen))
	{
		WLog_ERR(TAG, "error in server public key");
		return FALSE;
	}

	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	sigdatalen = Stream_Pointer(s) - sigdata;
	Stream_Read_UINT16(s, wSignatureBlobType);

	if (wSignatureBlobType != BB_RSA_SIGNATURE_BLOB)
	{
		WLog_ERR(TAG, "unsupported blob signature %d", wSignatureBlobType);
		return FALSE;
	}

	Stream_Read_UINT16(s, wSignatureBlobLen);

	if (Stream_GetRemainingLength(s) < wSignatureBlobLen)
	{
		WLog_ERR(TAG, "not enought bytes for signature(len=%d)", wSignatureBlobLen);
		return FALSE;
	}

	if (wSignatureBlobLen != 72)
	{
		WLog_ERR(TAG, "invalid signature length (got %d, expected %d)", wSignatureBlobLen, 64);
		return FALSE;
	}

	if (!certificate_process_server_public_signature(certificate, sigdata, sigdatalen, s, wSignatureBlobLen))
	{
		WLog_ERR(TAG, "unable to parse server public signature");
		return FALSE;
	}

	return TRUE;
}

/**
 * Read an X.509 Certificate Chain.\n
 * @param certificate certificate module
 * @param s stream
 */

BOOL certificate_read_server_x509_certificate_chain(rdpCertificate* certificate, wStream* s)
{
	int i;
	BOOL ret;
	UINT32 certLength;
	UINT32 numCertBlobs;

	DEBUG_CERTIFICATE("Server X.509 Certificate Chain");

	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT32(s, numCertBlobs); /* numCertBlobs */
	certificate->x509_cert_chain = certificate_new_x509_certificate_chain(numCertBlobs);

	if (!certificate->x509_cert_chain)
		return FALSE;

	for (i = 0; i < (int) numCertBlobs; i++)
	{
		if (Stream_GetRemainingLength(s) < 4)
			return FALSE;

		Stream_Read_UINT32(s, certLength);

		if (Stream_GetRemainingLength(s) < certLength)
			return FALSE;

		DEBUG_CERTIFICATE("X.509 Certificate #%d, length:%d", i + 1, certLength);
		certificate->x509_cert_chain->array[i].data = (BYTE*) malloc(certLength);

		if (!certificate->x509_cert_chain->array[i].data)
			return FALSE;

		Stream_Read(s, certificate->x509_cert_chain->array[i].data, certLength);
		certificate->x509_cert_chain->array[i].length = certLength;

		if ((numCertBlobs - i) == 2)
		{
			rdpCertInfo cert_info;

			DEBUG_CERTIFICATE("License Server Certificate");
			
			ret = certificate_read_x509_certificate(&certificate->x509_cert_chain->array[i], &cert_info);
			
			DEBUG_LICENSE("modulus length:%d", (int) cert_info.ModulusLength);

			if (cert_info.Modulus)
				free(cert_info.Modulus);

			if (!ret)
			{
				WLog_ERR(TAG, "failed to read License Server, content follows:");
				winpr_HexDump(TAG, WLOG_ERROR, certificate->x509_cert_chain->array[i].data, certificate->x509_cert_chain->array[i].length);
				return FALSE;
			}
		}
		else if (numCertBlobs - i == 1)
		{
			DEBUG_CERTIFICATE("Terminal Server Certificate");

			if (!certificate_read_x509_certificate(&certificate->x509_cert_chain->array[i], &certificate->cert_info))
				return FALSE;

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
	BOOL ret;
	wStream* s;
	UINT32 dwVersion;

	if (length < 4)  /* NULL certificate is not an error see #1795 */
		return TRUE;

	s = Stream_New(server_cert, length);
	Stream_Read_UINT32(s, dwVersion); /* dwVersion (4 bytes) */

	switch (dwVersion & CERT_CHAIN_VERSION_MASK)
	{
		case CERT_CHAIN_VERSION_1:
			ret = certificate_read_server_proprietary_certificate(certificate, s);
			break;

		case CERT_CHAIN_VERSION_2:
			ret = certificate_read_server_x509_certificate_chain(certificate, s);
			break;

		default:
			WLog_ERR(TAG, "invalid certificate chain version:%d", dwVersion & CERT_CHAIN_VERSION_MASK);
			ret = FALSE;
			break;
	}

	Stream_Free(s, FALSE);

	return ret;
}

rdpRsaKey* key_new(const char* keyfile)
{
	BIO* bio = NULL;
	FILE* fp = NULL;
	RSA* rsa = NULL;
	int length;
	BYTE* buffer = NULL;
	rdpRsaKey* key = NULL;

	key = (rdpRsaKey*) calloc(1, sizeof(rdpRsaKey));

	if (!key)
		return NULL;

	fp = fopen(keyfile, "r+b");

	if (!fp)
	{
		WLog_ERR(TAG, "unable to open RSA key file %s: %s.", keyfile, strerror(errno));
		goto out_free;
	}

	fseek(fp, 0, SEEK_END);
	length = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	buffer = (BYTE*) malloc(length);

	if (!buffer)
		goto out_free;

	fread((void*) buffer, length, 1, fp);
	fclose(fp);

	bio = BIO_new_mem_buf((void*) buffer, length);

	if (!bio)
		goto out_free;

	rsa = PEM_read_bio_RSAPrivateKey(bio, NULL, NULL, NULL);

	BIO_free(bio);
	free(buffer);
	buffer = NULL;

	if (!rsa)
	{
		WLog_ERR(TAG, "unable to load RSA key from %s: %s.", keyfile, strerror(errno));
		goto out_free;
	}

	switch (RSA_check_key(rsa))
	{
		case 0:
			WLog_ERR(TAG, "invalid RSA key in %s", keyfile);
			goto out_free_rsa;

		case 1:
			/* Valid key. */
			break;

		default:
			WLog_ERR(TAG, "unexpected error when checking RSA key from %s: %s.", keyfile, strerror(errno));
			goto out_free_rsa;
	}

	if (BN_num_bytes(rsa->e) > 4)
	{
		WLog_ERR(TAG, "RSA public exponent too large in %s", keyfile);
		goto out_free_rsa;
	}

	key->ModulusLength = BN_num_bytes(rsa->n);
	key->Modulus = (BYTE*) malloc(key->ModulusLength);

	if (!key->Modulus)
		goto out_free_rsa;

	BN_bn2bin(rsa->n, key->Modulus);
	crypto_reverse(key->Modulus, key->ModulusLength);
	key->PrivateExponentLength = BN_num_bytes(rsa->d);
	key->PrivateExponent = (BYTE*) malloc(key->PrivateExponentLength);

	if (!key->PrivateExponent)
		goto out_free_modulus;

	BN_bn2bin(rsa->d, key->PrivateExponent);
	crypto_reverse(key->PrivateExponent, key->PrivateExponentLength);
	memset(key->exponent, 0, sizeof(key->exponent));
	BN_bn2bin(rsa->e, key->exponent + sizeof(key->exponent) - BN_num_bytes(rsa->e));
	crypto_reverse(key->exponent, sizeof(key->exponent));
	RSA_free(rsa);
	return key;
out_free_modulus:
	free(key->Modulus);
out_free_rsa:
	RSA_free(rsa);
out_free:
	if (fp)
		fclose(fp);
	if (buffer)
		free(buffer);
	free(key);
	return NULL;
}

void key_free(rdpRsaKey* key)
{
	if (!key)
		return;

	if (key->Modulus)
		free(key->Modulus);

	free(key->PrivateExponent);
	free(key);
}

rdpCertificate* certificate_clone(rdpCertificate* certificate)
{
	int index;
	rdpCertificate* _certificate = (rdpCertificate*) calloc(1, sizeof(rdpCertificate));

	if (!_certificate)
		return NULL;

	CopyMemory(_certificate, certificate, sizeof(rdpCertificate));

	if (certificate->cert_info.ModulusLength)
	{
		_certificate->cert_info.Modulus = (BYTE*) malloc(certificate->cert_info.ModulusLength);
		CopyMemory(_certificate->cert_info.Modulus, certificate->cert_info.Modulus, certificate->cert_info.ModulusLength);
		_certificate->cert_info.ModulusLength = certificate->cert_info.ModulusLength;
	}

	if (certificate->x509_cert_chain)
	{
		_certificate->x509_cert_chain = (rdpX509CertChain*) malloc(sizeof(rdpX509CertChain));
		CopyMemory(_certificate->x509_cert_chain, certificate->x509_cert_chain, sizeof(rdpX509CertChain));

		if (certificate->x509_cert_chain->count)
		{
			_certificate->x509_cert_chain->array = (rdpCertBlob*) calloc(certificate->x509_cert_chain->count, sizeof(rdpCertBlob));

			for (index = 0; index < certificate->x509_cert_chain->count; index++)
			{
				_certificate->x509_cert_chain->array[index].length = certificate->x509_cert_chain->array[index].length;

				if (certificate->x509_cert_chain->array[index].length)
				{
					_certificate->x509_cert_chain->array[index].data = (BYTE*) malloc(certificate->x509_cert_chain->array[index].length);
					CopyMemory(_certificate->x509_cert_chain->array[index].data, certificate->x509_cert_chain->array[index].data,
							_certificate->x509_cert_chain->array[index].length);
				}
			}
		}
	}

	return _certificate;
}

/**
 * Instantiate new certificate module.\n
 * @param rdp RDP module
 * @return new certificate module
 */

rdpCertificate* certificate_new()
{
	return (rdpCertificate*) calloc(1, sizeof(rdpCertificate));
}

/**
 * Free certificate module.
 * @param certificate certificate module to be freed
 */

void certificate_free(rdpCertificate* certificate)
{
	if (!certificate)
		return;

	certificate_free_x509_certificate_chain(certificate->x509_cert_chain);

	if (certificate->cert_info.Modulus)
		free(certificate->cert_info.Modulus);

	free(certificate);
}

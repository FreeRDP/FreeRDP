/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Certificate Handling
 *
 * Copyright 2011 Jiten Pathy
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#include <freerdp/config.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <winpr/assert.h>
#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/crypto.h>

#include <openssl/pem.h>
#include <openssl/rsa.h>

#include "certificate.h"
#include "../crypto/crypto.h"
#include "../crypto/opensslcompat.h"

#define TAG "com.freerdp.core"

#define TSSK_KEY_LENGTH 64

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

struct rdp_CertBlob
{
	UINT32 length;
	BYTE* data;
};
typedef struct rdp_CertBlob rdpCertBlob;

struct rdp_X509CertChain
{
	UINT32 count;
	rdpCertBlob* array;
};
typedef struct rdp_X509CertChain rdpX509CertChain;

struct rdp_certificate
{
	char* pem;
	size_t pem_length;

	rdpCertInfo cert_info;
	rdpX509CertChain x509_cert_chain;
};

struct rdp_rsa_key
{
	char* pem;
	size_t pem_length;

	rdpCertInfo cert;
	BYTE* PrivateExponent;
	DWORD PrivateExponentLength;
};

static const char rsa_magic[4] = "RSA1";

static const char* certificate_read_errors[] = { "Certificate tag",
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
	                                             "publicExponent" };

static const BYTE initial_signature[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01
};

/*
 * Terminal Services Signing Keys.
 * Yes, Terminal Services Private Key is publicly available.
 */

static BYTE tssk_modulus[] = { 0x3d, 0x3a, 0x5e, 0xbd, 0x72, 0x43, 0x3e, 0xc9, 0x4d, 0xbb, 0xc1,
	                           0x1e, 0x4a, 0xba, 0x5f, 0xcb, 0x3e, 0x88, 0x20, 0x87, 0xef, 0xf5,
	                           0xc1, 0xe2, 0xd7, 0xb7, 0x6b, 0x9a, 0xf2, 0x52, 0x45, 0x95, 0xce,
	                           0x63, 0x65, 0x6b, 0x58, 0x3a, 0xfe, 0xef, 0x7c, 0xe7, 0xbf, 0xfe,
	                           0x3d, 0xf6, 0x5c, 0x7d, 0x6c, 0x5e, 0x06, 0x09, 0x1a, 0xf5, 0x61,
	                           0xbb, 0x20, 0x93, 0x09, 0x5f, 0x05, 0x6d, 0xea, 0x87 };

static BYTE tssk_privateExponent[] = {
	0x87, 0xa7, 0x19, 0x32, 0xda, 0x11, 0x87, 0x55, 0x58, 0x00, 0x16, 0x16, 0x25, 0x65, 0x68, 0xf8,
	0x24, 0x3e, 0xe6, 0xfa, 0xe9, 0x67, 0x49, 0x94, 0xcf, 0x92, 0xcc, 0x33, 0x99, 0xe8, 0x08, 0x60,
	0x17, 0x9a, 0x12, 0x9f, 0x24, 0xdd, 0xb1, 0x24, 0x99, 0xc7, 0x3a, 0xb8, 0x0a, 0x7b, 0x0d, 0xdd,
	0x35, 0x07, 0x79, 0x17, 0x0b, 0x51, 0x9b, 0xb3, 0xc7, 0x10, 0x01, 0x13, 0xe7, 0x3f, 0xf3, 0x5f
};

static const rdpRsaKey tssk = { .PrivateExponent = tssk_privateExponent,
	                            .PrivateExponentLength = sizeof(tssk_privateExponent),
	                            .cert = { .Modulus = tssk_modulus,
	                                      .ModulusLength = sizeof(tssk_modulus) } };

#if defined(CERT_VALIDATE_RSA)
static const BYTE tssk_exponent[] = { 0x5b, 0x7b, 0x88, 0xc0 };
#endif

static void certificate_free_int(rdpCertificate* certificate);
static BOOL cert_clone_int(rdpCertificate* dst, const rdpCertificate* src);

static BOOL cert_info_create(rdpCertInfo* dst, const BIGNUM* rsa, const BIGNUM* rsa_e);
static BOOL cert_info_allocate(rdpCertInfo* info, size_t size);
static void cert_info_free(rdpCertInfo* info);
static BOOL cert_info_read_modulus(rdpCertInfo* info, size_t size, wStream* s);
static BOOL cert_info_read_exponent(rdpCertInfo* info, size_t size, wStream* s);

/* [MS-RDPBCGR] 5.3.3.2 X.509 Certificate Chains:
 *
 * More detail[MS-RDPELE] section 2.2.1.4.2.
 */
static BOOL cert_blob_copy(rdpCertBlob* dst, const rdpCertBlob* src);
static void cert_blob_free(rdpCertBlob* blob);
static BOOL cert_blob_write(const rdpCertBlob* blob, wStream* s);
static BOOL cert_blob_read(rdpCertBlob* blob, wStream* s);

BOOL cert_blob_read(rdpCertBlob* blob, wStream* s)
{
	UINT32 certLength = 0;
	WINPR_ASSERT(blob);
	cert_blob_free(blob);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, certLength);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, certLength))
		return FALSE;

	DEBUG_CERTIFICATE("X.509 Certificate #%" PRIu32 ", length:%" PRIu32 "", i + 1, certLength);
	blob->data = (BYTE*)malloc(certLength);

	if (!blob->data)
		return FALSE;

	Stream_Read(s, blob->data, certLength);
	blob->length = certLength;

	return TRUE;
}

BOOL cert_blob_write(const rdpCertBlob* blob, wStream* s)
{
	WINPR_ASSERT(blob);

	if (!Stream_EnsureRemainingCapacity(s, 4 + blob->length))
		return FALSE;

	Stream_Write_UINT32(s, blob->length);
	Stream_Write(s, blob->data, blob->length);
	return TRUE;
}

void cert_blob_free(rdpCertBlob* blob)
{
	if (!blob)
		return;
	free(blob->data);
	blob->data = NULL;
	blob->length = 0;
}

/**
 * Read X.509 Certificate
 */

static BOOL certificate_read_x509_certificate(rdpCertBlob* cert, rdpCertInfo* info)
{
	wStream sbuffer = { 0 };
	wStream* s = NULL;
	size_t length = 0;
	BYTE padding = 0;
	UINT32 version = 0;
	size_t modulus_length = 0;
	size_t exponent_length = 0;
	int error = 0;

	if (!cert || !info)
		return FALSE;

	cert_info_free(info);
	s = Stream_StaticConstInit(&sbuffer, cert->data, cert->length);

	if (!s)
		return FALSE;

	if (!ber_read_sequence_tag(s, &length)) /* Certificate (SEQUENCE) */
		goto error;

	error++;

	if (!ber_read_sequence_tag(s, &length)) /* TBSCertificate (SEQUENCE) */
		goto error;

	error++;

	if (!ber_read_contextual_tag(s, 0, &length, TRUE)) /* Explicit Contextual Tag [0] */
		goto error;

	error++;

	if (!ber_read_integer(s, &version)) /* version (INTEGER) */
		goto error;

	error++;
	version++;

	/* serialNumber */
	if (!ber_read_integer(s, NULL)) /* CertificateSerialNumber (INTEGER) */
		goto error;

	error++;

	/* signature */
	if (!ber_read_sequence_tag(s, &length) ||
	    !Stream_SafeSeek(s, length)) /* AlgorithmIdentifier (SEQUENCE) */
		goto error;

	error++;

	/* issuer */
	if (!ber_read_sequence_tag(s, &length) || !Stream_SafeSeek(s, length)) /* Name (SEQUENCE) */
		goto error;

	error++;

	/* validity */
	if (!ber_read_sequence_tag(s, &length) || !Stream_SafeSeek(s, length)) /* Validity (SEQUENCE) */
		goto error;

	error++;

	/* subject */
	if (!ber_read_sequence_tag(s, &length) || !Stream_SafeSeek(s, length)) /* Name (SEQUENCE) */
		goto error;

	error++;

	/* subjectPublicKeyInfo */
	if (!ber_read_sequence_tag(s, &length)) /* SubjectPublicKeyInfo (SEQUENCE) */
		goto error;

	error++;

	/* subjectPublicKeyInfo::AlgorithmIdentifier */
	if (!ber_read_sequence_tag(s, &length) ||
	    !Stream_SafeSeek(s, length)) /* AlgorithmIdentifier (SEQUENCE) */
		goto error;

	error++;

	/* subjectPublicKeyInfo::subjectPublicKey */
	if (!ber_read_bit_string(s, &length, &padding)) /* BIT_STRING */
		goto error;

	error++;

	/* RSAPublicKey (SEQUENCE) */
	if (!ber_read_sequence_tag(s, &length)) /* SEQUENCE */
		goto error;

	error++;

	if (!ber_read_integer_length(s, &modulus_length)) /* modulus (INTEGER) */
		goto error;

	error++;

	/* skip zero padding, if any */
	do
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
			goto error;

		Stream_Peek_UINT8(s, padding);

		if (padding == 0)
		{
			if (!Stream_SafeSeek(s, 1))
				goto error;

			modulus_length--;
		}
	} while (padding == 0);

	error++;

	if (!cert_info_read_modulus(info, modulus_length, s))
		goto error;

	error++;

	if (!ber_read_integer_length(s, &exponent_length)) /* publicExponent (INTEGER) */
		goto error;

	error++;

	if (!cert_info_read_exponent(info, exponent_length, s))
		goto error;
	return TRUE;
error:
	WLog_ERR(TAG, "error reading when reading certificate: part=%s error=%d",
	         certificate_read_errors[error], error);
	cert_info_free(info);
	return FALSE;
}

/**
 * Instantiate new X.509 Certificate Chain.
 * @param count certificate chain count
 * @return new X.509 certificate chain
 */

static rdpX509CertChain certificate_new_x509_certificate_chain(UINT32 count)
{
	rdpX509CertChain x509_cert_chain = { 0 };

	x509_cert_chain.array = (rdpCertBlob*)calloc(count, sizeof(rdpCertBlob));

	if (x509_cert_chain.array)
		x509_cert_chain.count = count;

	return x509_cert_chain;
}

/**
 * Free X.509 Certificate Chain.
 * @param x509_cert_chain X.509 certificate chain to be freed
 */

static void certificate_free_x509_certificate_chain(rdpX509CertChain* x509_cert_chain)
{
	if (!x509_cert_chain)
		return;

	if (x509_cert_chain->array)
	{
		for (UINT32 i = 0; i < x509_cert_chain->count; i++)
		{
			rdpCertBlob* element = &x509_cert_chain->array[i];
			cert_blob_free(element);
		}
	}

	free(x509_cert_chain->array);
}

static BOOL certificate_process_server_public_key(rdpCertificate* certificate, wStream* s,
                                                  UINT32 length)
{
	char magic[sizeof(rsa_magic)] = { 0 };
	UINT32 keylen = 0;
	UINT32 bitlen = 0;
	UINT32 datalen = 0;
	BYTE* tmp = NULL;

	WINPR_ASSERT(certificate);
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 20))
		return FALSE;

	Stream_Read(s, magic, sizeof(magic));

	if (memcmp(magic, rsa_magic, sizeof(magic)) != 0)
	{
		WLog_ERR(TAG, "magic error");
		return FALSE;
	}

	Stream_Read_UINT32(s, keylen);
	Stream_Read_UINT32(s, bitlen);
	Stream_Read_UINT32(s, datalen);
	Stream_Read(s, certificate->cert_info.exponent, 4);

	if ((keylen <= 8) || (!Stream_CheckAndLogRequiredLength(TAG, s, keylen)))
		return FALSE;

	certificate->cert_info.ModulusLength = keylen - 8;
	tmp = realloc(certificate->cert_info.Modulus, certificate->cert_info.ModulusLength);

	if (!tmp)
		return FALSE;
	certificate->cert_info.Modulus = tmp;

	Stream_Read(s, certificate->cert_info.Modulus, certificate->cert_info.ModulusLength);
	Stream_Seek(s, 8); /* 8 bytes of zero padding */
	return TRUE;
}

static BOOL certificate_process_server_public_signature(rdpCertificate* certificate,
                                                        const BYTE* sigdata, size_t sigdatalen,
                                                        wStream* s, UINT32 siglen)
{
	WINPR_ASSERT(certificate);
#if defined(CERT_VALIDATE_PADDING) || defined(CERT_VALIDATE_RSA)
	size_t i, sum;
#endif
#if defined(CERT_VALIDATE_RSA)
	BYTE sig[TSSK_KEY_LENGTH];
#endif
	BYTE encsig[TSSK_KEY_LENGTH + 8];
#if defined(CERT_VALIDATE_MD5) && defined(CERT_VALIDATE_RSA)
	BYTE md5hash[WINPR_MD5_DIGEST_LENGTH];
#endif
#if !defined(CERT_VALIDATE_MD5) || !defined(CERT_VALIDATE_RSA)
	(void)sigdata;
	(void)sigdatalen;
#endif
	(void)certificate;
	/* Do not bother with validation of server proprietary certificate. The use of MD5 here is not
	 * allowed under FIPS. Since the validation is not protecting against anything since the
	 * private/public keys are well known and documented in MS-RDPBCGR section 5.3.3.1, we are not
	 * gaining any security by using MD5 for signature comparison. Rather then use MD5
	 * here we just dont do the validation to avoid its use. Historically, freerdp has been ignoring
	 * a failed validation anyways. */
#if defined(CERT_VALIDATE_MD5)

	if (!winpr_Digest(WINPR_MD_MD5, sigdata, sigdatalen, md5hash, sizeof(md5hash)))
		return FALSE;

#endif
	Stream_Read(s, encsig, siglen);

	if (siglen < 8)
		return FALSE;

		/* Last 8 bytes shall be all zero. */
#if defined(CERT_VALIDATE_PADDING)

	for (sum = 0, i = sizeof(encsig) - 8; i < sizeof(encsig); i++)
		sum += encsig[i];

	if (sum != 0)
	{
		WLog_ERR(TAG, "invalid signature");
		return FALSE;
	}

#endif
#if defined(CERT_VALIDATE_RSA)

	if (crypto_rsa_public_decrypt(encsig, siglen - 8, TSSK_KEY_LENGTH, tssk_modulus, tssk_exponent,
	                              sig) <= 0)
	{
		WLog_ERR(TAG, "invalid RSA decrypt");
		return FALSE;
	}

	/* Verify signature. */
	/* Do not bother with validation of server proprietary certificate as described above. */
#if defined(CERT_VALIDATE_MD5)

	if (memcmp(md5hash, sig, sizeof(md5hash)) != 0)
	{
		WLog_ERR(TAG, "invalid signature");
		return FALSE;
	}

#endif
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
		return FALSE;
	}

#endif
	return TRUE;
}

static BOOL certificate_read_server_proprietary_certificate(rdpCertificate* certificate, wStream* s)
{
	UINT32 dwSigAlgId = 0;
	UINT32 dwKeyAlgId = 0;
	UINT16 wPublicKeyBlobType = 0;
	UINT16 wPublicKeyBlobLen = 0;
	UINT16 wSignatureBlobType = 0;
	UINT16 wSignatureBlobLen = 0;
	BYTE* sigdata = NULL;
	size_t sigdatalen = 0;

	WINPR_ASSERT(certificate);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return FALSE;

	/* -4, because we need to include dwVersion */
	sigdata = Stream_Pointer(s) - 4;
	Stream_Read_UINT32(s, dwSigAlgId);
	Stream_Read_UINT32(s, dwKeyAlgId);

	if (!((dwSigAlgId == SIGNATURE_ALG_RSA) && (dwKeyAlgId == KEY_EXCHANGE_ALG_RSA)))
	{
		WLog_ERR(TAG,
		         "unsupported signature or key algorithm, dwSigAlgId=%" PRIu32
		         " dwKeyAlgId=%" PRIu32 "",
		         dwSigAlgId, dwKeyAlgId);
		return FALSE;
	}

	Stream_Read_UINT16(s, wPublicKeyBlobType);

	if (wPublicKeyBlobType != BB_RSA_KEY_BLOB)
	{
		WLog_ERR(TAG, "unsupported public key blob type %" PRIu16 "", wPublicKeyBlobType);
		return FALSE;
	}

	Stream_Read_UINT16(s, wPublicKeyBlobLen);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, wPublicKeyBlobLen))
		return FALSE;

	if (!certificate_process_server_public_key(certificate, s, wPublicKeyBlobLen))
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	sigdatalen = Stream_Pointer(s) - sigdata;
	Stream_Read_UINT16(s, wSignatureBlobType);

	if (wSignatureBlobType != BB_RSA_SIGNATURE_BLOB)
	{
		WLog_ERR(TAG, "unsupported blob signature %" PRIu16 "", wSignatureBlobType);
		return FALSE;
	}

	Stream_Read_UINT16(s, wSignatureBlobLen);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, wSignatureBlobLen))
		return FALSE;

	if (wSignatureBlobLen != 72)
	{
		WLog_ERR(TAG, "invalid signature length (got %" PRIu16 ", expected 72)", wSignatureBlobLen);
		return FALSE;
	}

	if (!certificate_process_server_public_signature(certificate, sigdata, sigdatalen, s,
	                                                 wSignatureBlobLen))
	{
		WLog_ERR(TAG, "unable to parse server public signature");
		return FALSE;
	}
	return TRUE;
}

/* [MS-RDPBCGR] 2.2.1.4.3.1.1.1 RSA Public Key (RSA_PUBLIC_KEY) */
static BOOL cert_write_rsa_public_key(wStream* s, const rdpCertificate* cert)
{
	WINPR_ASSERT(cert);

	const UINT32 keyLen = cert->cert_info.ModulusLength + 8;
	const UINT32 bitLen = cert->cert_info.ModulusLength * 8;
	const UINT32 dataLen = (bitLen / 8) - 1;
	const size_t pubExpLen = sizeof(cert->cert_info.exponent);
	const BYTE* pubExp = cert->cert_info.exponent;
	const BYTE* modulus = cert->cert_info.Modulus;

	const UINT16 wPublicKeyBlobLen = 16 + pubExpLen + keyLen;
	if (!Stream_EnsureRemainingCapacity(s, 2 + wPublicKeyBlobLen))
		return FALSE;
	Stream_Write_UINT16(s, wPublicKeyBlobLen);
	Stream_Write(s, rsa_magic, sizeof(rsa_magic));
	Stream_Write_UINT32(s, keyLen);
	Stream_Write_UINT32(s, bitLen);
	Stream_Write_UINT32(s, dataLen);
	Stream_Write(s, pubExp, pubExpLen);
	Stream_Write(s, modulus, cert->cert_info.ModulusLength);
	Stream_Zero(s, 8);
	return TRUE;
}

static BOOL cert_write_rsa_signature(wStream* s, const void* sigData, size_t sigDataLen)
{
	BYTE encryptedSignature[TSSK_KEY_LENGTH] = { 0 };
	BYTE signature[sizeof(initial_signature)] = { 0 };

	memcpy(signature, initial_signature, sizeof(initial_signature));
	if (!winpr_Digest(WINPR_MD_MD5, sigData, sigDataLen, signature, sizeof(signature)))
		return FALSE;

	crypto_rsa_private_encrypt(signature, sizeof(signature), &tssk, encryptedSignature,
	                           sizeof(encryptedSignature));

	if (!Stream_EnsureRemainingCapacity(s, 2 * sizeof(UINT16) + sizeof(encryptedSignature) + 8))
		return FALSE;
	Stream_Write_UINT16(s, BB_RSA_SIGNATURE_BLOB);
	Stream_Write_UINT16(s, sizeof(encryptedSignature) + 8); /* wSignatureBlobLen */
	Stream_Write(s, encryptedSignature, sizeof(encryptedSignature));
	Stream_Zero(s, 8);
	return TRUE;
}

/* [MS-RDPBCGR] 2.2.1.4.3.1.1 Server Proprietary Certificate (PROPRIETARYSERVERCERTIFICATE) */
static BOOL cert_write_server_certificate_v1(wStream* s, const rdpCertificate* certificate)
{
	const size_t start = Stream_GetPosition(s);
	const BYTE* sigData = Stream_Pointer(s) - sizeof(UINT32);

	WINPR_ASSERT(start >= 4);
	if (!Stream_EnsureRemainingCapacity(s, 10))
		return FALSE;
	Stream_Write_UINT32(s, SIGNATURE_ALG_RSA);
	Stream_Write_UINT32(s, KEY_EXCHANGE_ALG_RSA);
	Stream_Write_UINT16(s, BB_RSA_KEY_BLOB);
	if (!cert_write_rsa_public_key(s, certificate))
		return FALSE;

	const size_t end = Stream_GetPosition(s);
	return cert_write_rsa_signature(s, sigData, end - start + sizeof(UINT32));
}

static BOOL cert_write_server_certificate_v2(wStream* s, const rdpCertificate* certificate)
{
	WINPR_ASSERT(certificate);

	const rdpX509CertChain* chain = &certificate->x509_cert_chain;
	const size_t padding = 8ull + 4ull * chain->count;

	if (Stream_EnsureRemainingCapacity(s, sizeof(UINT32)))
		return FALSE;

	Stream_Write_UINT32(s, chain->count);
	for (UINT32 x = 0; x < chain->count; x++)
	{
		const rdpCertBlob* cert = &chain->array[x];
		if (!cert_blob_write(cert, s))
			return FALSE;
	}

	if (Stream_EnsureRemainingCapacity(s, padding))
		return FALSE;
	Stream_Zero(s, padding);
	return FALSE;
}

SSIZE_T certificate_write_server_certificate(const rdpCertificate* certificate, UINT32 dwVersion,
                                             wStream* s)
{
	if (!certificate)
		return -1;

	const size_t start = Stream_GetPosition(s);
	if (!Stream_EnsureRemainingCapacity(s, 4))
		return -1;
	Stream_Write_UINT32(s, dwVersion);

	switch (dwVersion & CERT_CHAIN_VERSION_MASK)
	{
		case CERT_CHAIN_VERSION_1:
			if (!cert_write_server_certificate_v1(s, certificate))
				return -1;
			break;
		case CERT_CHAIN_VERSION_2:
			if (!cert_write_server_certificate_v2(s, certificate))
				return -1;
			break;
		default:
			WLog_ERR(TAG, "invalid certificate chain version:%" PRIu32 "",
			         dwVersion & CERT_CHAIN_VERSION_MASK);
			return -1;
	}

	const size_t end = Stream_GetPosition(s);
	return end - start;
}

/**
 * Read an X.509 Certificate Chain.
 * @param certificate certificate module
 * @param s stream
 */

static BOOL certificate_read_server_x509_certificate_chain(rdpCertificate* certificate, wStream* s)
{
	BOOL ret = FALSE;
	UINT32 numCertBlobs = 0;
	DEBUG_CERTIFICATE("Server X.509 Certificate Chain");

	WINPR_ASSERT(certificate);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, numCertBlobs); /* numCertBlobs */
	certificate->x509_cert_chain = certificate_new_x509_certificate_chain(numCertBlobs);

	for (UINT32 i = 0; i < certificate->x509_cert_chain.count; i++)
	{
		rdpCertBlob* blob = &certificate->x509_cert_chain.array[i];
		if (!cert_blob_read(blob, s))
			return FALSE;

		if ((numCertBlobs - i) == 2)
		{
			rdpCertInfo cert_info = { 0 };
			DEBUG_CERTIFICATE("License Server Certificate");
			ret = certificate_read_x509_certificate(blob, &cert_info);
			DEBUG_LICENSE("modulus length:%" PRIu32 "", cert_info.ModulusLength);
			free(cert_info.Modulus);

			if (!ret)
			{
				WLog_ERR(TAG, "failed to read License Server, content follows:");
				winpr_HexDump(TAG, WLOG_ERROR, blob->data, blob->length);
				return FALSE;
			}
		}
		else if (numCertBlobs - i == 1)
		{
			DEBUG_CERTIFICATE("Terminal Server Certificate");

			if (!certificate_read_x509_certificate(blob, &certificate->cert_info))
				return FALSE;

			DEBUG_CERTIFICATE("modulus length:%" PRIu32 "", certificate->cert_info.ModulusLength);
		}
	}

	return TRUE;
}

static BOOL certificate_write_server_x509_certificate_chain(const rdpCertificate* certificate,
                                                            wStream* s)
{
	UINT32 numCertBlobs = 0;

	WINPR_ASSERT(certificate);
	WINPR_ASSERT(s);

	numCertBlobs = certificate->x509_cert_chain.count;

	if (!Stream_EnsureRemainingCapacity(s, 4))
		return FALSE;
	Stream_Write_UINT32(s, numCertBlobs); /* numCertBlobs */

	for (UINT32 i = 0; i < numCertBlobs; i++)
	{
		const rdpCertBlob* cert = &certificate->x509_cert_chain.array[i];
		if (!cert_blob_write(cert, s))
			return FALSE;
	}

	return TRUE;
}

/**
 * Read a Server Certificate.
 * @param certificate certificate module
 * @param server_cert server certificate
 * @param length certificate length
 */

BOOL certificate_read_server_certificate(rdpCertificate* certificate, const BYTE* server_cert,
                                         size_t length)
{
	BOOL ret = FALSE;
	wStream *s, sbuffer;
	UINT32 dwVersion = 0;

	WINPR_ASSERT(certificate);
	if (length < 4) /* NULL certificate is not an error see #1795 */
		return TRUE;

	WINPR_ASSERT(server_cert);
	s = Stream_StaticConstInit(&sbuffer, server_cert, length);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

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
			WLog_ERR(TAG, "invalid certificate chain version:%" PRIu32 "",
			         dwVersion & CERT_CHAIN_VERSION_MASK);
			ret = FALSE;
			break;
	}

	return ret;
}

static BOOL read_bignum(BYTE** dst, UINT32* length, const BIGNUM* num, BOOL alloc)
{
	WINPR_ASSERT(dst);
	WINPR_ASSERT(length);
	WINPR_ASSERT(num);

	if (alloc)
	{
		*dst = NULL;
		*length = 0;
	}

	const int len = BN_num_bytes(num);
	if (len < 0)
		return FALSE;

	if (!alloc)
	{
		if (*length < (UINT32)len)
			return FALSE;
	}

	if (len > 0)
	{
		if (alloc)
		{
			*dst = malloc((size_t)len);
			if (!*dst)
				return FALSE;
		}
		BN_bn2bin(num, *dst);
		crypto_reverse(*dst, (size_t)len);
		*length = (UINT32)len;
	}

	return TRUE;
}

static BIO* bio_from_pem(const char* pem, size_t pem_length)
{
	if (!pem)
		return NULL;

	return BIO_new_mem_buf((const void*)pem, pem_length);
}

static RSA* rsa_from_private_pem(const char* pem, size_t pem_length)
{
	RSA* rsa = NULL;
	BIO* bio = bio_from_pem(pem, pem_length);
	if (!bio)
		return NULL;

	rsa = PEM_read_bio_RSAPrivateKey(bio, NULL, NULL, NULL);
	BIO_free_all(bio);
	return rsa;
}

static RSA* rsa_from_public_pem(const char* pem, size_t pem_length)
{
	RSA* rsa = NULL;
	BIO* bio = bio_from_pem(pem, pem_length);
	if (!bio)
		return NULL;

	rsa = PEM_read_bio_RSAPublicKey(bio, NULL, NULL, NULL);
	BIO_free_all(bio);

	return rsa;
}

static BOOL key_read_private(rdpRsaKey* key, const char* pem, size_t pem_length)
{
	BOOL rc = FALSE;
	RSA* rsa = rsa_from_private_pem(pem, pem_length);

	const BIGNUM* rsa_e = NULL;
	const BIGNUM* rsa_n = NULL;
	const BIGNUM* rsa_d = NULL;

	WINPR_ASSERT(key);
	if (!rsa)
	{
		WLog_ERR(TAG, "unable to load RSA key from PEM: %s", strerror(errno));
		goto fail;
	}

	switch (RSA_check_key(rsa))
	{
		case 0:
			WLog_ERR(TAG, "invalid RSA key in PEM");
			goto fail;

		case 1:
			/* Valid key. */
			break;

		default:
			WLog_ERR(TAG, "unexpected error when checking RSA key from: %s", strerror(errno));
			goto fail;
	}

	RSA_get0_key(rsa, &rsa_n, &rsa_e, &rsa_d);

	if (BN_num_bytes(rsa_e) > 4)
	{
		WLog_ERR(TAG, "RSA public exponent in PEM too large");
		goto fail;
	}

	if (!read_bignum(&key->PrivateExponent, &key->PrivateExponentLength, rsa_d, TRUE))
		goto fail;

	if (!cert_info_create(&key->cert, rsa_n, rsa_e))
		goto fail;
	rc = TRUE;
fail:
	RSA_free(rsa);
	return rc;
}

static X509* x509_from_pem(const char* pem, size_t pem_length)
{
	X509* x509 = NULL;
	BIO* bio = bio_from_pem(pem, pem_length);
	if (!bio)
		return NULL;

	x509 = PEM_read_bio_X509(bio, NULL, NULL, NULL);
	BIO_free_all(bio);
	return x509;
}

static BOOL cert_read_public(rdpCertificate* cert, const char* pem, size_t pem_length)
{
	BOOL rc = FALSE;
	X509* x509 = x509_from_pem(pem, pem_length);

	WINPR_ASSERT(cert);

	if (!x509)
	{
		WLog_ERR(TAG, "unable to load X509 from: %s", strerror(errno));
		goto fail;
	}

	rc = TRUE;
fail:
	X509_free(x509);
	return rc;
}

rdpRsaKey* freerdp_key_new_from_pem(const char* keycontent, size_t keycontent_length)
{
	rdpRsaKey* key = NULL;

	if (!keycontent)
		return NULL;

	key = (rdpRsaKey*)calloc(1, sizeof(rdpRsaKey));

	if (!key)
		return NULL;

	if (!key_read_private(key, keycontent, keycontent_length))
		goto fail;

	return key;
fail:
	freerdp_key_free(key);
	return NULL;
}

rdpRsaKey* freerdp_key_new_from_file(const char* keyfile)
{
	FILE* fp = NULL;
	INT64 length;
	char* buffer = NULL;
	rdpRsaKey* key = NULL;

	if (!keyfile)
		return NULL;

	fp = winpr_fopen(keyfile, "rb");

	if (!fp)
	{
		WLog_ERR(TAG, "unable to open RSA key file %s: %s.", keyfile, strerror(errno));
		goto out_free;
	}

	if (_fseeki64(fp, 0, SEEK_END) < 0)
		goto out_free;

	if ((length = _ftelli64(fp)) < 0)
		goto out_free;

	if (_fseeki64(fp, 0, SEEK_SET) < 0)
		goto out_free;

	buffer = (char*)malloc(length + 1);

	if (!buffer)
		goto out_free;

	if (fread((void*)buffer, length, 1, fp) != 1)
		goto out_free;

	buffer[length] = '\0';
	key = freerdp_key_new_from_pem(buffer, length);
out_free:

	if (fp)
		fclose(fp);

	free(buffer);
	return key;
}

rdpRsaKey* key_clone(const rdpRsaKey* key)
{
	if (!key)
		return NULL;

	rdpRsaKey* _key = (rdpRsaKey*)calloc(1, sizeof(rdpRsaKey));

	if (!_key)
		return NULL;

	if (key->PrivateExponent)
	{
		_key->PrivateExponent = (BYTE*)malloc(key->PrivateExponentLength);

		if (!_key->PrivateExponent)
			goto out_fail;

		CopyMemory(_key->PrivateExponent, key->PrivateExponent, key->PrivateExponentLength);
		_key->PrivateExponentLength = key->PrivateExponentLength;
	}

	return _key;
out_fail:
	freerdp_key_free(_key);
	return NULL;
}

void freerdp_key_free(rdpRsaKey* key)
{
	if (!key)
		return;

	if (key->PrivateExponent)
		memset(key->PrivateExponent, 0, key->PrivateExponentLength);
	free(key->PrivateExponent);
	cert_info_free(&key->cert);
	free(key);
}

BOOL cert_info_create(rdpCertInfo* dst, const BIGNUM* rsa, const BIGNUM* rsa_e)
{
	const rdpCertInfo empty = { 0 };

	WINPR_ASSERT(dst);
	WINPR_ASSERT(rsa);

	*dst = empty;

	if (!read_bignum(&dst->Modulus, &dst->ModulusLength, rsa, TRUE))
		goto fail;

	UINT32 len = sizeof(dst->exponent);
	BYTE* ptr = &dst->exponent[0];
	if (!read_bignum(&ptr, &len, rsa_e, FALSE))
		goto fail;
	return TRUE;

fail:
	cert_info_free(dst);
	return FALSE;
}

static BOOL cert_info_clone(rdpCertInfo* dst, const rdpCertInfo* src)
{
	WINPR_ASSERT(dst);
	WINPR_ASSERT(src);

	*dst = *src;

	dst->Modulus = NULL;
	dst->ModulusLength = 0;
	if (src->ModulusLength > 0)
	{
		dst->Modulus = malloc(src->ModulusLength);
		if (!dst->Modulus)
			return FALSE;
		memcpy(dst->Modulus, src->Modulus, src->ModulusLength);
		dst->ModulusLength = src->ModulusLength;
	}
	return TRUE;
}

void cert_info_free(rdpCertInfo* info)
{
	WINPR_ASSERT(info);
	free(info->Modulus);
	info->ModulusLength = 0;
}

BOOL cert_info_allocate(rdpCertInfo* info, size_t size)
{
	WINPR_ASSERT(info);
	cert_info_free(info);

	info->Modulus = (BYTE*)malloc(size);

	if (!info->Modulus && (size > 0))
		return FALSE;
	info->ModulusLength = (UINT32)size;
	return TRUE;
}

BOOL cert_info_read_modulus(rdpCertInfo* info, size_t size, wStream* s)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, size))
		return FALSE;
	if (size > UINT32_MAX)
		return FALSE;
	if (!cert_info_allocate(info, size))
		return FALSE;
	Stream_Read(s, info->Modulus, info->ModulusLength);
	return TRUE;
}

BOOL cert_info_read_exponent(rdpCertInfo* info, size_t size, wStream* s)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, size))
		return FALSE;
	if (size > 4)
		return FALSE;
	if (!info->Modulus || (info->ModulusLength == 0))
		return FALSE;
	Stream_Read(s, &info->exponent[4 - size], size);
	crypto_reverse(info->Modulus, info->ModulusLength);
	crypto_reverse(info->exponent, 4);
	return TRUE;
}

static BOOL cert_blob_copy(rdpCertBlob* dst, const rdpCertBlob* src)
{
	const rdpCertBlob empty = { 0 };
	WINPR_ASSERT(dst);
	WINPR_ASSERT(src);

	*dst = empty;
	if (src->length > 0)
	{
		dst->data = malloc(src->length);
		if (!dst->data)
			return FALSE;
		dst->length = src->length;
		memcpy(dst->data, src->data, src->length);
	}

	return TRUE;
}

static BOOL cert_x509_chain_copy(rdpX509CertChain* cert, const rdpX509CertChain* src)
{
	rdpX509CertChain empty = { 0 };

	WINPR_ASSERT(cert);

	*cert = empty;
	if (!src)
		return TRUE;

	if (src->count > 0)
	{
		cert->array = calloc(src->count, sizeof(rdpCertBlob));
		if (!cert->array)
		{
			return FALSE;
		}
		cert->count = src->count;

		for (UINT32 x = 0; x < cert->count; x++)
		{
			const rdpCertBlob* srcblob = &src->array[x];
			rdpCertBlob* dstblob = &cert->array[x];

			if (!cert_blob_copy(dstblob, srcblob))
			{
				certificate_free_x509_certificate_chain(cert);
				return FALSE;
			}
		}
	}

	return TRUE;
}

BOOL cert_clone_int(rdpCertificate* dst, const rdpCertificate* src)
{
	WINPR_ASSERT(dst);
	WINPR_ASSERT(src);

	if (!cert_info_clone(&dst->cert_info, &src->cert_info))
		return FALSE;
	return cert_x509_chain_copy(&dst->x509_cert_chain, &src->x509_cert_chain);
}

rdpCertificate* certificate_clone(const rdpCertificate* certificate)
{
	if (!certificate)
		return NULL;

	rdpCertificate* _certificate = (rdpCertificate*)calloc(1, sizeof(rdpCertificate));

	if (!_certificate)
		return NULL;

	if (!cert_clone_int(_certificate, certificate))
		goto out_fail;

	return _certificate;
out_fail:

	freerdp_certificate_free(_certificate);
	return NULL;
}

/**
 * Instantiate new certificate module.
 * @return new certificate module
 */

rdpCertificate* freerdp_certificate_new(void)
{
	return (rdpCertificate*)calloc(1, sizeof(rdpCertificate));
}

void certificate_free_int(rdpCertificate* certificate)
{
	WINPR_ASSERT(certificate);

	certificate_free_x509_certificate_chain(&certificate->x509_cert_chain);
	cert_info_free(&certificate->cert_info);
}

/**
 * Free certificate module.
 * @param certificate certificate module to be freed
 */

void freerdp_certificate_free(rdpCertificate* certificate)
{
	if (!certificate)
		return;

	certificate_free_int(certificate);
	free(certificate);
}

rdpCertificate* freerdp_certificate_new_from_file(const char* file)
{
	INT64 size = 0;
	char* pem = NULL;
	rdpCertificate* cert = NULL;
	FILE* fp = winpr_fopen(file, "r");
	if (!fp)
		return NULL;

	fseek(fp, 0, SEEK_END);
	size = _ftelli64(fp);
	fseek(fp, 0, SEEK_SET);
	if (size <= 0)
		goto fail;

	pem = calloc((size_t)size + 2, sizeof(char));
	if (!pem)
		goto fail;
	if (fread(pem, 1, (size_t)size, fp) != (size_t)size)
		goto fail;

	cert = freerdp_certificate_new_from_pem(pem, size + 1);
fail:
	free(pem);
	fclose(fp);
	return cert;
}

rdpCertificate* freerdp_certificate_new_from_pem(const char* pem, size_t length)
{
	const BIGNUM* rsa_e = NULL;
	const BIGNUM* rsa_n = NULL;
	const BIGNUM* rsa_d = NULL;
	RSA* rsa = NULL;
	rdpCertificate* cert = freerdp_certificate_new();

	if (!cert || !pem)
		goto fail;

	rsa = rsa_from_private_pem(pem, length);
	if (!rsa)
		goto fail;
	RSA_get0_key(rsa, &rsa_n, &rsa_e, &rsa_d);
	if (!rsa_n || !rsa_e || !rsa_d)
		goto fail;
	if (!cert_info_create(&cert->cert_info, rsa_n, rsa_e))
		goto fail;
	RSA_free(rsa);
	return cert;

fail:
	RSA_free(rsa);
	freerdp_certificate_free(cert);
	return NULL;
}

void* freerdp_certificate_get_x509(const rdpCertificate* certificate)
{
	return NULL;
}

const char* freerdp_certificate_get_pem(const rdpCertificate* certificate, size_t* length)
{
	if (length)
		*length = 0;

	if (!certificate)
		return NULL;

	if (length)
		*length = certificate->pem_length;
	return certificate->pem;
}

const rdpCertInfo* freerdp_certificate_get_info(const rdpCertificate* certificate)
{
	if (!certificate)
		return NULL;
	return &certificate->cert_info;
}

rdpCertInfo* freerdp_certificate_get_info_writeable(rdpCertificate* certificate)
{
	if (!certificate)
		return NULL;
	return &certificate->cert_info;
}

const BYTE* freerdp_key_get_exponent(const rdpRsaKey* key, size_t* exponent_length)
{
	if (exponent_length)
		*exponent_length = 0;
	if (!key)
		return NULL;

	if (exponent_length)
		*exponent_length = key->PrivateExponentLength;
	return key->PrivateExponent;
}

const rdpCertInfo* freerdp_key_get_cert_info(const rdpRsaKey* key)
{
	if (!key)
		return NULL;
	return &key->cert;
}

const char* freerdp_key_get_pem(const rdpRsaKey* key, size_t* length)
{
	if (length)
		*length = 0;
	if (!key)
		return NULL;

	if (length)
		*length = key->pem_length;
	return key->pem;
}

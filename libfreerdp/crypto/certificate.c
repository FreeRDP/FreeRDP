/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Certificate Handling
 *
 * Copyright 2011 Jiten Pathy
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2023 Armin Novak <anovak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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
#include <winpr/print.h>
#include <winpr/crypto.h>

#include <freerdp/crypto/certificate.h>

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include "certificate.h"
#include "cert_common.h"
#include "crypto.h"

#include "x509_utils.h"
#include "privatekey.h"
#include "opensslcompat.h"

#define TAG FREERDP_TAG("core")

#define CERTIFICATE_TAG FREERDP_TAG("core.certificate")
#ifdef WITH_DEBUG_CERTIFICATE
#define DEBUG_CERTIFICATE(...) WLog_DBG(TAG, __VA_ARGS__)
#else
#define DEBUG_CERTIFICATE(...) \
	do                         \
	{                          \
	} while (0)
#endif

#define TSSK_KEY_LENGTH 64

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
	X509* x509;
	STACK_OF(X509) * chain;

	rdpCertInfo cert_info;
	rdpX509CertChain x509_cert_chain;
};

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

#if defined(CERT_VALIDATE_RSA)
static const BYTE tssk_exponent[] = { 0x5b, 0x7b, 0x88, 0xc0 };
#endif

static void certificate_free_int(rdpCertificate* certificate);
static BOOL cert_clone_int(rdpCertificate* dst, const rdpCertificate* src);

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
		goto fail;

	Stream_Read_UINT32(s, certLength);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, certLength))
		goto fail;

	DEBUG_CERTIFICATE("X.509 Certificate #%" PRIu32 ", length:%" PRIu32 "", i + 1, certLength);
	blob->data = (BYTE*)malloc(certLength);

	if (!blob->data)
		goto fail;

	Stream_Read(s, blob->data, certLength);
	blob->length = certLength;

	return TRUE;

fail:
	cert_blob_free(blob);
	return FALSE;
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

static BOOL is_rsa_key(const X509* x509)
{
	EVP_PKEY* evp = X509_get0_pubkey(x509);
	if (!evp)
		return FALSE;

	return (EVP_PKEY_id(evp) == EVP_PKEY_RSA);
}

static BOOL certificate_read_x509_certificate(const rdpCertBlob* cert, rdpCertInfo* info)
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

static BOOL update_x509_from_info(rdpCertificate* cert)
{
	BOOL rc = FALSE;

	WINPR_ASSERT(cert);

	X509_free(cert->x509);
	cert->x509 = NULL;

	rdpCertInfo* info = &cert->cert_info;

	RSA* rsa = RSA_new();
	BIGNUM* e = BN_new();
	BIGNUM* mod = BN_new();
	if (!mod || !e || !rsa)
		goto fail;
	if (!BN_bin2bn(info->Modulus, info->ModulusLength, mod))
		goto fail;
	if (!BN_bin2bn(info->exponent, sizeof(info->exponent), e))
		goto fail;

	const int rec = RSA_set0_key(rsa, mod, e, NULL);
	if (rec != 1)
		goto fail;

	cert->x509 = x509_from_rsa(rsa);
	if (!cert->x509)
		goto fail;

	rc = TRUE;

fail:
	if (rsa)
		RSA_free(rsa);
	else
	{
		BN_free(mod);
		BN_free(e);
	}
	return rc;
}

static BOOL certificate_process_server_public_key(rdpCertificate* cert, wStream* s, UINT32 length)
{
	char magic[sizeof(rsa_magic)] = { 0 };
	UINT32 keylen = 0;
	UINT32 bitlen = 0;
	UINT32 datalen = 0;
	BYTE* tmp = NULL;

	WINPR_ASSERT(cert);
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 20))
		return FALSE;

	Stream_Read(s, magic, sizeof(magic));

	if (memcmp(magic, rsa_magic, sizeof(magic)) != 0)
	{
		WLog_ERR(TAG, "magic error");
		return FALSE;
	}

	rdpCertInfo* info = &cert->cert_info;
	cert_info_free(info);

	Stream_Read_UINT32(s, keylen);
	Stream_Read_UINT32(s, bitlen);
	Stream_Read_UINT32(s, datalen);
	Stream_Read(s, info->exponent, 4);

	if ((keylen <= 8) || (!Stream_CheckAndLogRequiredLength(TAG, s, keylen)))
		return FALSE;

	info->ModulusLength = keylen - 8;
	tmp = realloc(info->Modulus, info->ModulusLength);

	if (!tmp)
		return FALSE;
	info->Modulus = tmp;

	Stream_Read(s, info->Modulus, info->ModulusLength);
	Stream_Seek(s, 8); /* 8 bytes of zero padding */
	return update_x509_from_info(cert);
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
	WINPR_ASSERT(freerdp_certificate_is_rsa(cert));

	const rdpCertInfo* info = &cert->cert_info;

	const UINT32 keyLen = info->ModulusLength + 8;
	const UINT32 bitLen = info->ModulusLength * 8;
	const UINT32 dataLen = (bitLen / 8) - 1;
	const size_t pubExpLen = sizeof(info->exponent);
	const BYTE* pubExp = info->exponent;
	const BYTE* modulus = info->Modulus;

	const UINT16 wPublicKeyBlobLen = 16 + pubExpLen + keyLen;
	if (!Stream_EnsureRemainingCapacity(s, 2 + wPublicKeyBlobLen))
		return FALSE;
	Stream_Write_UINT16(s, wPublicKeyBlobLen);
	Stream_Write(s, rsa_magic, sizeof(rsa_magic));
	Stream_Write_UINT32(s, keyLen);
	Stream_Write_UINT32(s, bitLen);
	Stream_Write_UINT32(s, dataLen);
	Stream_Write(s, pubExp, pubExpLen);
	Stream_Write(s, modulus, info->ModulusLength);
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

	crypto_rsa_private_encrypt(signature, sizeof(signature), priv_key_tssk, encryptedSignature,
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

SSIZE_T freerdp_certificate_write_server_cert(const rdpCertificate* certificate, UINT32 dwVersion,
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

static BOOL certificate_read_server_x509_certificate_chain(rdpCertificate* cert, wStream* s)
{
	UINT32 numCertBlobs = 0;
	DEBUG_CERTIFICATE("Server X.509 Certificate Chain");

	WINPR_ASSERT(cert);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, numCertBlobs); /* numCertBlobs */
	certificate_free_x509_certificate_chain(&cert->x509_cert_chain);
	cert->x509_cert_chain = certificate_new_x509_certificate_chain(numCertBlobs);

	for (UINT32 i = 0; i < cert->x509_cert_chain.count; i++)
	{
		rdpCertBlob* blob = &cert->x509_cert_chain.array[i];
		if (!cert_blob_read(blob, s))
			return FALSE;

		if (numCertBlobs - i == 1)
		{
			DEBUG_CERTIFICATE("Terminal Server Certificate");

			BOOL res = certificate_read_x509_certificate(blob, &cert->cert_info);

			if (res)
			{
				if (!update_x509_from_info(cert))
					res = FALSE;
			}

			if (!res)
			{
				return FALSE;
			}

			DEBUG_CERTIFICATE("modulus length:%" PRIu32 "", cert_info.ModulusLength);
		}
	}

	return update_x509_from_info(cert);
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

BOOL freerdp_certificate_read_server_cert(rdpCertificate* certificate, const BYTE* server_cert,
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

static BOOL cert_blob_copy(rdpCertBlob* dst, const rdpCertBlob* src)
{
	WINPR_ASSERT(dst);
	WINPR_ASSERT(src);

	cert_blob_free(dst);
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
	WINPR_ASSERT(cert);

	certificate_free_x509_certificate_chain(cert);
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

	if (src->x509)
	{
		dst->x509 = X509_dup(src->x509);
		if (!dst->x509)
			return FALSE;
	}

	if (!cert_info_clone(&dst->cert_info, &src->cert_info))
		return FALSE;
	return cert_x509_chain_copy(&dst->x509_cert_chain, &src->x509_cert_chain);
}

rdpCertificate* freerdp_certificate_clone(const rdpCertificate* certificate)
{
	if (!certificate)
		return NULL;

	rdpCertificate* _certificate = freerdp_certificate_new();

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

void certificate_free_int(rdpCertificate* cert)
{
	WINPR_ASSERT(cert);

	if (cert->x509)
		X509_free(cert->x509);
	if (cert->chain)
		sk_X509_free(cert->chain);

	certificate_free_x509_certificate_chain(&cert->x509_cert_chain);
	cert_info_free(&cert->cert_info);
}

/**
 * Free certificate module.
 * @param certificate certificate module to be freed
 */

void freerdp_certificate_free(rdpCertificate* cert)
{
	if (!cert)
		return;

	certificate_free_int(cert);
	free(cert);
}

static BOOL freerdp_rsa_from_x509(rdpCertificate* cert)
{
	BOOL rc = FALSE;

	WINPR_ASSERT(cert);

	if (!freerdp_certificate_is_rsa(cert))
		return TRUE;

	RSA* rsa = NULL;
	EVP_PKEY* pubkey = X509_get0_pubkey(cert->x509);
	if (!pubkey)
		goto fail;
	rsa = EVP_PKEY_get1_RSA(pubkey);

	/* If this is not a RSA key return success */
	rc = TRUE;
	if (!rsa)
		goto fail;

	/* Now we return failure again if something is wrong. */
	rc = FALSE;

	const BIGNUM* rsa_n = NULL;
	const BIGNUM* rsa_e = NULL;
	RSA_get0_key(rsa, &rsa_n, &rsa_e, NULL);
	if (!rsa_n || !rsa_e)
		goto fail;
	if (!cert_info_create(&cert->cert_info, rsa_n, rsa_e))
		goto fail;
	rc = TRUE;
fail:
	RSA_free(rsa);
	return rc;
}

rdpCertificate* freerdp_certificate_new_from_der(const BYTE* data, size_t length)
{
	rdpCertificate* cert = freerdp_certificate_new();

	if (!cert || !data || (length == 0))
		goto fail;
	char* ptr = data;
	cert->x509 = d2i_X509(NULL, &ptr, length);
	if (!cert->x509)
		goto fail;
	if (!freerdp_rsa_from_x509(cert))
		goto fail;
	return cert;
fail:
	freerdp_certificate_free(cert);
	return NULL;
}

rdpCertificate* freerdp_certificate_new_from_x509(const X509* xcert, const STACK_OF(X509) * chain)
{
	WINPR_ASSERT(xcert);

	rdpCertificate* cert = freerdp_certificate_new();
	if (!cert)
		return NULL;

	cert->x509 = X509_dup(xcert);
	if (!cert->x509)
		goto fail;

	if (!freerdp_rsa_from_x509(cert))
		goto fail;

	if (chain)
	{
		cert->chain = sk_X509_dup(chain);
	}
	return cert;
fail:
	freerdp_certificate_free(cert);
	return NULL;
}

static rdpCertificate* freerdp_certificate_new_from(const char* file, BOOL isFile)
{
	X509* x509 = x509_utils_from_pem(file, strlen(file), isFile);
	if (!x509)
		return NULL;
	rdpCertificate* cert = freerdp_certificate_new_from_x509(x509, NULL);
	X509_free(x509);
	return cert;
}

rdpCertificate* freerdp_certificate_new_from_file(const char* file)
{
	return freerdp_certificate_new_from(file, TRUE);
}

rdpCertificate* freerdp_certificate_new_from_pem(const char* pem)
{
	return freerdp_certificate_new_from(pem, FALSE);
}

const rdpCertInfo* freerdp_certificate_get_info(const rdpCertificate* cert)
{
	WINPR_ASSERT(cert);
	if (!freerdp_certificate_is_rsa(cert))
		return NULL;
	return &cert->cert_info;
}

char* freerdp_certificate_get_fingerprint(const rdpCertificate* cert)
{
	return freerdp_certificate_get_fingerprint_by_hash(cert, "sha256");
}

char* freerdp_certificate_get_fingerprint_by_hash(const rdpCertificate* cert, const char* hash)
{
	return freerdp_certificate_get_fingerprint_by_hash_ex(cert, hash, TRUE);
}

char* freerdp_certificate_get_fingerprint_by_hash_ex(const rdpCertificate* cert, const char* hash,
                                                     BOOL separator)
{
	size_t fp_len = 0;
	size_t pos = 0;
	size_t size = 0;
	BYTE* fp = NULL;
	char* fp_buffer = NULL;
	if (!cert || !cert->x509)
	{
		WLog_ERR(TAG, "Invalid certificate [%p, %p]", cert, cert ? cert->x509 : NULL);
		return NULL;
	}
	if (!hash)
	{
		WLog_ERR(TAG, "Invalid certificate hash %p", hash);
		return NULL;
	}
	fp = x509_utils_get_hash(cert->x509, hash, &fp_len);
	if (!fp)
		return NULL;

	size = fp_len * 3 + 1;
	fp_buffer = calloc(size, sizeof(char));
	if (!fp_buffer)
		goto fail;

	pos = 0;

	size_t i = 0;
	for (i = 0; i < (fp_len - 1); i++)
	{
		int rc;
		char* p = &fp_buffer[pos];
		if (separator)
			rc = sprintf_s(p, size - pos, "%02" PRIx8 ":", fp[i]);
		else
			rc = sprintf_s(p, size - pos, "%02" PRIx8, fp[i]);
		if (rc <= 0)
			goto fail;
		pos += (size_t)rc;
	}

	sprintf_s(&fp_buffer[pos], size - pos, "%02" PRIx8 "", fp[i]);

	free(fp);

	return fp_buffer;
fail:
	free(fp);
	free(fp_buffer);
	return NULL;
}

static BOOL bio_read_pem(BIO* bio, char** ppem, size_t* plength)
{
	BOOL rc = FALSE;

	WINPR_ASSERT(bio);
	WINPR_ASSERT(ppem);

	size_t offset = 0;
	size_t length = 2048;
	char* pem = NULL;
	while (offset < length)
	{
		char* tmp = realloc(pem, length + 1);
		if (!tmp)
			goto fail;
		pem = tmp;

		ERR_clear_error();

		const int status = BIO_read(bio, &pem[offset], length - offset);
		if (status < 0)
		{
			WLog_ERR(TAG, "failed to read certificate");
			goto fail;
		}

		if (status == 0)
			break;

		offset += (size_t)status;
		if (length - offset > 0)
			break;
		length *= 2;
	}
	pem[offset] = '\0';
	*ppem = pem;
	if (plength)
		*plength = offset;
	rc = TRUE;
fail:
	return rc;
}

char* freerdp_certificate_get_pem(const rdpCertificate* cert, size_t* pLength)
{
	BOOL rc = FALSE;
	char* pem = NULL;
	WINPR_ASSERT(cert);

	if (!cert->x509)
		return NULL;

	BIO* bio;
	int status;

	/**
	 * Don't manage certificates internally, leave it up entirely to the external client
	 * implementation
	 */
	bio = BIO_new(BIO_s_mem());

	if (!bio)
	{
		WLog_ERR(TAG, "BIO_new() failure");
		return NULL;
	}

	status = PEM_write_bio_X509(bio, cert->x509);

	if (status < 0)
	{
		WLog_ERR(TAG, "PEM_write_bio_X509 failure: %d", status);
		goto fail;
	}

#if 0
        if (chain)
        {
            count = sk_X509_num(chain);
            for (x = 0; x < count; x++)
            {
                X509* c = sk_X509_value(chain, x);
                status = PEM_write_bio_X509(bio, c);
                if (status < 0)
                {
                    WLog_ERR(TAG, "PEM_write_bio_X509 failure: %d", status);
                    goto fail;
                }
            }
        }
#endif
	if (!bio_read_pem(bio, &pem, pLength))
		goto fail;
	rc = TRUE;
fail:
	BIO_free_all(bio);
	return pem;
}

char* freerdp_certificate_get_subject(const rdpCertificate* cert)
{
	WINPR_ASSERT(cert);
	return x509_utils_get_subject(cert->x509);
}

char* freerdp_certificate_get_issuer(const rdpCertificate* cert)
{
	WINPR_ASSERT(cert);
	return x509_utils_get_issuer(cert->x509);
}

char* freerdp_certificate_get_upn(const rdpCertificate* cert)
{
	WINPR_ASSERT(cert);
	return x509_utils_get_upn(cert->x509);
}

char* freerdp_certificate_get_email(const rdpCertificate* cert)
{
	WINPR_ASSERT(cert);
	return x509_utils_get_email(cert->x509);
}

BOOL freerdp_certificate_check_eku(const rdpCertificate* cert, int nid)
{
	WINPR_ASSERT(cert);
	return x509_utils_check_eku(cert->x509, nid);
}

BOOL freerdp_certificate_get_public_key(const rdpCertificate* cert, BYTE** PublicKey,
                                        DWORD* PublicKeyLength)
{
	BYTE* ptr = NULL;
	BYTE* optr = NULL;
	int length;
	BOOL status = FALSE;
	EVP_PKEY* pkey = NULL;

	WINPR_ASSERT(cert);

	pkey = X509_get0_pubkey(cert->x509);

	if (!pkey)
	{
		WLog_ERR(TAG, "X509_get_pubkey() failed");
		goto exit;
	}

	length = i2d_PublicKey(pkey, NULL);

	if (length < 1)
	{
		WLog_ERR(TAG, "i2d_PublicKey() failed");
		goto exit;
	}

	*PublicKey = optr = ptr = (BYTE*)calloc(length, sizeof(BYTE));

	if (!ptr)
		goto exit;

	const int length2 = i2d_PublicKey(pkey, &ptr);
	if (length != length2)
		goto exit;
	*PublicKeyLength = (DWORD)length2;
	status = TRUE;
exit:

	if (!status)
		free(optr);

	return status;
}

BOOL freerdp_certificate_verify(const rdpCertificate* cert, const char* certificate_store_path)
{
	WINPR_ASSERT(cert);
	return x509_utils_verify(cert->x509, cert->chain, certificate_store_path);
}

char** freerdp_certificate_get_dns_names(const rdpCertificate* cert, size_t* pcount,
                                         size_t** pplengths)
{
	WINPR_ASSERT(cert);
	return x509_utils_get_dns_names(cert->x509, pcount, pplengths);
}

char* freerdp_certificate_get_common_name(const rdpCertificate* cert, size_t* plength)
{
	WINPR_ASSERT(cert);
	return x509_utils_get_common_name(cert->x509, plength);
}

WINPR_MD_TYPE freerdp_certificate_get_signature_alg(const rdpCertificate* cert)
{
	WINPR_ASSERT(cert);
	return x509_utils_get_signature_alg(cert->x509);
}

void freerdp_certificate_free_dns_names(size_t count, size_t* lengths, char** names)
{
	x509_utils_dns_names_free(count, lengths, names);
}

char* freerdp_certificate_get_hash(const rdpCertificate* cert, const char* hash, size_t* plength)
{
	WINPR_ASSERT(cert);
	return x509_utils_get_hash(cert->x509, hash, plength);
}

X509* freerdp_certificate_get_x509(rdpCertificate* cert)
{
	WINPR_ASSERT(cert);
	return cert->x509;
}

RSA* freerdp_certificate_get_RSA(const rdpCertificate* cert)
{
	WINPR_ASSERT(cert);

	if (!freerdp_certificate_is_rsa(cert))
		return NULL;

	EVP_PKEY* pubkey = X509_get0_pubkey(cert->x509);
	if (!pubkey)
		return NULL;

	return EVP_PKEY_get1_RSA(pubkey);
}

BYTE* freerdp_certificate_get_der(const rdpCertificate* cert, size_t* pLength)
{
	WINPR_ASSERT(cert);

	if (pLength)
		*pLength = 0;

	const int rc = i2d_X509(cert->x509, NULL);
	if (rc <= 0)
		return NULL;

	BYTE* ptr = calloc(rc + 1, sizeof(BYTE));
	if (!ptr)
		return NULL;
	BYTE* i2d_ptr = ptr;

	const int rc2 = i2d_X509(cert->x509, &i2d_ptr);
	if (rc2 <= 0)
	{
		free(ptr);
		return NULL;
	}

	if (pLength)
		*pLength = (size_t)rc2;
	return ptr;
}

BOOL freerdp_certificate_is_rsa(const rdpCertificate* cert)
{
	WINPR_ASSERT(cert);
	WINPR_ASSERT(cert->x509);
	return is_rsa_key(cert->x509);
}

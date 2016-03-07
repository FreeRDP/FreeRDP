/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Cryptographic Abstraction Layer
 *
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
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

#include <winpr/crt.h>
#include <winpr/crypto.h>

#include <freerdp/log.h>
#include <freerdp/crypto/crypto.h>

#define TAG FREERDP_TAG("crypto")

CryptoCert crypto_cert_read(BYTE* data, UINT32 length)
{
	CryptoCert cert = malloc(sizeof(*cert));
	if (!cert)
		return NULL;

	/* this will move the data pointer but we don't care, we don't use it again */
	cert->px509 = d2i_X509(NULL, (D2I_X509_CONST BYTE **) &data, length);
	return cert;
}

void crypto_cert_free(CryptoCert cert)
{
	if (cert == NULL)
		return;

	X509_free(cert->px509);

	free(cert);
}

BOOL crypto_cert_get_public_key(CryptoCert cert, BYTE** PublicKey, DWORD* PublicKeyLength)
{
	BYTE* ptr;
	int length;
	BOOL status = TRUE;
	EVP_PKEY* pkey = NULL;

	pkey = X509_get_pubkey(cert->px509);
	if (!pkey)
	{
		WLog_ERR(TAG,  "X509_get_pubkey() failed");
		status = FALSE;
		goto exit;
	}

	length = i2d_PublicKey(pkey, NULL);
	if (length < 1)
	{
		WLog_ERR(TAG,  "i2d_PublicKey() failed");
		status = FALSE;
		goto exit;
	}

	*PublicKeyLength = (DWORD) length;
	*PublicKey = (BYTE*) malloc(length);
	ptr = (BYTE*) (*PublicKey);
	if (!ptr)
	{
		status = FALSE;
		goto exit;
	}

	i2d_PublicKey(pkey, &ptr);

exit:
	if (pkey)
		EVP_PKEY_free(pkey);

	return status;
}

static int crypto_rsa_common(const BYTE* input, int length, UINT32 key_length, const BYTE* modulus, const BYTE* exponent, int exponent_size, BYTE* output)
{
	BN_CTX* ctx;
	int output_length = -1;
	BYTE* input_reverse;
	BYTE* modulus_reverse;
	BYTE* exponent_reverse;
	BIGNUM mod, exp, x, y;

	input_reverse = (BYTE*) malloc(2 * key_length + exponent_size);
	if (!input_reverse)
		return -1;
	modulus_reverse = input_reverse + key_length;
	exponent_reverse = modulus_reverse + key_length;

	memcpy(modulus_reverse, modulus, key_length);
	crypto_reverse(modulus_reverse, key_length);
	memcpy(exponent_reverse, exponent, exponent_size);
	crypto_reverse(exponent_reverse, exponent_size);
	memcpy(input_reverse, input, length);
	crypto_reverse(input_reverse, length);

	ctx = BN_CTX_new();
	if (!ctx)
		goto out_free_input_reverse;
	BN_init(&mod);
	BN_init(&exp);
	BN_init(&x);
	BN_init(&y);

	BN_bin2bn(modulus_reverse, key_length, &mod);
	BN_bin2bn(exponent_reverse, exponent_size, &exp);
	BN_bin2bn(input_reverse, length, &x);
	BN_mod_exp(&y, &x, &exp, &mod, ctx);

	output_length = BN_bn2bin(&y, output);
	crypto_reverse(output, output_length);

	if (output_length < (int) key_length)
		memset(output + output_length, 0, key_length - output_length);

	BN_free(&y);
	BN_clear_free(&x);
	BN_free(&exp);
	BN_free(&mod);
	BN_CTX_free(ctx);

out_free_input_reverse:
	free(input_reverse);

	return output_length;
}

static int crypto_rsa_public(const BYTE* input, int length, UINT32 key_length, const BYTE* modulus, const BYTE* exponent, BYTE* output)
{
	return crypto_rsa_common(input, length, key_length, modulus, exponent, EXPONENT_MAX_SIZE, output);
}

static int crypto_rsa_private(const BYTE* input, int length, UINT32 key_length, const BYTE* modulus, const BYTE* private_exponent, BYTE* output)
{
	return crypto_rsa_common(input, length, key_length, modulus, private_exponent, key_length, output);
}

int crypto_rsa_public_encrypt(const BYTE* input, int length, UINT32 key_length, const BYTE* modulus, const BYTE* exponent, BYTE* output)
{
	return crypto_rsa_public(input, length, key_length, modulus, exponent, output);
}

int crypto_rsa_public_decrypt(const BYTE* input, int length, UINT32 key_length, const BYTE* modulus, const BYTE* exponent, BYTE* output)
{
	return crypto_rsa_public(input, length, key_length, modulus, exponent, output);
}

int crypto_rsa_private_encrypt(const BYTE* input, int length, UINT32 key_length, const BYTE* modulus, const BYTE* private_exponent, BYTE* output)
{
	return crypto_rsa_private(input, length, key_length, modulus, private_exponent, output);
}

int crypto_rsa_private_decrypt(const BYTE* input, int length, UINT32 key_length, const BYTE* modulus, const BYTE* private_exponent, BYTE* output)
{
	return crypto_rsa_private(input, length, key_length, modulus, private_exponent, output);
}

int crypto_rsa_decrypt(const BYTE* input, int length, UINT32 key_length, const BYTE* modulus, const BYTE* private_exponent, BYTE* output)
{
	return crypto_rsa_common(input, length, key_length, modulus, private_exponent, key_length, output);
}

void crypto_reverse(BYTE* data, int length)
{
	int i, j;
	BYTE temp;

	for (i = 0, j = length - 1; i < j; i++, j--)
	{
		temp = data[i];
		data[i] = data[j];
		data[j] = temp;
	}
}

char* crypto_cert_fingerprint(X509* xcert)
{
	int i = 0;
	char* p;
	char* fp_buffer;
	UINT32 fp_len;
	BYTE fp[EVP_MAX_MD_SIZE];

	X509_digest(xcert, EVP_sha1(), fp, &fp_len);

	fp_buffer = (char*) calloc(3, fp_len);
	if (!fp_buffer)
		return NULL;

	p = fp_buffer;
	for (i = 0; i < (int) (fp_len - 1); i++)
	{
		sprintf(p, "%02x:", fp[i]);
		p = &fp_buffer[(i + 1) * 3];
	}
	sprintf(p, "%02x", fp[i]);

	return fp_buffer;
}

char* crypto_print_name(X509_NAME* name)
{
	char* buffer = NULL;
	BIO* outBIO = BIO_new(BIO_s_mem());

	if (X509_NAME_print_ex(outBIO, name, 0, XN_FLAG_ONELINE) > 0)
	{
		unsigned long size = BIO_number_written(outBIO);
		buffer = calloc(1, size + 1);
		if (!buffer)
			return NULL;
		BIO_read(outBIO, buffer, size);
	}

	BIO_free(outBIO);

	return buffer;
}

char* crypto_cert_subject(X509* xcert)
{
	return crypto_print_name(X509_get_subject_name(xcert));
}

char* crypto_cert_subject_common_name(X509* xcert, int* length)
{
	int index;
	BYTE* common_name_raw;
	char* common_name;
	X509_NAME* subject_name;
	X509_NAME_ENTRY* entry;
	ASN1_STRING* entry_data;

	subject_name = X509_get_subject_name(xcert);

	if (subject_name == NULL)
		return NULL;

	index = X509_NAME_get_index_by_NID(subject_name, NID_commonName, -1);

	if (index < 0)
		return NULL;

	entry = X509_NAME_get_entry(subject_name, index);

	if (entry == NULL)
		return NULL;

	entry_data = X509_NAME_ENTRY_get_data(entry);

	if (entry_data == NULL)
		return NULL;

	*length = ASN1_STRING_to_UTF8(&common_name_raw, entry_data);

	if (*length < 0)
		return NULL;

	common_name = _strdup((char*)common_name_raw);
	OPENSSL_free(common_name_raw);

	return (char*) common_name;
}

void crypto_cert_subject_alt_name_free(int count, int *lengths,
						   char** alt_name)
{
	int i;

	free(lengths);

	if (alt_name)
	{
		for (i=0; i<count; i++)
		{
			if (alt_name[i])
				OPENSSL_free(alt_name[i]);
		}

		free(alt_name);
	}
}

char** crypto_cert_subject_alt_name(X509* xcert, int* count, int** lengths)
{
	int index;
	int length = 0;
	char** strings = NULL;
	BYTE* string;
	int num_subject_alt_names;
	GENERAL_NAMES* subject_alt_names;
	GENERAL_NAME* subject_alt_name;

	*count = 0;
	subject_alt_names = X509_get_ext_d2i(xcert, NID_subject_alt_name, 0, 0);

	if (!subject_alt_names)
		return NULL;

	num_subject_alt_names = sk_GENERAL_NAME_num(subject_alt_names);
	if (num_subject_alt_names)
	{
		strings = (char**) malloc(sizeof(char*) * num_subject_alt_names);
		if (!strings)
			goto out;

		*lengths = (int*) malloc(sizeof(int) * num_subject_alt_names);
		if (!*lengths)
		{
			free(strings);
			strings = NULL;
			goto out;
		}
	}

	for (index = 0; index < num_subject_alt_names; ++index)
	{
		subject_alt_name = sk_GENERAL_NAME_value(subject_alt_names, index);

		if (subject_alt_name->type == GEN_DNS)
		{
			length = ASN1_STRING_to_UTF8(&string, subject_alt_name->d.dNSName);
			strings[*count] = (char*) string;
			(*lengths)[*count] = length;
			(*count)++;
		}
	}

	if (*count < 1)
	{
		free(strings) ;
		free(*lengths) ;
		*lengths = NULL ;
		return NULL;
	}

out:
	GENERAL_NAMES_free(subject_alt_names);

	return strings;
}

char* crypto_cert_issuer(X509* xcert)
{
	return crypto_print_name(X509_get_issuer_name(xcert));
}

BOOL x509_verify_certificate(CryptoCert cert, char* certificate_store_path)
{
	X509_STORE_CTX* csc;
	BOOL status = FALSE;
	X509_STORE* cert_ctx = NULL;
	X509_LOOKUP* lookup = NULL;
	X509* xcert = cert->px509;

	cert_ctx = X509_STORE_new();

	if (cert_ctx == NULL)
		goto end;

	OpenSSL_add_all_algorithms();
	lookup = X509_STORE_add_lookup(cert_ctx, X509_LOOKUP_file());

	if (lookup == NULL)
		goto end;

	lookup = X509_STORE_add_lookup(cert_ctx, X509_LOOKUP_hash_dir());

	if (lookup == NULL)
		goto end;

	X509_LOOKUP_add_dir(lookup, NULL, X509_FILETYPE_DEFAULT);

	if (certificate_store_path != NULL)
	{
		X509_LOOKUP_add_dir(lookup, certificate_store_path, X509_FILETYPE_PEM);
	}

	csc = X509_STORE_CTX_new();

	if (csc == NULL)
		goto end;

	X509_STORE_set_flags(cert_ctx, 0);

	if (!X509_STORE_CTX_init(csc, cert_ctx, xcert, cert->px509chain))
		goto end;

	if (X509_verify_cert(csc) == 1)
		status = TRUE;

	X509_STORE_CTX_free(csc);
	X509_STORE_free(cert_ctx);

end:
	return status;
}

rdpCertificateData* crypto_get_certificate_data(X509* xcert, char* hostname, UINT16 port)
{
	char* issuer;
	char* subject;
	char* fp;
	rdpCertificateData* certdata;

	fp = crypto_cert_fingerprint(xcert);
	if (!fp)
		return NULL;

	issuer = crypto_cert_issuer(xcert);
	subject = crypto_cert_subject(xcert);

	certdata = certificate_data_new(hostname, port, issuer, subject, fp);

	free(subject);
	free(issuer);
	free(fp);

	return certdata;
}

void crypto_cert_print_info(X509* xcert)
{
	char* fp;
	char* issuer;
	char* subject;

	subject = crypto_cert_subject(xcert);
	issuer = crypto_cert_issuer(xcert);
	fp = crypto_cert_fingerprint(xcert);
	if (!fp)
	{
		WLog_ERR(TAG,  "error computing fingerprint");
		goto out_free_issuer;
	}

	WLog_INFO(TAG,  "Certificate details:");
	WLog_INFO(TAG,  "\tSubject: %s", subject);
	WLog_INFO(TAG,  "\tIssuer: %s", issuer);
	WLog_INFO(TAG,  "\tThumbprint: %s", fp);
	WLog_INFO(TAG,  "The above X.509 certificate could not be verified, possibly because you do not have "
			"the CA certificate in your certificate store, or the certificate has expired. "
			"Please look at the documentation on how to create local certificate store for a private CA.");
	free(fp);
out_free_issuer:
	free(issuer);
	free(subject);
}

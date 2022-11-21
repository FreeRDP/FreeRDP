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

#include <openssl/objects.h>

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/assert.h>

#include <freerdp/log.h>
#include <freerdp/crypto/crypto.h>

#define TAG FREERDP_TAG("crypto")

CryptoCert crypto_cert_read(const BYTE* data, UINT32 length)
{
	CryptoCert cert = malloc(sizeof(*cert));

	if (!cert)
		return NULL;

	/* this will move the data pointer but we don't care, we don't use it again */
	cert->px509 = d2i_X509(NULL, (D2I_X509_CONST BYTE**)&data, length);
	return cert;
}

CryptoCert crypto_cert_pem_read(const char* data)
{
	CryptoCert cert = malloc(sizeof(*cert));

	if (!cert)
		return NULL;

	cert->px509 = crypto_cert_from_pem(data, strlen(data), FALSE);
	if (!cert->px509)
	{
		free(cert);
		return NULL;
	}

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
		WLog_ERR(TAG, "X509_get_pubkey() failed");
		status = FALSE;
		goto exit;
	}

	length = i2d_PublicKey(pkey, NULL);

	if (length < 1)
	{
		WLog_ERR(TAG, "i2d_PublicKey() failed");
		status = FALSE;
		goto exit;
	}

	*PublicKeyLength = (DWORD)length;
	*PublicKey = (BYTE*)malloc(length);
	ptr = (BYTE*)(*PublicKey);

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

static SSIZE_T crypto_rsa_common(const BYTE* input, size_t length, UINT32 key_length,
                                 const BYTE* modulus, const BYTE* exponent, size_t exponent_size,
                                 BYTE* output)
{
	BN_CTX* ctx = NULL;
	int output_length = -1;
	BYTE* input_reverse = NULL;
	BYTE* modulus_reverse = NULL;
	BYTE* exponent_reverse = NULL;
	BIGNUM* mod = NULL;
	BIGNUM* exp = NULL;
	BIGNUM* x = NULL;
	BIGNUM* y = NULL;
	size_t bufferSize;

	if (!input || !modulus || !exponent || !output)
		return -1;

	if ((size_t)exponent_size > SIZE_MAX / 2)
		return -1;

	if (key_length >= SIZE_MAX / 2 - exponent_size)
		return -1;

	bufferSize = 2ULL * key_length + exponent_size;
	if ((size_t)length > bufferSize)
		bufferSize = (size_t)length;

	input_reverse = (BYTE*)calloc(bufferSize, 1);

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

	if (!(ctx = BN_CTX_new()))
		goto fail_bn_ctx;

	if (!(mod = BN_new()))
		goto fail_bn_mod;

	if (!(exp = BN_new()))
		goto fail_bn_exp;

	if (!(x = BN_new()))
		goto fail_bn_x;

	if (!(y = BN_new()))
		goto fail_bn_y;

	if (!BN_bin2bn(modulus_reverse, key_length, mod))
		goto fail;

	if (!BN_bin2bn(exponent_reverse, exponent_size, exp))
		goto fail;
	if (!BN_bin2bn(input_reverse, length, x))
		goto fail;
	if (BN_mod_exp(y, x, exp, mod, ctx) != 1)
		goto fail;
	output_length = BN_bn2bin(y, output);
	if (output_length < 0)
		goto fail;
	crypto_reverse(output, output_length);

	if ((UINT32)output_length < key_length)
		memset(output + output_length, 0, key_length - output_length);

fail:
	BN_free(y);
fail_bn_y:
	BN_clear_free(x);
fail_bn_x:
	BN_free(exp);
fail_bn_exp:
	BN_free(mod);
fail_bn_mod:
	BN_CTX_free(ctx);
fail_bn_ctx:
	free(input_reverse);
	return output_length;
}

static int crypto_rsa_public(const BYTE* input, size_t length, size_t key_length,
                             const BYTE* modulus, const BYTE* exponent, BYTE* output)
{
	return crypto_rsa_common(input, length, key_length, modulus, exponent, EXPONENT_MAX_SIZE,
	                         output);
}

static int crypto_rsa_private(const BYTE* input, size_t length, size_t key_length,
                              const BYTE* modulus, const BYTE* private_exponent, BYTE* output)
{
	return crypto_rsa_common(input, length, key_length, modulus, private_exponent, key_length,
	                         output);
}

SSIZE_T crypto_rsa_public_encrypt(const BYTE* input, size_t length, size_t key_length,
                                  const BYTE* modulus, const BYTE* exponent, BYTE* output)
{
	return crypto_rsa_public(input, length, key_length, modulus, exponent, output);
}

SSIZE_T crypto_rsa_public_decrypt(const BYTE* input, size_t length, size_t key_length,
                                  const BYTE* modulus, const BYTE* exponent, BYTE* output)
{
	return crypto_rsa_public(input, length, key_length, modulus, exponent, output);
}

SSIZE_T crypto_rsa_private_encrypt(const BYTE* input, size_t length, size_t key_length,
                                   const BYTE* modulus, const BYTE* private_exponent, BYTE* output)
{
	return crypto_rsa_private(input, length, key_length, modulus, private_exponent, output);
}

SSIZE_T crypto_rsa_private_decrypt(const BYTE* input, size_t length, size_t key_length,
                                   const BYTE* modulus, const BYTE* private_exponent, BYTE* output)
{
	return crypto_rsa_private(input, length, key_length, modulus, private_exponent, output);
}

void crypto_reverse(BYTE* data, size_t length)
{
	size_t i, j;

	if (length < 1)
		return;

	for (i = 0, j = length - 1; i < j; i++, j--)
	{
		const BYTE temp = data[i];
		data[i] = data[j];
		data[j] = temp;
	}
}

char* crypto_cert_fingerprint(X509* xcert)
{
	return crypto_cert_fingerprint_by_hash(xcert, "sha256");
}

BYTE* crypto_cert_hash(X509* xcert, const char* hash, UINT32* length)
{
	UINT32 fp_len = EVP_MAX_MD_SIZE;
	BYTE* fp;
	const EVP_MD* md = EVP_get_digestbyname(hash);
	if (!md)
	{
		WLog_ERR(TAG, "System does not support %s hash!", hash);
		return NULL;
	}
	if (!xcert || !length)
	{
		WLog_ERR(TAG, "[%s] Invalid arugments: xcert=%p, length=%p", __FUNCTION__, xcert, length);
		return NULL;
	}

	fp = calloc(fp_len, sizeof(BYTE));
	if (!fp)
	{
		WLog_ERR(TAG, "[%s] could not allocate %" PRIuz " bytes", __FUNCTION__, fp_len);
		return NULL;
	}

	if (X509_digest(xcert, md, fp, &fp_len) != 1)
	{
		free(fp);
		WLog_ERR(TAG, "certificate does not have a %s hash!", hash);
		return NULL;
	}

	*length = fp_len;
	return fp;
}

char* crypto_cert_fingerprint_by_hash(X509* xcert, const char* hash)
{
	return crypto_cert_fingerprint_by_hash_ex(xcert, hash, TRUE);
}

char* crypto_cert_fingerprint_by_hash_ex(X509* xcert, const char* hash, BOOL separator)
{
	UINT32 fp_len, i;
	size_t pos, size;
	BYTE* fp;
	char* fp_buffer = NULL;
	if (!xcert)
	{
		WLog_ERR(TAG, "Invalid certificate %p", xcert);
		return NULL;
	}
	if (!hash)
	{
		WLog_ERR(TAG, "Invalid certificate hash %p", hash);
		return NULL;
	}
	fp = crypto_cert_hash(xcert, hash, &fp_len);
	if (!fp)
		return NULL;

	size = fp_len * 3 + 1;
	fp_buffer = calloc(size, sizeof(char));
	if (!fp_buffer)
		goto fail;

	pos = 0;

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

static char* crypto_print_name(X509_NAME* name)
{
	char* buffer = NULL;
	BIO* outBIO = BIO_new(BIO_s_mem());

	if (X509_NAME_print_ex(outBIO, name, 0, XN_FLAG_ONELINE) > 0)
	{
		UINT64 size = BIO_number_written(outBIO);
		if (size > INT_MAX)
			return NULL;
		buffer = calloc(1, (size_t)size + 1);

		if (!buffer)
			return NULL;

		ERR_clear_error();
		BIO_read(outBIO, buffer, (int)size);
	}

	BIO_free_all(outBIO);
	return buffer;
}

char* crypto_cert_subject(X509* xcert)
{
	char* subject;
	if (!xcert)
	{
		WLog_ERR(TAG, "Invalid certificate %p", xcert);
		return NULL;
	}
	subject = crypto_print_name(X509_get_subject_name(xcert));
	if (!subject)
		WLog_ERR(TAG, "certificate does not have a subject!");
	return subject;
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
	return (char*)common_name;
}

/* GENERAL_NAME type labels */

static const char* general_name_type_labels[] = { "OTHERNAME", "EMAIL    ", "DNS      ",
	                                              "X400     ", "DIRNAME  ", "EDIPARTY ",
	                                              "URI      ", "IPADD    ", "RID      " };

static const char* general_name_type_label(int general_name_type)
{
	if ((0 <= general_name_type) &&
	    ((size_t)general_name_type < ARRAYSIZE(general_name_type_labels)))
	{
		return general_name_type_labels[general_name_type];
	}
	else
	{
		static char buffer[80];
		sprintf(buffer, "Unknown general name type (%d)", general_name_type);
		return buffer;
	}
}

/*

map_subject_alt_name(x509,  general_name_type, mapper, data)

Call the function mapper with subjectAltNames found in the x509
certificate and data.  if generate_name_type is GEN_ALL,  the the
mapper is called for all the names,  else it's called only for names
of the given type.


We implement two extractors:

 -  a string extractor that can be used to get the subjectAltNames of
    the following types: GEN_URI,  GEN_DNS,  GEN_EMAIL

 - a ASN1_OBJECT filter/extractor that can be used to get the
   subjectAltNames of OTHERNAME type.

   Note: usually, it's a string, but some type of otherNames can be
   associated with different classes of objects. eg. a KPN may be a
   sequence of realm and principal name, instead of a single string
   object.

Not implemented yet: extractors for the types: GEN_X400, GEN_DIRNAME,
GEN_EDIPARTY, GEN_RID, GEN_IPADD (the later can contain nul-bytes).


mapper(name, data, index, count)

The mapper is passed:
 - the GENERAL_NAME selected,
 - the data,
 - the index of the general name in the subjectAltNames,
 - the total number of names in the subjectAltNames.

The last parameter let's the mapper allocate arrays to collect objects.
Note: if names are filtered,  not all the indices from 0 to count-1 are
passed to mapper,  only the indices selected.

When the mapper returns 0, map_subject_alt_name stops the iteration immediately.

*/

#define GEN_ALL (-1)

typedef int (*general_name_mapper_pr)(GENERAL_NAME* name, void* data, int index, int count);

static void map_subject_alt_name(X509* x509, int general_name_type, general_name_mapper_pr mapper,
                                 void* data)
{
	int i;
	int num;
	STACK_OF(GENERAL_NAME) * gens;
	gens = X509_get_ext_d2i(x509, NID_subject_alt_name, NULL, NULL);

	if (!gens)
	{
		return;
	}

	num = sk_GENERAL_NAME_num(gens);

	for (i = 0; (i < num); i++)
	{
		GENERAL_NAME* name = sk_GENERAL_NAME_value(gens, i);

		if (name)
		{
			if ((general_name_type == GEN_ALL) || (general_name_type == name->type))
			{
				if (!mapper(name, data, i, num))
				{
					break;
				}
			}
		}
	}

	sk_GENERAL_NAME_pop_free(gens, GENERAL_NAME_free);
}

/*
extract_string  --  string extractor

- the strings array is allocated lazily, when we first have to store a
  string.

- allocated contains the size of the strings array, or -1 if
  allocation failed.

- count contains the actual count of strings in the strings array.

- maximum limits the number of strings we can store in the strings
  array: beyond, the extractor returns 0 to short-cut the search.

extract_string stores in the string list OPENSSL strings,
that must be freed with OPENSSL_free.

*/

typedef struct string_list
{
	char** strings;
	int allocated;
	int count;
	int maximum;
} string_list;

static void string_list_initialize(string_list* list)
{
	list->strings = 0;
	list->allocated = 0;
	list->count = 0;
	list->maximum = INT_MAX;
}

static void string_list_allocate(string_list* list, int allocate_count)
{
	if (!list->strings && list->allocated == 0)
	{
		list->strings = calloc((size_t)allocate_count, sizeof(char*));
		list->allocated = list->strings ? allocate_count : -1;
		list->count = 0;
	}
}

static void string_list_free(string_list* list)
{
	/* Note: we don't free the contents of the strings array: this */
	/* is handled by the caller,  either by returning this */
	/* content,  or freeing it itself. */
	free(list->strings);
}

static int extract_string(GENERAL_NAME* name, void* data, int index, int count)
{
	string_list* list = data;
	unsigned char* cstring = 0;
	ASN1_STRING* str;

	switch (name->type)
	{
		case GEN_URI:
			str = name->d.uniformResourceIdentifier;
			break;

		case GEN_DNS:
			str = name->d.dNSName;
			break;

		case GEN_EMAIL:
			str = name->d.rfc822Name;
			break;

		default:
			return 1;
	}

	if ((ASN1_STRING_to_UTF8(&cstring, str)) < 0)
	{
		WLog_ERR(TAG, "ASN1_STRING_to_UTF8() failed for %s: %s",
		         general_name_type_label(name->type), ERR_error_string(ERR_get_error(), NULL));
		return 1;
	}

	string_list_allocate(list, count);

	if (list->allocated <= 0)
	{
		OPENSSL_free(cstring);
		return 0;
	}

	list->strings[list->count] = (char*)cstring;
	list->count++;

	if (list->count >= list->maximum)
	{
		return 0;
	}

	return 1;
}

/*
extract_othername_object --  object extractor.

- the objects array is allocated lazily, when we first have to store a
  string.

- allocated contains the size of the objects array, or -1 if
  allocation failed.

- count contains the actual count of objects in the objects array.

- maximum limits the number of objects we can store in the objects
  array: beyond, the extractor returns 0 to short-cut the search.

extract_othername_objects stores in the objects array ASN1_TYPE *
pointers directly obtained from the GENERAL_NAME.
*/

typedef struct object_list
{
	ASN1_OBJECT* type_id;
	char** strings;
	int allocated;
	int count;
	int maximum;
} object_list;

static void object_list_initialize(object_list* list)
{
	list->type_id = 0;
	list->strings = 0;
	list->allocated = 0;
	list->count = 0;
	list->maximum = INT_MAX;
}

static void object_list_allocate(object_list* list, int allocate_count)
{
	if (!list->strings && list->allocated == 0)
	{
		list->strings = calloc(allocate_count, sizeof(list->strings[0]));
		list->allocated = list->strings ? allocate_count : -1;
		list->count = 0;
	}
}

static char* object_string(ASN1_TYPE* object)
{
	char* result;
	unsigned char* utf8String;
	int length;
	/* TODO: check that object.type is a string type. */
	length = ASN1_STRING_to_UTF8(&utf8String, object->value.asn1_string);

	if (length < 0)
	{
		return 0;
	}

	result = (char*)_strdup((char*)utf8String);
	OPENSSL_free(utf8String);
	return result;
}

static void object_list_free(object_list* list)
{
	free(list->strings);
}

static int extract_othername_object_as_string(GENERAL_NAME* name, void* data, int index, int count)
{
	object_list* list = data;

	if (name->type != GEN_OTHERNAME)
	{
		return 1;
	}

	if (0 != OBJ_cmp(name->d.otherName->type_id, list->type_id))
	{
		return 1;
	}

	object_list_allocate(list, count);

	if (list->allocated <= 0)
	{
		return 0;
	}

	list->strings[list->count] = object_string(name->d.otherName->value);

	if (list->strings[list->count])
	{
		list->count++;
	}

	if (list->count >= list->maximum)
	{
		return 0;
	}

	return 1;
}

/*
crypto_cert_get_email returns a dynamically allocated copy of the
first email found in the subjectAltNames (use free to free it).
*/

char* crypto_cert_get_email(X509* x509)
{
	char* result = 0;
	string_list list;
	string_list_initialize(&list);
	list.maximum = 1;
	map_subject_alt_name(x509, GEN_EMAIL, extract_string, &list);

	if (list.count == 0)
	{
		string_list_free(&list);
		return 0;
	}

	result = _strdup(list.strings[0]);
	OPENSSL_free(list.strings[0]);
	string_list_free(&list);
	return result;
}

/*
crypto_cert_get_upn returns a dynamically allocated copy of the
first UPN otherNames in the subjectAltNames (use free to free it).
Note: if this first UPN otherName is not a string, then 0 is returned,
instead of searching for another UPN that would be a string.
*/

char* crypto_cert_get_upn(X509* x509)
{
	char* result = 0;
	object_list list;
	object_list_initialize(&list);
	list.type_id = OBJ_nid2obj(NID_ms_upn);
	list.maximum = 1;
	map_subject_alt_name(x509, GEN_OTHERNAME, extract_othername_object_as_string, &list);

	if (list.count == 0)
	{
		object_list_free(&list);
		return 0;
	}

	result = list.strings[0];
	object_list_free(&list);
	return result;
}

/* Deprecated name.*/
void crypto_cert_subject_alt_name_free(int count, int* lengths, char** alt_names)
{
	crypto_cert_dns_names_free(count, lengths, alt_names);
}

void crypto_cert_dns_names_free(int count, int* lengths, char** dns_names)
{
	free(lengths);

	if (dns_names)
	{
		int i;

		for (i = 0; i < count; i++)
		{
			if (dns_names[i])
			{
				OPENSSL_free(dns_names[i]);
			}
		}

		free(dns_names);
	}
}

/* Deprecated name.*/
char** crypto_cert_subject_alt_name(X509* xcert, int* count, int** lengths)
{
	return crypto_cert_get_dns_names(xcert, count, lengths);
}

char** crypto_cert_get_dns_names(X509* x509, int* count, int** lengths)
{
	int i;
	char** result = 0;
	string_list list;
	string_list_initialize(&list);
	map_subject_alt_name(x509, GEN_DNS, extract_string, &list);
	(*count) = list.count;

	if (list.count == 0)
	{
		string_list_free(&list);
		return NULL;
	}

	/* lengths are not useful,  since we converted the
	   strings to utf-8,  there cannot be nul-bytes in them. */
	result = calloc(list.count, sizeof(*result));
	(*lengths) = calloc(list.count, sizeof(**lengths));

	if (!result || !(*lengths))
	{
		string_list_free(&list);
		free(result);
		free(*lengths);
		(*lengths) = 0;
		(*count) = 0;
		return NULL;
	}

	for (i = 0; i < list.count; i++)
	{
		result[i] = list.strings[i];
		(*lengths)[i] = strlen(result[i]);
	}

	string_list_free(&list);
	return result;
}

char* crypto_cert_issuer(X509* xcert)
{
	char* issuer;
	if (!xcert)
	{
		WLog_ERR(TAG, "Invalid certificate %p", xcert);
		return NULL;
	}
	issuer = crypto_print_name(X509_get_issuer_name(xcert));
	if (!issuer)
		WLog_ERR(TAG, "certificate does not have an issuer!");
	return issuer;
}

BOOL crypto_check_eku(X509* xcert, int nid)
{
	BOOL ret = FALSE;
	STACK_OF(ASN1_OBJECT) * oid_stack;
	ASN1_OBJECT* oid;

	if (!xcert)
		return FALSE;

	oid = OBJ_nid2obj(nid);
	if (!oid)
		return FALSE;

	oid_stack = X509_get_ext_d2i(xcert, NID_ext_key_usage, NULL, NULL);
	if (!oid_stack)
		return FALSE;

	if (sk_ASN1_OBJECT_find(oid_stack, oid) >= 0)
		ret = TRUE;

	sk_ASN1_OBJECT_pop_free(oid_stack, ASN1_OBJECT_free);
	return ret;
}

static int verify_cb(int ok, X509_STORE_CTX* csc)
{
	if (ok != 1)
	{
		int err = X509_STORE_CTX_get_error(csc);
		int derr = X509_STORE_CTX_get_error_depth(csc);
		X509* where = X509_STORE_CTX_get_current_cert(csc);
		const char* what = X509_verify_cert_error_string(err);
		char* name = crypto_cert_subject(where);

		WLog_WARN(TAG, "Certificate verification failure '%s (%d)' at stack position %d", what, err,
		          derr);
		WLog_WARN(TAG, "%s", name);

		free(name);
	}
	return ok;
}

BOOL x509_verify_certificate(CryptoCert cert, const char* certificate_store_path)
{
	size_t i;
	const int purposes[3] = { X509_PURPOSE_SSL_SERVER, X509_PURPOSE_SSL_CLIENT, X509_PURPOSE_ANY };
	X509_STORE_CTX* csc;
	BOOL status = FALSE;
	X509_STORE* cert_ctx = NULL;
	X509_LOOKUP* lookup = NULL;
	cert_ctx = X509_STORE_new();

	if (cert_ctx == NULL)
		goto end;

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
	OpenSSL_add_all_algorithms();
#else
	OPENSSL_init_crypto(OPENSSL_INIT_ADD_ALL_CIPHERS | OPENSSL_INIT_ADD_ALL_DIGESTS |
	                        OPENSSL_INIT_LOAD_CONFIG,
	                    NULL);
#endif

	if (X509_STORE_set_default_paths(cert_ctx) != 1)
		goto end;

	lookup = X509_STORE_add_lookup(cert_ctx, X509_LOOKUP_hash_dir());

	if (lookup == NULL)
		goto end;

	X509_LOOKUP_add_dir(lookup, NULL, X509_FILETYPE_DEFAULT);

	if (certificate_store_path != NULL)
	{
		X509_LOOKUP_add_dir(lookup, certificate_store_path, X509_FILETYPE_PEM);
	}

	X509_STORE_set_flags(cert_ctx, 0);

	for (i = 0; i < ARRAYSIZE(purposes); i++)
	{
		int err = -1, rc = -1;
		int purpose = purposes[i];
		csc = X509_STORE_CTX_new();

		if (csc == NULL)
			goto skip;
		if (!X509_STORE_CTX_init(csc, cert_ctx, cert->px509, cert->px509chain))
			goto skip;

		X509_STORE_CTX_set_purpose(csc, purpose);
		X509_STORE_CTX_set_verify_cb(csc, verify_cb);

		rc = X509_verify_cert(csc);
		err = X509_STORE_CTX_get_error(csc);
	skip:
		X509_STORE_CTX_free(csc);
		if (rc == 1)
		{
			status = TRUE;
			break;
		}
		else if (err != X509_V_ERR_INVALID_PURPOSE)
			break;
	}

	X509_STORE_free(cert_ctx);
end:
	return status;
}

rdpCertificateData* crypto_get_certificate_data(X509* xcert, const char* hostname, UINT16 port)
{
	char* pem = NULL;
	size_t length;
	rdpCertificateData* certdata = NULL;

	pem = (char*)crypto_cert_pem(xcert, NULL, &length);
	if (!pem)
		goto fail;
	certdata = certificate_data_new(hostname, port);
	if (!certdata)
		goto fail;
	if (!certificate_data_set_pem(certdata, pem))
		goto fail;
	free(pem);
	return certdata;
fail:
	WLog_WARN(TAG, "Failed to extract PEM from X509=%p for host %s:%" PRIu16, xcert, hostname,
	          port);
	certificate_data_free(certdata);
	free(pem);
	return NULL;
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
		WLog_ERR(TAG, "error computing fingerprint");
		goto out_free_issuer;
	}

	WLog_INFO(TAG, "Certificate details:");
	WLog_INFO(TAG, "\tSubject: %s", subject);
	WLog_INFO(TAG, "\tIssuer: %s", issuer);
	WLog_INFO(TAG, "\tThumbprint: %s", fp);
	WLog_INFO(TAG,
	          "The above X.509 certificate could not be verified, possibly because you do not have "
	          "the CA certificate in your certificate store, or the certificate has expired. "
	          "Please look at the OpenSSL documentation on how to add a private CA to the store.");
	free(fp);
out_free_issuer:
	free(issuer);
	free(subject);
}

BYTE* crypto_cert_pem(X509* xcert, STACK_OF(X509) * chain, size_t* plength)
{
	BIO* bio;
	int status, count, x;
	size_t offset;
	size_t length = 0;
	BOOL rc = FALSE;
	BYTE* pemCert = NULL;

	if (!xcert || !plength)
		return NULL;

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

	status = PEM_write_bio_X509(bio, xcert);

	if (status < 0)
	{
		WLog_ERR(TAG, "PEM_write_bio_X509 failure: %d", status);
		goto fail;
	}

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

	offset = 0;
	length = 2048;
	pemCert = (BYTE*)malloc(length + 1);

	if (!pemCert)
	{
		WLog_ERR(TAG, "error allocating pemCert");
		goto fail;
	}

	ERR_clear_error();
	status = BIO_read(bio, pemCert, length);

	if (status < 0)
	{
		WLog_ERR(TAG, "failed to read certificate");
		goto fail;
	}

	offset += (size_t)status;

	while (offset >= length)
	{
		int new_len;
		BYTE* new_cert;
		new_len = length * 2;
		new_cert = (BYTE*)realloc(pemCert, new_len + 1);

		if (!new_cert)
			goto fail;

		length = new_len;
		pemCert = new_cert;
		ERR_clear_error();
		status = BIO_read(bio, &pemCert[offset], length - offset);

		if (status < 0)
			break;

		offset += status;
	}

	if (status < 0)
	{
		WLog_ERR(TAG, "failed to read certificate");
		goto fail;
	}

	length = offset;
	pemCert[length] = '\0';
	*plength = length;
	rc = TRUE;
fail:

	if (!rc)
	{
		WLog_ERR(TAG, "Failed to extract PEM from certificate %p", xcert);
		free(pemCert);
		pemCert = NULL;
	}

	BIO_free_all(bio);
	return pemCert;
}

X509* crypto_cert_from_pem(const char* data, size_t len, BOOL fromFile)
{
	X509* x509 = NULL;
	BIO* bio;
	if (fromFile)
		bio = BIO_new_file(data, "rb");
	else
		bio = BIO_new_mem_buf(data, len);

	if (!bio)
	{
		WLog_ERR(TAG, "BIO_new failed for certificate");
		return NULL;
	}

	x509 = PEM_read_bio_X509(bio, NULL, NULL, 0);
	BIO_free_all(bio);
	if (!x509)
		WLog_ERR(TAG, "PEM_read_bio_X509 returned NULL [input length %" PRIuz "]", len);

	return x509;
}

WINPR_MD_TYPE crypto_cert_get_signature_alg(X509* xcert)
{
	WINPR_ASSERT(xcert);

	const int nid = X509_get_signature_nid(xcert);

	int hash_nid = 0;
	if (OBJ_find_sigid_algs(nid, &hash_nid, NULL) != 1)
		return WINPR_MD_NONE;

	switch (hash_nid)
	{
		case NID_md2:
			return WINPR_MD_MD2;
		case NID_md4:
			return WINPR_MD_MD4;
		case NID_md5:
			return WINPR_MD_MD5;
		case NID_sha1:
			return WINPR_MD_SHA1;
		case NID_sha224:
			return WINPR_MD_SHA224;
		case NID_sha256:
			return WINPR_MD_SHA256;
		case NID_sha384:
			return WINPR_MD_SHA384;
		case NID_sha512:
			return WINPR_MD_SHA512;
		case NID_ripemd160:
			return WINPR_MD_RIPEMD160;
#if (OPENSSL_VERSION_NUMBER >= 0x1010101fL) || defined(LIBRESSL_VERSION_NUMBER)
		case NID_sha3_224:
			return WINPR_MD_SHA3_224;
		case NID_sha3_256:
			return WINPR_MD_SHA3_256;
		case NID_sha3_384:
			return WINPR_MD_SHA3_384;
		case NID_sha3_512:
			return WINPR_MD_SHA3_512;
#endif
		case NID_shake128:
			return WINPR_MD_SHAKE128;
		case NID_shake256:
			return WINPR_MD_SHAKE256;
		case NID_undef:
		default:
			return WINPR_MD_NONE;
	}
}

/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Cryptographic Abstraction Layer
 *
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2023 Armin Novak <anovak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/string.h>
#include <winpr/assert.h>

#include <freerdp/log.h>

#include "x509_utils.h"

#define TAG FREERDP_TAG("crypto")

BYTE* x509_utils_get_hash(const X509* xcert, const char* hash, size_t* length)
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
		WLog_ERR(TAG, "Invalid arugments: xcert=%p, length=%p", xcert, length);
		return NULL;
	}

	fp = calloc(fp_len + 1, sizeof(BYTE));
	if (!fp)
	{
		WLog_ERR(TAG, "could not allocate %" PRIuz " bytes", fp_len);
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

static char* crypto_print_name(const X509_NAME* name)
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

char* x509_utils_get_subject(const X509* xcert)
{
	char* subject;
	if (!xcert)
	{
		WLog_ERR(TAG, "Invalid certificate %p", xcert);
		return NULL;
	}
	subject = crypto_print_name(X509_get_subject_name(xcert));
	if (!subject)
		WLog_WARN(TAG, "certificate does not have a subject!");
	return subject;
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

static void map_subject_alt_name(const X509* x509, int general_name_type,
                                 general_name_mapper_pr mapper, void* data)
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

char* x509_utils_get_email(const X509* x509)
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

char* x509_utils_get_upn(const X509* x509)
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

void x509_utils_dns_names_free(size_t count, size_t* lengths, char** dns_names)
{
	free(lengths);

	if (dns_names)
	{
		for (size_t i = 0; i < count; i++)
		{
			if (dns_names[i])
			{
				OPENSSL_free(dns_names[i]);
			}
		}

		free(dns_names);
	}
}

char** x509_utils_get_dns_names(const X509* x509, size_t* count, size_t** lengths)
{
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

	for (int i = 0; i < list.count; i++)
	{
		result[i] = list.strings[i];
		(*lengths)[i] = strlen(result[i]);
	}

	string_list_free(&list);
	return result;
}

char* x509_utils_get_issuer(const X509* xcert)
{
	char* issuer;
	if (!xcert)
	{
		WLog_ERR(TAG, "Invalid certificate %p", xcert);
		return NULL;
	}
	issuer = crypto_print_name(X509_get_issuer_name(xcert));
	if (!issuer)
		WLog_WARN(TAG, "certificate does not have an issuer!");
	return issuer;
}

BOOL x509_utils_check_eku(const X509* xcert, int nid)
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

void x509_utils_print_info(const X509* xcert)
{
	char* fp;
	char* issuer;
	char* subject;
	subject = x509_utils_get_subject(xcert);
	issuer = x509_utils_get_issuer(xcert);
	fp = (char*)x509_utils_get_hash(xcert, "sha256", NULL);

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

static BYTE* x509_utils_get_pem(const X509* xcert, const STACK_OF(X509) * chain, size_t* plength)
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

	status = PEM_write_bio_X509(bio, (X509*)xcert);

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

X509* x509_utils_from_pem(const char* data, size_t len, BOOL fromFile)
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

WINPR_MD_TYPE x509_utils_get_signature_alg(const X509* xcert)
{
	WINPR_ASSERT(xcert);

	EVP_PKEY* evp = X509_get0_pubkey(xcert);
	WINPR_ASSERT(evp);

	int nid = 0;
	const int res = EVP_PKEY_get_default_digest_nid(evp, &nid);
	if (res <= 0)
		return WINPR_MD_NONE;

	switch (nid)
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
#if (OPENSSL_VERSION_NUMBER >= 0x1010101fL) && !defined(LIBRESSL_VERSION_NUMBER)
		case NID_sha3_224:
			return WINPR_MD_SHA3_224;
		case NID_sha3_256:
			return WINPR_MD_SHA3_256;
		case NID_sha3_384:
			return WINPR_MD_SHA3_384;
		case NID_sha3_512:
			return WINPR_MD_SHA3_512;
		case NID_shake128:
			return WINPR_MD_SHAKE128;
		case NID_shake256:
			return WINPR_MD_SHAKE256;
#endif
		case NID_undef:
		default:
			return WINPR_MD_NONE;
	}
}

char* x509_utils_get_common_name(const X509* xcert, size_t* plength)
{
	X509_NAME* subject_name = X509_get_subject_name(xcert);
	if (subject_name == NULL)
		return NULL;

	const int index = X509_NAME_get_index_by_NID(subject_name, NID_commonName, -1);
	if (index < 0)
		return NULL;

	const X509_NAME_ENTRY* entry = X509_NAME_get_entry(subject_name, index);
	if (entry == NULL)
		return NULL;

	const ASN1_STRING* entry_data = X509_NAME_ENTRY_get_data(entry);
	if (entry_data == NULL)
		return NULL;

	BYTE* common_name_raw = NULL;
	const int length = ASN1_STRING_to_UTF8(&common_name_raw, entry_data);
	if (length < 0)
		return NULL;

	if (plength)
		*plength = (size_t)length;

	char* common_name = _strdup((char*)common_name_raw);
	OPENSSL_free(common_name_raw);
	return common_name;
}

static int verify_cb(int ok, X509_STORE_CTX* csc)
{
	if (ok != 1)
	{
		WINPR_ASSERT(csc);
		int err = X509_STORE_CTX_get_error(csc);
		int derr = X509_STORE_CTX_get_error_depth(csc);
		X509* where = X509_STORE_CTX_get_current_cert(csc);
		const char* what = X509_verify_cert_error_string(err);
		char* name = x509_utils_get_subject(where);

		WLog_WARN(TAG, "Certificate verification failure '%s (%d)' at stack position %d", what, err,
		          derr);
		WLog_WARN(TAG, "%s", name);

		free(name);
	}
	return ok;
}

BOOL x509_utils_verify(X509* xcert, STACK_OF(X509) * chain, const char* certificate_store_path)
{
	const int purposes[3] = { X509_PURPOSE_SSL_SERVER, X509_PURPOSE_SSL_CLIENT, X509_PURPOSE_ANY };
	X509_STORE_CTX* csc = NULL;
	BOOL status = FALSE;
	X509_LOOKUP* lookup = NULL;

	if (!xcert)
		return FALSE;

	X509_STORE* cert_ctx = X509_STORE_new();

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

	for (size_t i = 0; i < ARRAYSIZE(purposes); i++)
	{
		int err = -1, rc = -1;
		int purpose = purposes[i];
		csc = X509_STORE_CTX_new();

		if (csc == NULL)
			goto skip;
		if (!X509_STORE_CTX_init(csc, cert_ctx, xcert, chain))
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

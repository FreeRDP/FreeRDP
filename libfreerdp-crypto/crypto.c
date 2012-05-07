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

#include <freerdp/crypto/crypto.h>

CryptoSha1 crypto_sha1_init(void)
{
	CryptoSha1 sha1 = xmalloc(sizeof(*sha1));
	SHA1_Init(&sha1->sha_ctx);
	return sha1;
}

void crypto_sha1_update(CryptoSha1 sha1, const uint8* data, uint32 length)
{
	SHA1_Update(&sha1->sha_ctx, data, length);
}

void crypto_sha1_final(CryptoSha1 sha1, uint8* out_data)
{
	SHA1_Final(out_data, &sha1->sha_ctx);
	xfree(sha1);
}

CryptoMd5 crypto_md5_init(void)
{
	CryptoMd5 md5 = xmalloc(sizeof(*md5));
	MD5_Init(&md5->md5_ctx);
	return md5;
}

void crypto_md5_update(CryptoMd5 md5, const uint8* data, uint32 length)
{
	MD5_Update(&md5->md5_ctx, data, length);
}

void crypto_md5_final(CryptoMd5 md5, uint8* out_data)
{
	MD5_Final(out_data, &md5->md5_ctx);
	xfree(md5);
}

CryptoRc4 crypto_rc4_init(const uint8* key, uint32 length)
{
	CryptoRc4 rc4 = xmalloc(sizeof(*rc4));
	RC4_set_key(&rc4->rc4_key, length, key);
	return rc4;
}

void crypto_rc4(CryptoRc4 rc4, uint32 length, const uint8* in_data, uint8* out_data)
{
	RC4(&rc4->rc4_key, length, in_data, out_data);
}

void crypto_rc4_free(CryptoRc4 rc4)
{
	xfree(rc4);
}

CryptoDes3 crypto_des3_encrypt_init(const uint8* key, const uint8* ivec)
{
	CryptoDes3 des3 = xmalloc(sizeof(*des3));
	EVP_CIPHER_CTX_init(&des3->des3_ctx);
	EVP_EncryptInit_ex(&des3->des3_ctx, EVP_des_ede3_cbc(), NULL, key, ivec);
	EVP_CIPHER_CTX_set_padding(&des3->des3_ctx, 0);
	return des3;
}

CryptoDes3 crypto_des3_decrypt_init(const uint8* key, const uint8* ivec)
{
	CryptoDes3 des3 = xmalloc(sizeof(*des3));
	EVP_CIPHER_CTX_init(&des3->des3_ctx);
	EVP_DecryptInit_ex(&des3->des3_ctx, EVP_des_ede3_cbc(), NULL, key, ivec);
	EVP_CIPHER_CTX_set_padding(&des3->des3_ctx, 0);
	return des3;
}

void crypto_des3_encrypt(CryptoDes3 des3, uint32 length, const uint8* in_data, uint8* out_data)
{
	int len;
	EVP_EncryptUpdate(&des3->des3_ctx, out_data, &len, in_data, length);
}

void crypto_des3_decrypt(CryptoDes3 des3, uint32 length, const uint8* in_data, uint8* out_data)
{
	int len;
	EVP_DecryptUpdate(&des3->des3_ctx, out_data, &len, in_data, length);

	if (length != len)
		abort(); /* TODO */
}

void crypto_des3_free(CryptoDes3 des3)
{
	if (des3 == NULL)
		return;
	EVP_CIPHER_CTX_cleanup(&des3->des3_ctx);
	xfree(des3);
}

CryptoHmac crypto_hmac_new(void)
{
	CryptoHmac hmac = xmalloc(sizeof(*hmac));
	HMAC_CTX_init(&hmac->hmac_ctx);
	return hmac;
}

void crypto_hmac_sha1_init(CryptoHmac hmac, const uint8* data, uint32 length)
{
	HMAC_Init_ex(&hmac->hmac_ctx, data, length, EVP_sha1(), NULL);
}

void crypto_hmac_update(CryptoHmac hmac, const uint8* data, uint32 length)
{
	HMAC_Update(&hmac->hmac_ctx, data, length);
}

void crypto_hmac_final(CryptoHmac hmac, uint8* out_data, uint32 length)
{
	HMAC_Final(&hmac->hmac_ctx, out_data, &length);
}

void crypto_hmac_free(CryptoHmac hmac)
{
	if (hmac == NULL)
		return;
	HMAC_CTX_cleanup(&hmac->hmac_ctx);
	xfree(hmac);
}

CryptoCert crypto_cert_read(uint8* data, uint32 length)
{
	CryptoCert cert = xmalloc(sizeof(*cert));
	/* this will move the data pointer but we don't care, we don't use it again */
	cert->px509 = d2i_X509(NULL, (D2I_X509_CONST uint8 **) &data, length);
	return cert;
}

void crypto_cert_free(CryptoCert cert)
{
	if (cert == NULL)
		return;
	X509_free(cert->px509);
	xfree(cert);
}

boolean crypto_cert_get_public_key(CryptoCert cert, rdpBlob* public_key)
{
	uint8* p;
	int length;
	boolean status = true;
	EVP_PKEY* pkey = NULL;

	pkey = X509_get_pubkey(cert->px509);

	if (!pkey)
	{
		printf("crypto_cert_get_public_key: X509_get_pubkey() failed\n");
		status = false;
		goto exit;
	}

	length = i2d_PublicKey(pkey, NULL);

	if (length < 1)
	{
		printf("crypto_cert_get_public_key: i2d_PublicKey() failed\n");
		status = false;
		goto exit;
	}

	freerdp_blob_alloc(public_key, length);
	p = (uint8*) public_key->data;
	i2d_PublicKey(pkey, &p);

exit:
	if (pkey)
		EVP_PKEY_free(pkey);

	return status;
}

static void crypto_rsa_common(const uint8* input, int length, uint32 key_length, const uint8* modulus, const uint8* exponent, int exponent_size, uint8* output)
{
	BN_CTX* ctx;
	int output_length;
	uint8* input_reverse;
	uint8* modulus_reverse;
	uint8* exponent_reverse;
	BIGNUM mod, exp, x, y;

	input_reverse = (uint8*) xmalloc(2 * key_length + exponent_size);
	modulus_reverse = input_reverse + key_length;
	exponent_reverse = modulus_reverse + key_length;

	memcpy(modulus_reverse, modulus, key_length);
	crypto_reverse(modulus_reverse, key_length);
	memcpy(exponent_reverse, exponent, exponent_size);
	crypto_reverse(exponent_reverse, exponent_size);
	memcpy(input_reverse, input, length);
	crypto_reverse(input_reverse, length);

	ctx = BN_CTX_new();
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
	xfree(input_reverse);
}

static void crypto_rsa_public(const uint8* input, int length, uint32 key_length, const uint8* modulus, const uint8* exponent, uint8* output)
{
	crypto_rsa_common(input, length, key_length, modulus, exponent, EXPONENT_MAX_SIZE, output);
}

static void crypto_rsa_private(const uint8* input, int length, uint32 key_length, const uint8* modulus, const uint8* private_exponent, uint8* output)
{

	crypto_rsa_common(input, length, key_length, modulus, private_exponent, key_length, output);
}

void crypto_rsa_public_encrypt(const uint8* input, int length, uint32 key_length, const uint8* modulus, const uint8* exponent, uint8* output)
{

	crypto_rsa_public(input, length, key_length, modulus, exponent, output);
}

void crypto_rsa_public_decrypt(const uint8* input, int length, uint32 key_length, const uint8* modulus, const uint8* exponent, uint8* output)
{

	crypto_rsa_public(input, length, key_length, modulus, exponent, output);
}

void crypto_rsa_private_encrypt(const uint8* input, int length, uint32 key_length, const uint8* modulus, const uint8* private_exponent, uint8* output)
{

	crypto_rsa_private(input, length, key_length, modulus, private_exponent, output);
}

void crypto_rsa_private_decrypt(const uint8* input, int length, uint32 key_length, const uint8* modulus, const uint8* private_exponent, uint8* output)
{

	crypto_rsa_private(input, length, key_length, modulus, private_exponent, output);
}

void crypto_rsa_decrypt(const uint8* input, int length, uint32 key_length, const uint8* modulus, const uint8* private_exponent, uint8* output)
{

	crypto_rsa_common(input, length, key_length, modulus, private_exponent, key_length, output);
}

void crypto_reverse(uint8* data, int length)
{
	int i, j;
	uint8 temp;

	for (i = 0, j = length - 1; i < j; i++, j--)
	{
		temp = data[i];
		data[i] = data[j];
		data[j] = temp;
	}
}

void crypto_nonce(uint8* nonce, int size)
{
	RAND_bytes((void*) nonce, size);
}

char* crypto_cert_fingerprint(X509* xcert)
{
	int i = 0;
	char* p;
	char* fp_buffer;
	uint32 fp_len;
	uint8 fp[EVP_MAX_MD_SIZE];

	X509_digest(xcert, EVP_sha1(), fp, &fp_len);

	fp_buffer = (char*) xzalloc(3 * fp_len);
	p = fp_buffer;

	for (i = 0; i < (int) (fp_len - 1); i++)
	{
		sprintf(p, "%02x:", fp[i]);
		p = &fp_buffer[i * 3];
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
		buffer = xzalloc(size + 1);
		memset(buffer, 0, size + 1);
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
	uint8* common_name;
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

	*length = ASN1_STRING_to_UTF8(&common_name, entry_data);

	if (*length < 0)
		return NULL;

	return (char*) common_name;
}

char** crypto_cert_subject_alt_name(X509* xcert, int* count, int** lengths)
{
	int index;
	int length;
	char** strings;
	uint8* string;
	int num_subject_alt_names;
	GENERAL_NAMES* subject_alt_names;
	GENERAL_NAME* subject_alt_name;

	*count = 0;
	subject_alt_names = X509_get_ext_d2i(xcert, NID_subject_alt_name, 0, 0);

	if (!subject_alt_names)
		return NULL;

	num_subject_alt_names = sk_GENERAL_NAME_num(subject_alt_names);
	strings = (char**) xmalloc(sizeof(char*) * num_subject_alt_names);
	*lengths = (int*) xmalloc(sizeof(int*) * num_subject_alt_names);

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
		return NULL;

	return strings;
}

char* crypto_cert_issuer(X509* xcert)
{
	return crypto_print_name(X509_get_issuer_name(xcert));
}

boolean x509_verify_certificate(CryptoCert cert, char* certificate_store_path)
{
	X509_STORE_CTX* csc;
	boolean status = false;
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
		X509_LOOKUP_add_dir(lookup, certificate_store_path, X509_FILETYPE_ASN1);
	}

	csc = X509_STORE_CTX_new();

	if (csc == NULL)
		goto end;

	X509_STORE_set_flags(cert_ctx, 0);

	if (!X509_STORE_CTX_init(csc, cert_ctx, xcert, 0))
		goto end;

	if (X509_verify_cert(csc) == 1)
		status = true;

	X509_STORE_CTX_free(csc);
	X509_STORE_free(cert_ctx);

end:
	return status;
}

rdpCertificateData* crypto_get_certificate_data(X509* xcert, char* hostname)
{
	char* fp;
	rdpCertificateData* certdata;

	fp = crypto_cert_fingerprint(xcert);
	certdata = certificate_data_new(hostname, fp);
	xfree(fp);

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

	printf("Certificate details:\n");
	printf("\tSubject: %s\n", subject);
	printf("\tIssuer: %s\n", issuer);
	printf("\tThumbprint: %s\n", fp);
	printf("The above X.509 certificate could not be verified, possibly because you do not have "
			"the CA certificate in your certificate store, or the certificate has expired. "
			"Please look at the documentation on how to create local certificate store for a private CA.\n");

	xfree(subject);
	xfree(issuer);
	xfree(fp);
}

char* crypto_base64_encode(uint8* data, int length)
{
	BIO* bmem;
	BIO* b64;
	BUF_MEM *bptr;
	char* base64_string;

	b64 = BIO_new(BIO_f_base64());
	BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
	bmem = BIO_new(BIO_s_mem());
	b64 = BIO_push(b64, bmem);
	BIO_write(b64, data, length);

	if (BIO_flush(b64) < 1)
		return NULL;

	BIO_get_mem_ptr(b64, &bptr);

	base64_string = xmalloc(bptr->length);
	memcpy(base64_string, bptr->data, bptr->length - 1);
	base64_string[bptr->length - 1] = '\0';

	BIO_free_all(b64);

	return base64_string;
}

void crypto_base64_decode(uint8* enc_data, int length, uint8** dec_data, int* res_length)
{
      BIO *b64, *bmem;

      *dec_data = xmalloc(length);
      memset(*dec_data, 0, length);

      b64 = BIO_new(BIO_f_base64());
      BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
      bmem = BIO_new_mem_buf(enc_data, length);
      bmem = BIO_push(b64, bmem);

      *res_length = BIO_read(bmem, *dec_data, length);

      BIO_free_all(bmem);
}

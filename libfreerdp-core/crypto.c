/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Cryptographic Abstraction Layer
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

#include "crypto.h"

CryptoSha1 crypto_sha1_init(void)
{
	CryptoSha1 sha1 = xmalloc(sizeof(*sha1));
	SHA1_Init(&sha1->sha_ctx);
	return sha1;
}

void crypto_sha1_update(CryptoSha1 sha1, uint8* data, uint32 length)
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

void crypto_md5_update(CryptoMd5 md5, uint8* data, uint32 length)
{
	MD5_Update(&md5->md5_ctx, data, length);
}

void crypto_md5_final(CryptoMd5 md5, uint8* out_data)
{
	MD5_Final(out_data, &md5->md5_ctx);
	xfree(md5);
}

CryptoRc4 crypto_rc4_init(uint8* key, uint32 length)
{
	CryptoRc4 rc4 = xmalloc(sizeof(*rc4));
	RC4_set_key(&rc4->rc4_key, length, key);
	return rc4;
}

void crypto_rc4(CryptoRc4 rc4, uint32 length, uint8* in_data, uint8* out_data)
{
	RC4(&rc4->rc4_key, length, in_data, out_data);
}

void crypto_rc4_free(CryptoRc4 rc4)
{
	xfree(rc4);
}

CryptoCert crypto_cert_read(uint8* data, uint32 length)
{
	CryptoCert cert = xmalloc(sizeof(*cert));
	/* this will move the data pointer but we don't care, we don't use it again */
	cert->px509 = d2i_X509(NULL, (D2I_X509_CONST unsigned char **) &data, length);
	return cert;
}

void crypto_cert_free(CryptoCert cert)
{
	X509_free(cert->px509);
	xfree(cert);
}

boolean crypto_cert_verify(CryptoCert server_cert, CryptoCert cacert)
{
	return True; /* FIXME: do the actual verification */
}

boolean crypto_cert_get_public_key(CryptoCert cert, DATABLOB* public_key)
{
	uint8* p;
	int length;
	boolean status = True;
	EVP_PKEY* pkey = NULL;

	pkey = X509_get_pubkey(cert->px509);

	if (!pkey)
	{
		printf("crypto_cert_get_public_key: X509_get_pubkey() failed\n");
		status = False;
		goto exit;
	}

	length = i2d_PublicKey(pkey, NULL);

	if (length < 1)
	{
		printf("crypto_cert_get_public_key: i2d_PublicKey() failed\n");
		status = False;
		goto exit;
	}

	datablob_alloc(public_key, length);
	p = (unsigned char*) public_key->data;
	i2d_PublicKey(pkey, &p);

exit:
	if (pkey)
		EVP_PKEY_free(pkey);

	return status;
}

void crypto_nonce(uint8* nonce, int size)
{
	RAND_bytes((void*) nonce, size);
}

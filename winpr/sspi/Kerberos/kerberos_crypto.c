/**
* WinPR: Windows Portable Runtime
* kerberos Crypto Support
*
* Copyright 2011 Samsung, Author Jiten Pathy
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

#include "kerberos.h"
#include "kerberos_crypto.h"

BYTE* crypto_md4_hash(rdpBlob* blob)
{
	BYTE* hash;
	MD4_CTX md4_ctx;

	hash = (BYTE*) malloc(16);
	ZeroMemory(hash, 16);

	MD4_Init(&md4_ctx);
	MD4_Update(&md4_ctx, blob->data, blob->length);
	MD4_Final(hash, &md4_ctx);

	return hash;
}

KrbENCKey* string2key(rdpBlob* string, sint8 enctype)
{
	KrbENCKey* key;
	key = xnew(KrbENCKey);
	key->enctype = enctype;

	switch(enctype)
	{
		case ETYPE_RC4_HMAC:
			key->skey.data = crypto_md4_hash(string);
			key->skey.length = 16;
			break;
	}

	return key;
}

rdpBlob* crypto_kdcmsg_encrypt_rc4(rdpBlob* msg, BYTE* key, UINT32 msgtype)
{
	rdpBlob* encmsg;
	BYTE* K1;
	BYTE* K3;
	UINT32 len;
	krbEdata* edata;
	CryptoRc4 rc4;

	K1 = xzalloc(16);
	K3 = xzalloc(16);
	len = ((msg->length > 16) ? msg->length : 16);
	len += sizeof(krbEdata);
	edata = xzalloc(len);
	encmsg = xnew(rdpBlob);
	freerdp_blob_alloc(encmsg, len);
	HMAC(EVP_md5(), (void*) key, 16, (BYTE*)&msgtype, 4, (void*) K1, NULL);
	crypto_nonce((BYTE*)(edata->Confounder), 8);
	memcpy(&(edata->data[0]), msg->data, msg->length);
	HMAC(EVP_md5(), (void*) K1, 16, (BYTE*) edata->Confounder, len - 16, (void*) &(edata->Checksum), NULL);
	HMAC(EVP_md5(), (void*) K1, 16, (BYTE*) &(edata->Checksum), 16, (void*) K3, NULL);
	memcpy(encmsg->data, &(edata->Checksum), 16);
	rc4 = crypto_rc4_init(K3, 16);
	crypto_rc4(rc4, len - 16, (BYTE*) edata->Confounder, (BYTE*)(((BYTE*) encmsg->data) + 16));
	crypto_rc4_free(rc4);
	free(K1);
	free(K3);
	free(edata);

	return encmsg;
}

rdpBlob* crypto_kdcmsg_encrypt(rdpBlob* msg, KrbENCKey* key, UINT32 msgtype)
{
	switch(key->enctype)
	{
		case ETYPE_RC4_HMAC:
			if(key->skey.length != 16)
				return NULL;
			return crypto_kdcmsg_encrypt_rc4(msg, key->skey.data, msgtype);
	}

	return NULL;
}

rdpBlob* crypto_kdcmsg_decrypt_rc4(rdpBlob* msg, BYTE* key, UINT32 msgtype)
{
	rdpBlob* decmsg;
	BYTE* K1;
	BYTE* K3;
	UINT32 len;
	krbEdata* edata;
	CryptoRc4 rc4;

	K1 = xzalloc(16);
	K3 = xzalloc(16);
	len = msg->length;
	edata = xzalloc(len);
	HMAC(EVP_md5(), (void*) key, 16, (BYTE*) &msgtype, 4, (void*) K1, NULL);
	HMAC(EVP_md5(), (void*) K1, 16, (BYTE*) msg->data , 16, (void*) K3, NULL);
	rc4 = crypto_rc4_init(K3, 16);
	crypto_rc4(rc4, len - 16, (BYTE*)(((BYTE*) msg->data) + 16), (BYTE*) edata->Confounder);
	crypto_rc4_free(rc4);
	HMAC(EVP_md5(), (void*) K1, 16, (BYTE*)edata->Confounder, len - 16, (void*) &(edata->Checksum), NULL);

	if (memcmp(msg->data, &edata->Checksum, 16))
	{
		free(edata) ;
		free(K1) ;
		free(K3) ;
		return NULL;
	}

	decmsg = xnew(rdpBlob);
	freerdp_blob_alloc(decmsg, len);
	memcpy(decmsg->data, edata, len);

	free(K1);
	free(K3);
	free(edata);

	return decmsg;
}

rdpBlob* crypto_kdcmsg_decrypt(rdpBlob* msg, KrbENCKey* key, UINT32 msgtype)
{
	switch(key->enctype)
	{
		case ETYPE_RC4_HMAC:
			if(key->skey.length != 16)
				return NULL;
			return crypto_kdcmsg_decrypt_rc4(msg, key->skey.data, msgtype);
	}
	return NULL;
}

rdpBlob* crypto_kdcmsg_cksum_hmacmd5(rdpBlob* msg, BYTE* key, UINT32 msgtype)
{
	rdpBlob* cksum;
	BYTE* Ksign;
	BYTE* tmpdata;
	BYTE* tmp;
	CryptoMd5 md5;
	Ksign = xzalloc(16);
	tmp = xzalloc(16);
	cksum = xnew(rdpBlob);
	freerdp_blob_alloc(cksum, 16);
	tmpdata = xzalloc(msg->length + 4);
	HMAC(EVP_md5(), (void*) key, 16, (BYTE*)"signaturekey\0", 13, (void*) Ksign, NULL);
	memcpy(tmpdata, (void*)&msgtype, 4);
	memcpy(tmpdata + 4, msg->data, msg->length);
	md5 = crypto_md5_init();
	crypto_md5_update(md5, tmpdata, msg->length + 4);
	crypto_md5_final(md5, tmp);
	HMAC(EVP_md5(), (void*) Ksign, 16, (BYTE*)tmp, 16, (void*) cksum->data, NULL);
	return cksum;
}

rdpBlob* crypto_kdcmsg_cksum(rdpBlob* msg, KrbENCKey* key, UINT32 msgtype)
{
	switch(key->enctype)
	{
		case ETYPE_RC4_HMAC:
			if(key->skey.length != 16)
				return NULL;
			return crypto_kdcmsg_cksum_hmacmd5(msg, key->skey.data, msgtype);
	}
	return NULL;
}

int get_cksum_type(UINT32 enctype)
{
	switch(enctype)
	{
		case ETYPE_RC4_HMAC:
			return KRB_CKSUM_HMAC_MD5;
	}

	return 0;
}

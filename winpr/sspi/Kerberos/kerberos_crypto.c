/**
* FreeRDP: A Remote Desktop Protocol Client
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

uint8* crypto_md4_hash(rdpBlob* blob)
{
	MD4_CTX md4_ctx;
	uint8* hash;
	hash = (uint8*)xzalloc(16);
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

rdpBlob* crypto_kdcmsg_encrypt_rc4(rdpBlob* msg, uint8* key, uint32 msgtype)
{
	rdpBlob* encmsg;
	uint8* K1;
	uint8* K3;
	uint32 len;
	krbEdata* edata;
	CryptoRc4 rc4;
	K1 = xzalloc(16);
	K3 = xzalloc(16);
	len = ((msg->length > 16) ? msg->length : 16);
	len += sizeof(krbEdata);
	edata = xzalloc(len);
	encmsg = xnew(rdpBlob);
	freerdp_blob_alloc(encmsg, len);
	HMAC(EVP_md5(), (void*) key, 16, (uint8*)&msgtype, 4, (void*) K1, NULL);
	crypto_nonce((uint8*)(edata->Confounder), 8);
	memcpy(&(edata->data[0]), msg->data, msg->length);
	HMAC(EVP_md5(), (void*) K1, 16, (uint8*)edata->Confounder, len - 16, (void*)&(edata->Checksum), NULL);
	HMAC(EVP_md5(), (void*) K1, 16, (uint8*)&(edata->Checksum), 16, (void*)K3, NULL);
	memcpy(encmsg->data, &(edata->Checksum), 16);
	rc4 = crypto_rc4_init(K3, 16);
	crypto_rc4(rc4, len - 16, (uint8*) edata->Confounder, (uint8*)(((uint8*) encmsg->data) + 16));
	crypto_rc4_free(rc4);
	xfree(K1);
	xfree(K3);
	xfree(edata);
	return encmsg;
}

rdpBlob* crypto_kdcmsg_encrypt(rdpBlob* msg, KrbENCKey* key, uint32 msgtype)
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

rdpBlob* crypto_kdcmsg_decrypt_rc4(rdpBlob* msg, uint8* key, uint32 msgtype)
{
	rdpBlob* decmsg;
	uint8* K1;
	uint8* K3;
	uint32 len;
	krbEdata* edata;
	CryptoRc4 rc4;
	K1 = xzalloc(16);
	K3 = xzalloc(16);
	len = msg->length;
	edata = xzalloc(len);
	HMAC(EVP_md5(), (void*) key, 16, (uint8*) &msgtype, 4, (void*) K1, NULL);
	HMAC(EVP_md5(), (void*) K1, 16, (uint8*) msg->data , 16, (void*) K3, NULL);
	rc4 = crypto_rc4_init(K3, 16);
	crypto_rc4(rc4, len - 16, (uint8*)(((uint8*) msg->data) + 16), (uint8*) edata->Confounder);
	crypto_rc4_free(rc4);
	HMAC(EVP_md5(), (void*) K1, 16, (uint8*)edata->Confounder, len - 16, (void*)&(edata->Checksum), NULL);
	if(memcmp(msg->data, &edata->Checksum, 16))
	{
		xfree(edata) ;
		xfree(K1) ;
		xfree(K3) ;
		return NULL;
	}
	decmsg = xnew(rdpBlob);
	freerdp_blob_alloc(decmsg, len);
	memcpy(decmsg->data, edata, len);
	xfree(K1);
	xfree(K3);
	xfree(edata);
	return decmsg;
}

rdpBlob* crypto_kdcmsg_decrypt(rdpBlob* msg, KrbENCKey* key, uint32 msgtype)
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

rdpBlob* crypto_kdcmsg_cksum_hmacmd5(rdpBlob* msg, uint8* key, uint32 msgtype)
{
	rdpBlob* cksum;
	uint8* Ksign;
	uint8* tmpdata;
	uint8* tmp;
	CryptoMd5 md5;
	Ksign = xzalloc(16);
	tmp = xzalloc(16);
	cksum = xnew(rdpBlob);
	freerdp_blob_alloc(cksum, 16);
	tmpdata = xzalloc(msg->length + 4);
	HMAC(EVP_md5(), (void*) key, 16, (uint8*)"signaturekey\0", 13, (void*) Ksign, NULL);
	memcpy(tmpdata, (void*)&msgtype, 4);
	memcpy(tmpdata + 4, msg->data, msg->length);
	md5 = crypto_md5_init();
	crypto_md5_update(md5, tmpdata, msg->length + 4);
	crypto_md5_final(md5, tmp);
	HMAC(EVP_md5(), (void*) Ksign, 16, (uint8*)tmp, 16, (void*) cksum->data, NULL);
	return cksum;
}

rdpBlob* crypto_kdcmsg_cksum(rdpBlob* msg, KrbENCKey* key, uint32 msgtype)
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

int get_cksum_type(uint32 enctype)
{
	switch(enctype)
	{
		case ETYPE_RC4_HMAC:
			return KRB_CKSUM_HMAC_MD5;
	}
	return 0;
}

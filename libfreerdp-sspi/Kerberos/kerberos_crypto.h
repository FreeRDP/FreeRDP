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
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef FREERDP_SSPI_KERBEROS_CRYPTO_H
#define FREERDP_SSPI_KERBEROS_CRYPTO_H

#include <openssl/err.h>
#include <openssl/rc4.h>
#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/bn.h>
#include <openssl/rand.h>

#include <freerdp/crypto/crypto.h>

struct _KRB_EDATA
{
	uint8 Checksum[16];
	uint8 Confounder[8];
	uint8* data[0];
};
typedef struct _KRB_EDATA krbEdata;

int get_cksum_type(uint32 enctype);

uint8* crypto_md4_hash(rdpBlob* blob);
KrbENCKey* string2key(rdpBlob* string, sint8 enctype);

rdpBlob* crypto_kdcmsg_encrypt_rc4(rdpBlob* msg, uint8* key, uint32 msgtype);
rdpBlob* crypto_kdcmsg_encrypt(rdpBlob* msg, KrbENCKey* key, uint32 msgtype);

rdpBlob* crypto_kdcmsg_decrypt_rc4(rdpBlob* msg, uint8* key, uint32 msgtype);
rdpBlob* crypto_kdcmsg_decrypt(rdpBlob* msg, KrbENCKey* key, uint32 msgtype);

rdpBlob* crypto_kdcmsg_cksum_hmacmd5(rdpBlob* msg, uint8* key, uint32 msgtype);
rdpBlob* crypto_kdcmsg_cksum(rdpBlob* msg, KrbENCKey* key, uint32 msgtype);

#endif /* FREERDP_SSPI_KERBEROS_CRYPTO_H */

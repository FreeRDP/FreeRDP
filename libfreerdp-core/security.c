/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RDP Security
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

#include "security.h"

/* 0x36 repeated 40 times */
static uint8 pad1[40] =
{
	"\x36\x36\x36\x36\x36\x36\x36\x36"
	"\x36\x36\x36\x36\x36\x36\x36\x36"
	"\x36\x36\x36\x36\x36\x36\x36\x36"
	"\x36\x36\x36\x36\x36\x36\x36\x36"
	"\x36\x36\x36\x36\x36\x36\x36\x36"
};

/* 0x5C repeated 48 times */
static uint8 pad2[48] =
{
	"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C"
	"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C"
	"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C"
	"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C"
	"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C"
	"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C"
};

void security_salted_hash(uint8* salt, uint8* input, int length, uint8* salt1, uint8* salt2, uint8* output)
{
	CryptoMd5 md5;
	CryptoSha1 sha1;
	uint8 sha1_digest[20];

	/* SaltedHash(Salt, Input, Salt1, Salt2) = MD5(S + SHA1(Input + Salt + Salt1 + Salt2)) */

	/* SHA1_Digest = SHA1(Input + Salt + Salt1 + Salt2) */
	sha1 = crypto_sha1_init();
	crypto_sha1_update(sha1, input, length); /* Input */
	crypto_sha1_update(sha1, salt, 48); /* Salt (48 bytes) */
	crypto_sha1_update(sha1, salt1, 32); /* Salt1 (32 bytes) */
	crypto_sha1_update(sha1, salt2, 32); /* Salt2 (32 bytes) */
	crypto_sha1_final(sha1, sha1_digest);

	/* SaltedHash(Salt, Input, Salt1, Salt2) = MD5(S + SHA1_Digest) */
	md5 = crypto_md5_init();
	crypto_md5_update(md5, salt, 48); /* Salt (48 bytes) */
	crypto_md5_update(md5, sha1_digest, 20); /* SHA1_Digest */
	crypto_md5_final(md5, output);
}

void security_premaster_hash(uint8* input, int length, uint8* premaster_secret, uint8* client_random, uint8* server_random, uint8* output)
{
	/* PremasterHash(Input) = SaltedHash(PremasterSecret, Input, ClientRandom, ServerRandom) */
	security_salted_hash(premaster_secret, input, length, client_random, server_random, output);
}

void security_master_secret(uint8* premaster_secret, uint8* client_random, uint8* server_random, uint8* output)
{
	/* MasterSecret = PremasterHash('A') + PremasterHash('BB') + PremasterHash('CCC') */
	security_premaster_hash("A", 1, premaster_secret, client_random, server_random, &output[0]);
	security_premaster_hash("BB", 2, premaster_secret, client_random, server_random, &output[16]);
	security_premaster_hash("CCC", 3, premaster_secret, client_random, server_random, &output[32]);
}

void security_master_hash(uint8* input, int length, uint8* master_secret, uint8* client_random, uint8* server_random, uint8* output)
{
	/* MasterHash(Input) = SaltedHash(MasterSecret, Input, ServerRandom, ClientRandom) */
	security_salted_hash(master_secret, input, length, server_random, client_random, output);
}

void security_session_key_blob(uint8* master_secret, uint8* client_random, uint8* server_random, uint8* output)
{
	/* MasterHash = MasterHash('A') + MasterHash('BB') + MasterHash('CCC') */
	security_master_hash("A", 1, master_secret, client_random, server_random, &output[0]);
	security_master_hash("BB", 2, master_secret, client_random, server_random, &output[16]);
	security_master_hash("CCC", 3, master_secret, client_random, server_random, &output[32]);
}

void security_mac_salt_key(uint8* session_key_blob, uint8* client_random, uint8* server_random, uint8* output)
{
	/* MacSaltKey = First128Bits(SessionKeyBlob) */
	memcpy(output, session_key_blob, 16);
}

void security_licensing_encryption_key(uint8* session_key_blob, uint8* client_random, uint8* server_random, uint8* output)
{
	CryptoMd5 md5;

	/* LicensingEncryptionKey = MD5(Second128Bits(SessionKeyBlob) + ClientRandom + ServerRandom)) */

	md5 = crypto_md5_init();
	crypto_md5_update(md5, &session_key_blob[16], 16); /* Second128Bits(SessionKeyBlob) */
	crypto_md5_update(md5, client_random, 32); /* ClientRandom */
	crypto_md5_update(md5, server_random, 32); /* ServerRandom */
	crypto_md5_final(md5, output);
}

void security_uint32_le(uint8* output, uint32 value)
{
	output[0] = (value) & 0xFF;
	output[1] = (value >> 8) & 0xFF;
	output[2] = (value >> 16) & 0xFF;
	output[3] = (value >> 24) & 0xFF;
}

void security_mac_data(uint8* mac_salt_key, uint8* data, uint32 length, uint8* output)
{
	CryptoMd5 md5;
	CryptoSha1 sha1;
	uint8 length_le[4];
	uint8 sha1_digest[20];

	/* MacData = MD5(MacSaltKey + pad2 + SHA1(MacSaltKey + pad1 + length + data)) */

	security_uint32_le(length_le, length); /* length must be little-endian */

	/* SHA1_Digest = SHA1(MacSaltKey + pad1 + length + data) */
	sha1 = crypto_sha1_init();
	crypto_sha1_update(sha1, mac_salt_key, 16); /* MacSaltKey */
	crypto_sha1_update(sha1, pad1, sizeof(pad1)); /* pad1 */
	crypto_sha1_update(sha1, length_le, sizeof(length_le)); /* length */
	crypto_sha1_update(sha1, data, length); /* data */
	crypto_sha1_final(sha1, sha1_digest);

	/* MacData = MD5(MacSaltKey + pad2 + SHA1_Digest) */
	md5 = crypto_md5_init();
	crypto_md5_update(md5, mac_salt_key, 16); /* MacSaltKey */
	crypto_md5_update(md5, pad2, sizeof(pad2)); /* pad2 */
	crypto_md5_update(md5, sha1_digest, 20); /* SHA1_Digest */
	crypto_md5_final(md5, output);
}

void security_mac_signature(uint8* mac_key, int mac_key_length, uint8* data, uint32 length, uint8* output)
{
	CryptoMd5 md5;
	CryptoSha1 sha1;
	uint8 length_le[4];
	uint8 md5_digest[16];
	uint8 sha1_digest[20];

	security_uint32_le(length_le, length); /* length must be little-endian */

	/* SHA1_Digest = SHA1(MACKeyN + pad1 + length + data) */
	sha1 = crypto_sha1_init();
	crypto_sha1_update(sha1, mac_key, mac_key_length); /* MacKeyN */
	crypto_sha1_update(sha1, pad1, sizeof(pad1)); /* pad1 */
	crypto_sha1_update(sha1, length_le, sizeof(length_le)); /* length */
	crypto_sha1_update(sha1, data, length); /* data */
	crypto_sha1_final(sha1, sha1_digest);

	/* MACSignature = First64Bits(MD5(MACKeyN + pad2 + SHA1_Digest)) */
	md5 = crypto_md5_init();
	crypto_md5_update(md5, mac_key, mac_key_length); /* MacKeyN */
	crypto_md5_update(md5, pad2, sizeof(pad2)); /* pad2 */
	crypto_md5_update(md5, sha1_digest, 20); /* SHA1_Digest */
	crypto_md5_final(md5, md5_digest);

	memcpy(output, md5_digest, 8);
}

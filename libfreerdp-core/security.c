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

static void security_salted_hash(uint8* salt, uint8* input, int length, uint8* salt1, uint8* salt2, uint8* output)
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

static void security_premaster_hash(char* input, int length, uint8* premaster_secret, uint8* client_random, uint8* server_random, uint8* output)
{
	/* PremasterHash(Input) = SaltedHash(PremasterSecret, Input, ClientRandom, ServerRandom) */
	security_salted_hash(premaster_secret, (uint8*)input, length, client_random, server_random, output);
}

void security_master_secret(uint8* premaster_secret, uint8* client_random, uint8* server_random, uint8* output)
{
	/* MasterSecret = PremasterHash('A') + PremasterHash('BB') + PremasterHash('CCC') */
	security_premaster_hash("A", 1, premaster_secret, client_random, server_random, &output[0]);
	security_premaster_hash("BB", 2, premaster_secret, client_random, server_random, &output[16]);
	security_premaster_hash("CCC", 3, premaster_secret, client_random, server_random, &output[32]);
}

static void security_master_hash(char* input, int length, uint8* master_secret, uint8* client_random, uint8* server_random, uint8* output)
{
	/* MasterHash(Input) = SaltedHash(MasterSecret, Input, ServerRandom, ClientRandom) */
	security_salted_hash(master_secret, (uint8*)input, length, server_random, client_random, output);
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

void security_md5_16_32_32(uint8* in0, uint8* in1, uint8* in2, uint8* output)
{
	CryptoMd5 md5;

	md5 = crypto_md5_init();
	crypto_md5_update(md5, in0, 16);
	crypto_md5_update(md5, in1, 32);
	crypto_md5_update(md5, in2, 32);
	crypto_md5_final(md5, output);
}

void security_licensing_encryption_key(uint8* session_key_blob, uint8* client_random, uint8* server_random, uint8* output)
{
	/* LicensingEncryptionKey = MD5(Second128Bits(SessionKeyBlob) + ClientRandom + ServerRandom)) */
	security_md5_16_32_32(&session_key_blob[16], client_random, server_random, output);
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

static void security_A(uint8* master_secret, uint8* client_random, uint8* server_random, uint8* output)
{
	security_premaster_hash("A", 1, master_secret, client_random, server_random, &output[0]);
	security_premaster_hash("BB", 2, master_secret, client_random, server_random, &output[16]);
	security_premaster_hash("CCC", 3, master_secret, client_random, server_random, &output[32]);
}

static void security_X(uint8* master_secret, uint8* client_random, uint8* server_random, uint8* output)
{
	security_premaster_hash("X", 1, master_secret, client_random, server_random, &output[0]);
	security_premaster_hash("YY", 2, master_secret, client_random, server_random, &output[16]);
	security_premaster_hash("ZZZ", 3, master_secret, client_random, server_random, &output[32]);
}

boolean security_establish_keys(uint8* client_random, rdpSettings* settings)
{
	uint8 pre_master_secret[48];
	uint8 master_secret[48];
	uint8 session_key_blob[48];
	uint8* server_random;
	uint8 salt40[] = { 0xD1, 0x26, 0x9E };

	printf("security_establish_keys:\n");

	server_random = settings->server_random.data;

	freerdp_hexdump(client_random, 32);
	freerdp_hexdump(server_random, 32);

	memcpy(pre_master_secret, client_random, 24);
	memcpy(pre_master_secret + 24, server_random, 24);

	security_A(pre_master_secret, client_random, server_random, master_secret);
	security_X(master_secret, client_random, server_random, session_key_blob);

	memcpy(settings->sign_key, session_key_blob, 16);

	security_md5_16_32_32(&session_key_blob[16], client_random, server_random, settings->decrypt_key);
	security_md5_16_32_32(&session_key_blob[32], client_random, server_random, settings->encrypt_key);

	if (settings->encryption_method == 1) /* 40 and 56 bit */
	{
		memcpy(settings->sign_key, salt40, 3); /* TODO 56 bit */
		memcpy(settings->decrypt_key, salt40, 3); /* TODO 56 bit */
		memcpy(settings->encrypt_key, salt40, 3); /* TODO 56 bit */
		settings->rc4_key_len = 8;
	}
	else /* 128 bit */
	{
		settings->rc4_key_len = 16;
	}

	memcpy(settings->decrypt_update_key, settings->decrypt_key, 16);
	memcpy(settings->encrypt_update_key, settings->encrypt_key, 16);

	return True;
}

boolean security_key_update(uint8* key, uint8* update_key, int key_len)
{
	uint8 sha1h[20];
	CryptoMd5 md5;
	CryptoSha1 sha1;
	CryptoRc4 rc4;
	uint8 salt40[] = { 0xD1, 0x26, 0x9E };

	sha1 = crypto_sha1_init();
	crypto_sha1_update(sha1, update_key, key_len);
	crypto_sha1_update(sha1, pad1, sizeof(pad1));
	crypto_sha1_update(sha1, key, key_len);
	crypto_sha1_final(sha1, sha1h);

	md5 = crypto_md5_init();
	crypto_md5_update(md5, update_key, key_len);
	crypto_md5_update(md5, pad2, sizeof(pad2));
	crypto_md5_update(md5, sha1h, 20);
	crypto_md5_final(md5, key);

	rc4 = crypto_rc4_init(key, key_len);
	crypto_rc4(rc4, key_len, key, key);
	crypto_rc4_free(rc4);

	if (key_len == 8)
		memcpy(key, salt40, 3); /* TODO 56 bit */

	return True;
}

boolean security_encrypt(uint8* data, int length, rdpRdp* rdp)
{
	if (rdp->encrypt_use_count >= 4096)
	{
		security_key_update(rdp->settings->encrypt_key, rdp->settings->encrypt_update_key, rdp->settings->rc4_key_len);
		crypto_rc4_free(rdp->rc4_encrypt_key);
		rdp->rc4_encrypt_key = crypto_rc4_init(rdp->settings->encrypt_key, rdp->settings->rc4_key_len);
		rdp->encrypt_use_count = 0;
	}
	crypto_rc4(rdp->rc4_encrypt_key, length, data, data);
	rdp->encrypt_use_count += 1;
	return True;
}

boolean security_decrypt(uint8* data, int length, rdpRdp* rdp)
{
	if (rdp->decrypt_use_count >= 4096)
	{
		security_key_update(rdp->settings->decrypt_key, rdp->settings->decrypt_update_key, rdp->settings->rc4_key_len);
		crypto_rc4_free(rdp->rc4_decrypt_key);
		rdp->rc4_decrypt_key = crypto_rc4_init(rdp->settings->decrypt_key, rdp->settings->rc4_key_len);
		rdp->decrypt_use_count = 0;
	}
	crypto_rc4(rdp->rc4_decrypt_key, length, data, data);
	rdp->decrypt_use_count += 1;
	return True;
}

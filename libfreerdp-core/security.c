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
static const uint8 pad1[40] =
{
	"\x36\x36\x36\x36\x36\x36\x36\x36"
	"\x36\x36\x36\x36\x36\x36\x36\x36"
	"\x36\x36\x36\x36\x36\x36\x36\x36"
	"\x36\x36\x36\x36\x36\x36\x36\x36"
	"\x36\x36\x36\x36\x36\x36\x36\x36"
};

/* 0x5C repeated 48 times */
static const uint8 pad2[48] =
{
	"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C"
	"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C"
	"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C"
	"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C"
	"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C"
	"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C"
};

static const uint8
fips_reverse_table[256] =
{
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

static const uint8
fips_oddparity_table[256] =
{
	0x01, 0x01, 0x02, 0x02, 0x04, 0x04, 0x07, 0x07,
	0x08, 0x08, 0x0b, 0x0b, 0x0d, 0x0d, 0x0e, 0x0e,
	0x10, 0x10, 0x13, 0x13, 0x15, 0x15, 0x16, 0x16,
	0x19, 0x19, 0x1a, 0x1a, 0x1c, 0x1c, 0x1f, 0x1f,
	0x20, 0x20, 0x23, 0x23, 0x25, 0x25, 0x26, 0x26,
	0x29, 0x29, 0x2a, 0x2a, 0x2c, 0x2c, 0x2f, 0x2f,
	0x31, 0x31, 0x32, 0x32, 0x34, 0x34, 0x37, 0x37,
	0x38, 0x38, 0x3b, 0x3b, 0x3d, 0x3d, 0x3e, 0x3e,
	0x40, 0x40, 0x43, 0x43, 0x45, 0x45, 0x46, 0x46,
	0x49, 0x49, 0x4a, 0x4a, 0x4c, 0x4c, 0x4f, 0x4f,
	0x51, 0x51, 0x52, 0x52, 0x54, 0x54, 0x57, 0x57,
	0x58, 0x58, 0x5b, 0x5b, 0x5d, 0x5d, 0x5e, 0x5e,
	0x61, 0x61, 0x62, 0x62, 0x64, 0x64, 0x67, 0x67,
	0x68, 0x68, 0x6b, 0x6b, 0x6d, 0x6d, 0x6e, 0x6e,
	0x70, 0x70, 0x73, 0x73, 0x75, 0x75, 0x76, 0x76,
	0x79, 0x79, 0x7a, 0x7a, 0x7c, 0x7c, 0x7f, 0x7f,
	0x80, 0x80, 0x83, 0x83, 0x85, 0x85, 0x86, 0x86,
	0x89, 0x89, 0x8a, 0x8a, 0x8c, 0x8c, 0x8f, 0x8f,
	0x91, 0x91, 0x92, 0x92, 0x94, 0x94, 0x97, 0x97,
	0x98, 0x98, 0x9b, 0x9b, 0x9d, 0x9d, 0x9e, 0x9e,
	0xa1, 0xa1, 0xa2, 0xa2, 0xa4, 0xa4, 0xa7, 0xa7,
	0xa8, 0xa8, 0xab, 0xab, 0xad, 0xad, 0xae, 0xae,
	0xb0, 0xb0, 0xb3, 0xb3, 0xb5, 0xb5, 0xb6, 0xb6,
	0xb9, 0xb9, 0xba, 0xba, 0xbc, 0xbc, 0xbf, 0xbf,
	0xc1, 0xc1, 0xc2, 0xc2, 0xc4, 0xc4, 0xc7, 0xc7,
	0xc8, 0xc8, 0xcb, 0xcb, 0xcd, 0xcd, 0xce, 0xce,
	0xd0, 0xd0, 0xd3, 0xd3, 0xd5, 0xd5, 0xd6, 0xd6,
	0xd9, 0xd9, 0xda, 0xda, 0xdc, 0xdc, 0xdf, 0xdf,
	0xe0, 0xe0, 0xe3, 0xe3, 0xe5, 0xe5, 0xe6, 0xe6,
	0xe9, 0xe9, 0xea, 0xea, 0xec, 0xec, 0xef, 0xef,
	0xf1, 0xf1, 0xf2, 0xf2, 0xf4, 0xf4, 0xf7, 0xf7,
	0xf8, 0xf8, 0xfb, 0xfb, 0xfd, 0xfd, 0xfe, 0xfe
};

static void security_salted_hash(uint8* salt, uint8* input, int length, uint8* salt1, uint8* salt2, uint8* output)
{
	CryptoMd5 md5;
	CryptoSha1 sha1;
	uint8 sha1_digest[CRYPTO_SHA1_DIGEST_LENGTH];

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
	crypto_md5_update(md5, sha1_digest, sizeof(sha1_digest)); /* SHA1_Digest */
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
	uint8 sha1_digest[CRYPTO_SHA1_DIGEST_LENGTH];

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
	crypto_md5_update(md5, sha1_digest, sizeof(sha1_digest)); /* SHA1_Digest */
	crypto_md5_final(md5, output);
}

void security_mac_signature(rdpRdp *rdp, uint8* data, uint32 length, uint8* output)
{
	CryptoMd5 md5;
	CryptoSha1 sha1;
	uint8 length_le[4];
	uint8 md5_digest[CRYPTO_MD5_DIGEST_LENGTH];
	uint8 sha1_digest[CRYPTO_SHA1_DIGEST_LENGTH];

	security_uint32_le(length_le, length); /* length must be little-endian */

	/* SHA1_Digest = SHA1(MACKeyN + pad1 + length + data) */
	sha1 = crypto_sha1_init();
	crypto_sha1_update(sha1, rdp->sign_key, rdp->rc4_key_len); /* MacKeyN */
	crypto_sha1_update(sha1, pad1, sizeof(pad1)); /* pad1 */
	crypto_sha1_update(sha1, length_le, sizeof(length_le)); /* length */
	crypto_sha1_update(sha1, data, length); /* data */
	crypto_sha1_final(sha1, sha1_digest);

	/* MACSignature = First64Bits(MD5(MACKeyN + pad2 + SHA1_Digest)) */
	md5 = crypto_md5_init();
	crypto_md5_update(md5, rdp->sign_key, rdp->rc4_key_len); /* MacKeyN */
	crypto_md5_update(md5, pad2, sizeof(pad2)); /* pad2 */
	crypto_md5_update(md5, sha1_digest, sizeof(sha1_digest)); /* SHA1_Digest */
	crypto_md5_final(md5, md5_digest);

	memcpy(output, md5_digest, 8);
}

void security_salted_mac_signature(rdpRdp *rdp, uint8* data, uint32 length, boolean encryption, uint8* output)
{
	CryptoMd5 md5;
	CryptoSha1 sha1;
	uint8 length_le[4];
	uint8 use_count_le[4];
	uint8 md5_digest[CRYPTO_MD5_DIGEST_LENGTH];
	uint8 sha1_digest[CRYPTO_SHA1_DIGEST_LENGTH];

	security_uint32_le(length_le, length); /* length must be little-endian */
	if (encryption)
		security_uint32_le(use_count_le, rdp->encrypt_use_count);
	else
	{
		/*
		 * We calculate checksum on plain text, so we must have already
		 * decrypt it, which means decrypt_use_count is off by one.
		 */
		security_uint32_le(use_count_le, rdp->decrypt_use_count - 1);
	}

	/* SHA1_Digest = SHA1(MACKeyN + pad1 + length + data) */
	sha1 = crypto_sha1_init();
	crypto_sha1_update(sha1, rdp->sign_key, rdp->rc4_key_len); /* MacKeyN */
	crypto_sha1_update(sha1, pad1, sizeof(pad1)); /* pad1 */
	crypto_sha1_update(sha1, length_le, sizeof(length_le)); /* length */
	crypto_sha1_update(sha1, data, length); /* data */
	crypto_sha1_update(sha1, use_count_le, sizeof(use_count_le)); /* encryptionCount */
	crypto_sha1_final(sha1, sha1_digest);

	/* MACSignature = First64Bits(MD5(MACKeyN + pad2 + SHA1_Digest)) */
	md5 = crypto_md5_init();
	crypto_md5_update(md5, rdp->sign_key, rdp->rc4_key_len); /* MacKeyN */
	crypto_md5_update(md5, pad2, sizeof(pad2)); /* pad2 */
	crypto_md5_update(md5, sha1_digest, sizeof(sha1_digest)); /* SHA1_Digest */
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

static void fips_expand_key_bits(uint8* in, uint8* out)
{
	uint8 buf[21], c;
	int i, b, p, r;

	/* reverse every byte in the key */
	for (i = 0; i < 21; i++)
		buf[i] = fips_reverse_table[in[i]];

	/* insert a zero-bit after every 7th bit */
	for (i = 0, b = 0; i < 24; i++, b += 7)
	{
		p = b / 8;
		r = b % 8;
		if (r == 0)
		{
			out[i] = buf[p] & 0xfe;
		}
		else
		{
			/* c is accumulator */
			c = buf[p] << r;
			c |= buf[p + 1] >> (8 - r);
			out[i] = c & 0xfe;
		}
	}

	/* reverse every byte */
	/* alter lsb so the byte has odd parity */
	for (i = 0; i < 24; i++)
		out[i] = fips_oddparity_table[fips_reverse_table[out[i]]];
}

boolean security_establish_keys(uint8* client_random, rdpRdp* rdp)
{
	uint8 pre_master_secret[48];
	uint8 master_secret[48];
	uint8 session_key_blob[48];
	uint8* server_random;
	uint8 salt40[] = { 0xD1, 0x26, 0x9E };
	rdpSettings* settings;

	settings = rdp->settings;
	server_random = settings->server_random->data;

	if (settings->encryption_method == ENCRYPTION_METHOD_FIPS)
	{
		CryptoSha1 sha1;
		uint8 client_encrypt_key_t[CRYPTO_SHA1_DIGEST_LENGTH + 1];
		uint8 client_decrypt_key_t[CRYPTO_SHA1_DIGEST_LENGTH + 1];

		printf("FIPS Compliant encryption level.\n");

		sha1 = crypto_sha1_init();
		crypto_sha1_update(sha1, client_random + 16, 16);
		crypto_sha1_update(sha1, server_random + 16, 16);
		crypto_sha1_final(sha1, client_encrypt_key_t);

		client_encrypt_key_t[20] = client_encrypt_key_t[0];
		fips_expand_key_bits(client_encrypt_key_t, rdp->fips_encrypt_key);

		sha1 = crypto_sha1_init();
		crypto_sha1_update(sha1, client_random, 16);
		crypto_sha1_update(sha1, server_random, 16);
		crypto_sha1_final(sha1, client_decrypt_key_t);

		client_decrypt_key_t[20] = client_decrypt_key_t[0];
		fips_expand_key_bits(client_decrypt_key_t, rdp->fips_decrypt_key);

		sha1 = crypto_sha1_init();
		crypto_sha1_update(sha1, client_decrypt_key_t, 20);
		crypto_sha1_update(sha1, client_encrypt_key_t, 20);
		crypto_sha1_final(sha1, rdp->fips_sign_key);
	}

	memcpy(pre_master_secret, client_random, 24);
	memcpy(pre_master_secret + 24, server_random, 24);

	security_A(pre_master_secret, client_random, server_random, master_secret);
	security_X(master_secret, client_random, server_random, session_key_blob);

	memcpy(rdp->sign_key, session_key_blob, 16);

	if (rdp->settings->server_mode) {
		security_md5_16_32_32(&session_key_blob[16], client_random,
		    server_random, rdp->encrypt_key);
		security_md5_16_32_32(&session_key_blob[32], client_random,
		    server_random, rdp->decrypt_key);
	} else {
		security_md5_16_32_32(&session_key_blob[16], client_random,
		    server_random, rdp->decrypt_key);
		security_md5_16_32_32(&session_key_blob[32], client_random,
		    server_random, rdp->encrypt_key);
	}

	if (settings->encryption_method == 1) /* 40 and 56 bit */
	{
		memcpy(rdp->sign_key, salt40, 3); /* TODO 56 bit */
		memcpy(rdp->decrypt_key, salt40, 3); /* TODO 56 bit */
		memcpy(rdp->encrypt_key, salt40, 3); /* TODO 56 bit */
		rdp->rc4_key_len = 8;
	}
	else if (settings->encryption_method == 2) /* 128 bit */
	{
		rdp->rc4_key_len = 16;
	}

	memcpy(rdp->decrypt_update_key, rdp->decrypt_key, 16);
	memcpy(rdp->encrypt_update_key, rdp->encrypt_key, 16);

	return true;
}

boolean security_key_update(uint8* key, uint8* update_key, int key_len)
{
	uint8 sha1h[CRYPTO_SHA1_DIGEST_LENGTH];
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
	crypto_md5_update(md5, sha1h, sizeof(sha1h));
	crypto_md5_final(md5, key);

	rc4 = crypto_rc4_init(key, key_len);
	crypto_rc4(rc4, key_len, key, key);
	crypto_rc4_free(rc4);

	if (key_len == 8)
		memcpy(key, salt40, 3); /* TODO 56 bit */

	return true;
}

boolean security_encrypt(uint8* data, int length, rdpRdp* rdp)
{
	if (rdp->encrypt_use_count >= 4096)
	{
		security_key_update(rdp->encrypt_key, rdp->encrypt_update_key, rdp->rc4_key_len);
		crypto_rc4_free(rdp->rc4_encrypt_key);
		rdp->rc4_encrypt_key = crypto_rc4_init(rdp->encrypt_key, rdp->rc4_key_len);
		rdp->encrypt_use_count = 0;
	}
	crypto_rc4(rdp->rc4_encrypt_key, length, data, data);
	rdp->encrypt_use_count += 1;
	return true;
}

boolean security_decrypt(uint8* data, int length, rdpRdp* rdp)
{
	if (rdp->decrypt_use_count >= 4096)
	{
		security_key_update(rdp->decrypt_key, rdp->decrypt_update_key, rdp->rc4_key_len);
		crypto_rc4_free(rdp->rc4_decrypt_key);
		rdp->rc4_decrypt_key = crypto_rc4_init(rdp->decrypt_key, rdp->rc4_key_len);
		rdp->decrypt_use_count = 0;
	}
	crypto_rc4(rdp->rc4_decrypt_key, length, data, data);
	rdp->decrypt_use_count += 1;
	return true;
}

void security_hmac_signature(uint8* data, int length, uint8* output, rdpRdp* rdp)
{
	uint8 buf[20];
	uint8 use_count_le[4];

	security_uint32_le(use_count_le, rdp->encrypt_use_count);

	crypto_hmac_sha1_init(rdp->fips_hmac, rdp->fips_sign_key, 20);
	crypto_hmac_update(rdp->fips_hmac, data, length);
	crypto_hmac_update(rdp->fips_hmac, use_count_le, 4);
	crypto_hmac_final(rdp->fips_hmac, buf, 20);

	memmove(output, buf, 8);
}

boolean security_fips_encrypt(uint8* data, int length, rdpRdp* rdp)
{
	crypto_des3_encrypt(rdp->fips_encrypt, length, data, data);
	rdp->encrypt_use_count++;
	return true;
}

boolean security_fips_decrypt(uint8* data, int length, rdpRdp* rdp)
{
	crypto_des3_decrypt(rdp->fips_decrypt, length, data, data);
	return true;
}

boolean security_fips_check_signature(uint8* data, int length, uint8* sig, rdpRdp* rdp)
{
	uint8 buf[20];
	uint8 use_count_le[4];

	security_uint32_le(use_count_le, rdp->decrypt_use_count);

	crypto_hmac_sha1_init(rdp->fips_hmac, rdp->fips_sign_key, 20);
	crypto_hmac_update(rdp->fips_hmac, data, length);
	crypto_hmac_update(rdp->fips_hmac, use_count_le, 4);
	crypto_hmac_final(rdp->fips_hmac, buf, 20);

	rdp->decrypt_use_count++;

	if (memcmp(sig, buf, 8))
		return false;

	return true;
}

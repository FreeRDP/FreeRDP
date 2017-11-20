/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Security
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Norbert Federa <norbert.federa@thincast.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "security.h"

#include <freerdp/log.h>
#include <winpr/crypto.h>

#define TAG FREERDP_TAG("core")

/* 0x36 repeated 40 times */
static const BYTE pad1[40] =
{
	"\x36\x36\x36\x36\x36\x36\x36\x36"
	"\x36\x36\x36\x36\x36\x36\x36\x36"
	"\x36\x36\x36\x36\x36\x36\x36\x36"
	"\x36\x36\x36\x36\x36\x36\x36\x36"
	"\x36\x36\x36\x36\x36\x36\x36\x36"
};

/* 0x5C repeated 48 times */
static const BYTE pad2[48] =
{
	"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C"
	"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C"
	"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C"
	"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C"
	"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C"
	"\x5C\x5C\x5C\x5C\x5C\x5C\x5C\x5C"
};

static const BYTE
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

static const BYTE
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

static BOOL security_salted_hash(const BYTE* salt, const BYTE* input, int length,
		const BYTE* salt1, const BYTE* salt2, BYTE* output)
{
	WINPR_DIGEST_CTX* sha1 = NULL;
	WINPR_DIGEST_CTX* md5 = NULL;
	BYTE sha1_digest[WINPR_SHA1_DIGEST_LENGTH];
	BOOL result = FALSE;

	/* SaltedHash(Salt, Input, Salt1, Salt2) = MD5(S + SHA1(Input + Salt + Salt1 + Salt2)) */

	/* SHA1_Digest = SHA1(Input + Salt + Salt1 + Salt2) */
	if (!(sha1 = winpr_Digest_New()))
		goto out;

	if (!winpr_Digest_Init(sha1, WINPR_MD_SHA1))
		goto out;

	if (!winpr_Digest_Update(sha1, input, length)) /* Input */
		goto out;

	if (!winpr_Digest_Update(sha1, salt, 48)) /* Salt (48 bytes) */
		goto out;

	if (!winpr_Digest_Update(sha1, salt1, 32)) /* Salt1 (32 bytes) */
		goto out;

	if (!winpr_Digest_Update(sha1, salt2, 32)) /* Salt2 (32 bytes) */
		goto out;

	if (!winpr_Digest_Final(sha1, sha1_digest, sizeof(sha1_digest)))
		goto out;

	/* SaltedHash(Salt, Input, Salt1, Salt2) = MD5(S + SHA1_Digest) */
	if (!(md5 = winpr_Digest_New()))
		goto out;

	/* Allow FIPS override for use of MD5 here, this is used for creating hashes of the premaster_secret and master_secret */
	/* used for RDP licensing as described in MS-RDPELE. This is for RDP licensing packets */
	/* which will already be encrypted under FIPS, so the use of MD5 here is not for sensitive data protection. */
	if (!winpr_Digest_Init_Allow_FIPS(md5, WINPR_MD_MD5))
		goto out;

	if (!winpr_Digest_Update(md5, salt, 48)) /* Salt (48 bytes) */
		goto out;

	if (!winpr_Digest_Update(md5, sha1_digest, sizeof(sha1_digest))) /* SHA1_Digest */
		goto out;

	if (!winpr_Digest_Final(md5, output, WINPR_MD5_DIGEST_LENGTH))
		goto out;

	result = TRUE;
out:
	winpr_Digest_Free(sha1);
	winpr_Digest_Free(md5);
	return result;
}

static BOOL security_premaster_hash(const char* input, int length, const BYTE* premaster_secret,
		const BYTE* client_random, const BYTE* server_random, BYTE* output)
{
	/* PremasterHash(Input) = SaltedHash(PremasterSecret, Input, ClientRandom, ServerRandom) */
	return security_salted_hash(premaster_secret, (BYTE*)input, length, client_random, server_random, output);
}

BOOL security_master_secret(const BYTE* premaster_secret, const BYTE* client_random,
		const BYTE* server_random, BYTE* output)
{
	/* MasterSecret = PremasterHash('A') + PremasterHash('BB') + PremasterHash('CCC') */
	return security_premaster_hash("A", 1, premaster_secret, client_random, server_random, &output[0]) &&
		security_premaster_hash("BB", 2, premaster_secret, client_random, server_random, &output[16]) &&
		security_premaster_hash("CCC", 3, premaster_secret, client_random, server_random, &output[32]);
}

static BOOL security_master_hash(const char* input, int length, const BYTE* master_secret,
		const BYTE* client_random, const BYTE* server_random, BYTE* output)
{
	/* MasterHash(Input) = SaltedHash(MasterSecret, Input, ServerRandom, ClientRandom) */
	return security_salted_hash(master_secret, (const BYTE*)input, length, server_random, client_random, output);
}

BOOL security_session_key_blob(const BYTE* master_secret, const BYTE* client_random,
		const BYTE* server_random, BYTE* output)
{
	/* MasterHash = MasterHash('A') + MasterHash('BB') + MasterHash('CCC') */
	return security_master_hash("A", 1, master_secret, client_random, server_random, &output[0]) &&
		security_master_hash("BB", 2, master_secret, client_random, server_random, &output[16]) &&
		security_master_hash("CCC", 3, master_secret, client_random, server_random, &output[32]);
}

void security_mac_salt_key(const BYTE* session_key_blob, const BYTE* client_random,
		const BYTE* server_random, BYTE* output)
{
	/* MacSaltKey = First128Bits(SessionKeyBlob) */
	memcpy(output, session_key_blob, 16);
}

BOOL security_md5_16_32_32(const BYTE* in0, const BYTE* in1, const BYTE* in2, BYTE* output)
{
	WINPR_DIGEST_CTX* md5 = NULL;
	BOOL result = FALSE;

	if (!(md5 = winpr_Digest_New()))
		return FALSE;

	if (!winpr_Digest_Init(md5, WINPR_MD_MD5))
		goto out;

	if (!winpr_Digest_Update(md5, in0, 16))
		goto out;

	if (!winpr_Digest_Update(md5, in1, 32))
		goto out;

	if (!winpr_Digest_Update(md5, in2, 32))
		goto out;

	if (!winpr_Digest_Final(md5, output, WINPR_MD5_DIGEST_LENGTH))
		goto out;

	result = TRUE;
out:
	winpr_Digest_Free(md5);
	return result;
}

BOOL security_md5_16_32_32_Allow_FIPS(const BYTE* in0, const BYTE* in1, const BYTE* in2, BYTE* output)
{
	WINPR_DIGEST_CTX* md5 = NULL;
	BOOL result = FALSE;

	if (!(md5 = winpr_Digest_New()))
		return FALSE;
	if (!winpr_Digest_Init_Allow_FIPS(md5, WINPR_MD_MD5))
		goto out;
	if (!winpr_Digest_Update(md5, in0, 16))
		goto out;
	if (!winpr_Digest_Update(md5, in1, 32))
		goto out;
	if (!winpr_Digest_Update(md5, in2, 32))
		goto out;
	if (!winpr_Digest_Final(md5, output, WINPR_MD5_DIGEST_LENGTH))
		goto out;

	result = TRUE;
out:
	winpr_Digest_Free(md5);
	return result;
}

BOOL security_licensing_encryption_key(const BYTE* session_key_blob, const BYTE* client_random,
                                       const BYTE* server_random, BYTE* output)
{
	/* LicensingEncryptionKey = MD5(Second128Bits(SessionKeyBlob) + ClientRandom + ServerRandom))
	 * Allow FIPS use of MD5 here, this is just used for creating the licensing encryption key as described in MS-RDPELE.
	 * This is for RDP licensing packets which will already be encrypted under FIPS, so the use of MD5 here is not for
	 * sensitive data protection. */
	return security_md5_16_32_32_Allow_FIPS(&session_key_blob[16], client_random, server_random,
	                                        output);
}

void security_UINT32_le(BYTE* output, UINT32 value)
{
	output[0] = (value) & 0xFF;
	output[1] = (value >> 8) & 0xFF;
	output[2] = (value >> 16) & 0xFF;
	output[3] = (value >> 24) & 0xFF;
}

BOOL security_mac_data(const BYTE* mac_salt_key, const BYTE* data, UINT32 length,
		BYTE* output)
{
	WINPR_DIGEST_CTX* sha1 = NULL;
	WINPR_DIGEST_CTX* md5 = NULL;
	BYTE length_le[4];
	BYTE sha1_digest[WINPR_SHA1_DIGEST_LENGTH];
	BOOL result = FALSE;
	/* MacData = MD5(MacSaltKey + pad2 + SHA1(MacSaltKey + pad1 + length + data)) */
	security_UINT32_le(length_le, length); /* length must be little-endian */

	/* SHA1_Digest = SHA1(MacSaltKey + pad1 + length + data) */
	if (!(sha1 = winpr_Digest_New()))
		goto out;

	if (!winpr_Digest_Init(sha1, WINPR_MD_SHA1))
		goto out;

	if (!winpr_Digest_Update(sha1, mac_salt_key, 16)) /* MacSaltKey */
		goto out;

	if (!winpr_Digest_Update(sha1, pad1, sizeof(pad1))) /* pad1 */
		goto out;

	if (!winpr_Digest_Update(sha1, length_le, sizeof(length_le))) /* length */
		goto out;

	if (!winpr_Digest_Update(sha1, data, length)) /* data */
		goto out;

	if (!winpr_Digest_Final(sha1, sha1_digest, sizeof(sha1_digest)))
		goto out;

	/* MacData = MD5(MacSaltKey + pad2 + SHA1_Digest) */
	if (!(md5 = winpr_Digest_New()))
		goto out;

	/* Allow FIPS override for use of MD5 here, this is only used for creating the MACData field of the */
	/* Client Platform Challenge Response packet (from MS-RDPELE section 2.2.2.5). This is for RDP licensing packets */
	/* which will already be encrypted under FIPS, so the use of MD5 here is not for sensitive data protection. */
	if (!winpr_Digest_Init_Allow_FIPS(md5, WINPR_MD_MD5))
		goto out;

	if (!winpr_Digest_Update(md5, mac_salt_key, 16)) /* MacSaltKey */
		goto out;

	if (!winpr_Digest_Update(md5, pad2, sizeof(pad2))) /* pad2 */
		goto out;

	if (!winpr_Digest_Update(md5, sha1_digest, sizeof(sha1_digest))) /* SHA1_Digest */
		goto out;

	if (!winpr_Digest_Final(md5, output, WINPR_MD5_DIGEST_LENGTH))
		goto out;

	result = TRUE;
out:
	winpr_Digest_Free(sha1);
	winpr_Digest_Free(md5);
	return result;
}

BOOL security_mac_signature(rdpRdp *rdp, const BYTE* data, UINT32 length, BYTE* output)
{
	WINPR_DIGEST_CTX* sha1 = NULL;
	WINPR_DIGEST_CTX* md5 = NULL;
	BYTE length_le[4];
	BYTE md5_digest[WINPR_MD5_DIGEST_LENGTH];
	BYTE sha1_digest[WINPR_SHA1_DIGEST_LENGTH];
	BOOL result = FALSE;
	security_UINT32_le(length_le, length); /* length must be little-endian */

	/* SHA1_Digest = SHA1(MACKeyN + pad1 + length + data) */
	if (!(sha1 = winpr_Digest_New()))
		goto out;

	if (!winpr_Digest_Init(sha1, WINPR_MD_SHA1))
		goto out;

	if (!winpr_Digest_Update(sha1, rdp->sign_key, rdp->rc4_key_len)) /* MacKeyN */
		goto out;

	if (!winpr_Digest_Update(sha1, pad1, sizeof(pad1))) /* pad1 */
		goto out;

	if (!winpr_Digest_Update(sha1, length_le, sizeof(length_le))) /* length */
		goto out;

	if (!winpr_Digest_Update(sha1, data, length)) /* data */
		goto out;

	if (!winpr_Digest_Final(sha1, sha1_digest, sizeof(sha1_digest)))
		goto out;

	/* MACSignature = First64Bits(MD5(MACKeyN + pad2 + SHA1_Digest)) */
	if (!(md5 = winpr_Digest_New()))
		goto out;

	if (!winpr_Digest_Init(md5, WINPR_MD_MD5))
		goto out;

	if (!winpr_Digest_Update(md5, rdp->sign_key, rdp->rc4_key_len)) /* MacKeyN */
		goto out;

	if (!winpr_Digest_Update(md5, pad2, sizeof(pad2))) /* pad2 */
		goto out;

	if (!winpr_Digest_Update(md5, sha1_digest, sizeof(sha1_digest))) /* SHA1_Digest */
		goto out;

	if (!winpr_Digest_Final(md5, md5_digest, sizeof(md5_digest)))
		goto out;

	memcpy(output, md5_digest, 8);
	result = TRUE;
out:
	winpr_Digest_Free(sha1);
	winpr_Digest_Free(md5);
	return result;
}

BOOL security_salted_mac_signature(rdpRdp *rdp, const BYTE* data, UINT32 length,
		BOOL encryption, BYTE* output)
{
	WINPR_DIGEST_CTX* sha1 = NULL;
	WINPR_DIGEST_CTX* md5 = NULL;
	BYTE length_le[4];
	BYTE use_count_le[4];
	BYTE md5_digest[WINPR_MD5_DIGEST_LENGTH];
	BYTE sha1_digest[WINPR_SHA1_DIGEST_LENGTH];
	BOOL result = FALSE;
	security_UINT32_le(length_le, length); /* length must be little-endian */

	if (encryption)
	{
		security_UINT32_le(use_count_le, rdp->encrypt_checksum_use_count);
	}
	else
	{
		/*
		 * We calculate checksum on plain text, so we must have already
		 * decrypt it, which means decrypt_checksum_use_count is off by one.
		 */
		security_UINT32_le(use_count_le, rdp->decrypt_checksum_use_count - 1);
	}

	/* SHA1_Digest = SHA1(MACKeyN + pad1 + length + data) */
	if (!(sha1 = winpr_Digest_New()))
		goto out;

	if (!winpr_Digest_Init(sha1, WINPR_MD_SHA1))
		goto out;

	if (!winpr_Digest_Update(sha1, rdp->sign_key, rdp->rc4_key_len)) /* MacKeyN */
		goto out;

	if (!winpr_Digest_Update(sha1, pad1, sizeof(pad1))) /* pad1 */
		goto out;

	if (!winpr_Digest_Update(sha1, length_le, sizeof(length_le))) /* length */
		goto out;

	if (!winpr_Digest_Update(sha1, data, length)) /* data */
		goto out;

	if (!winpr_Digest_Update(sha1, use_count_le, sizeof(use_count_le))) /* encryptionCount */
		goto out;

	if (!winpr_Digest_Final(sha1, sha1_digest, sizeof(sha1_digest)))
		goto out;

	/* MACSignature = First64Bits(MD5(MACKeyN + pad2 + SHA1_Digest)) */
	if (!(md5 = winpr_Digest_New()))
		goto out;

	if (!winpr_Digest_Init(md5, WINPR_MD_MD5))
		goto out;

	if (!winpr_Digest_Update(md5, rdp->sign_key, rdp->rc4_key_len)) /* MacKeyN */
		goto out;

	if (!winpr_Digest_Update(md5, pad2, sizeof(pad2))) /* pad2 */
		goto out;

	if (!winpr_Digest_Update(md5, sha1_digest, sizeof(sha1_digest))) /* SHA1_Digest */
		goto out;

	if (!winpr_Digest_Final(md5, md5_digest, sizeof(md5_digest)))
		goto out;

	memcpy(output, md5_digest, 8);
	result = TRUE;
out:
	winpr_Digest_Free(sha1);
	winpr_Digest_Free(md5);
	return result;
}

static BOOL security_A(BYTE* master_secret, const BYTE* client_random, BYTE* server_random,
		BYTE* output)
{
	return
		security_premaster_hash("A", 1, master_secret, client_random, server_random, &output[0]) &&
		security_premaster_hash("BB", 2, master_secret, client_random, server_random, &output[16]) &&
		security_premaster_hash("CCC", 3, master_secret, client_random, server_random, &output[32]);
}

static BOOL security_X(BYTE* master_secret, const BYTE* client_random, BYTE* server_random,
		BYTE* output)
{
	return
		security_premaster_hash("X", 1, master_secret, client_random, server_random, &output[0]) &&
		security_premaster_hash("YY", 2, master_secret, client_random, server_random, &output[16]) &&
		security_premaster_hash("ZZZ", 3, master_secret, client_random, server_random, &output[32]);
}

static void fips_expand_key_bits(BYTE* in, BYTE* out)
{
	BYTE buf[21], c;
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

BOOL security_establish_keys(const BYTE* client_random, rdpRdp* rdp)
{
	BYTE pre_master_secret[48];
	BYTE master_secret[48];
	BYTE session_key_blob[48];
	BYTE* server_random;
	BYTE salt[] = { 0xD1, 0x26, 0x9E }; /* 40 bits: 3 bytes, 56 bits: 1 byte */
	rdpSettings* settings;
	BOOL status;
	settings = rdp->settings;
	server_random = settings->ServerRandom;

	if (settings->EncryptionMethods == ENCRYPTION_METHOD_FIPS)
	{
		WINPR_DIGEST_CTX* sha1;
		BYTE client_encrypt_key_t[WINPR_SHA1_DIGEST_LENGTH + 1];
		BYTE client_decrypt_key_t[WINPR_SHA1_DIGEST_LENGTH + 1];

		if (!(sha1 = winpr_Digest_New()))
			return FALSE;

		if (!winpr_Digest_Init(sha1, WINPR_MD_SHA1) ||
		    !winpr_Digest_Update(sha1, client_random + 16, 16) ||
		    !winpr_Digest_Update(sha1, server_random + 16, 16) ||
		    !winpr_Digest_Final(sha1, client_encrypt_key_t, sizeof(client_encrypt_key_t)))
		{
			winpr_Digest_Free(sha1);
			return FALSE;
		}

		client_encrypt_key_t[20] = client_encrypt_key_t[0];

		if (!winpr_Digest_Init(sha1, WINPR_MD_SHA1) ||
		    !winpr_Digest_Update(sha1, client_random, 16) ||
		    !winpr_Digest_Update(sha1, server_random, 16) ||
		    !winpr_Digest_Final(sha1, client_decrypt_key_t, sizeof(client_decrypt_key_t)))
		{
			winpr_Digest_Free(sha1);
			return FALSE;
		}

		client_decrypt_key_t[20] = client_decrypt_key_t[0];

		if (!winpr_Digest_Init(sha1, WINPR_MD_SHA1) ||
		    !winpr_Digest_Update(sha1, client_decrypt_key_t, WINPR_SHA1_DIGEST_LENGTH) ||
		    !winpr_Digest_Update(sha1, client_encrypt_key_t, WINPR_SHA1_DIGEST_LENGTH) ||
		    !winpr_Digest_Final(sha1, rdp->fips_sign_key, WINPR_SHA1_DIGEST_LENGTH))
		{
			winpr_Digest_Free(sha1);
			return FALSE;
		}

		winpr_Digest_Free(sha1);

		if (rdp->settings->ServerMode)
		{
			fips_expand_key_bits(client_encrypt_key_t, rdp->fips_decrypt_key);
			fips_expand_key_bits(client_decrypt_key_t, rdp->fips_encrypt_key);
		}
		else
		{
			fips_expand_key_bits(client_encrypt_key_t, rdp->fips_encrypt_key);
			fips_expand_key_bits(client_decrypt_key_t, rdp->fips_decrypt_key);
		}
	}

	memcpy(pre_master_secret, client_random, 24);
	memcpy(pre_master_secret + 24, server_random, 24);

	if (!security_A(pre_master_secret, client_random, server_random, master_secret) ||
		!security_X(master_secret, client_random, server_random, session_key_blob))
	{
		return FALSE;
	}

	memcpy(rdp->sign_key, session_key_blob, 16);

	if (rdp->settings->ServerMode)
	{
		status = security_md5_16_32_32(&session_key_blob[16], client_random, server_random, rdp->encrypt_key);
		status &= security_md5_16_32_32(&session_key_blob[32], client_random, server_random, rdp->decrypt_key);
	}
	else
	{
		/* Allow FIPS use of MD5 here, this is just used for generation of the SessionKeyBlob as described in MS-RDPELE. */
		/* This is for RDP licensing packets which will already be encrypted under FIPS, so the use of MD5 here is not */
		/* for sensitive data protection. */
		status = security_md5_16_32_32_Allow_FIPS(&session_key_blob[16], client_random, server_random, rdp->decrypt_key);
		status &= security_md5_16_32_32_Allow_FIPS(&session_key_blob[32], client_random, server_random, rdp->encrypt_key);
	}

	if (!status)
		return FALSE;

	if (settings->EncryptionMethods == ENCRYPTION_METHOD_40BIT)
	{
		memcpy(rdp->sign_key, salt, 3);
		memcpy(rdp->decrypt_key, salt, 3);
		memcpy(rdp->encrypt_key, salt, 3);
		rdp->rc4_key_len = 8;
	}
	else if (settings->EncryptionMethods == ENCRYPTION_METHOD_56BIT)
	{
		memcpy(rdp->sign_key, salt, 1);
		memcpy(rdp->decrypt_key, salt, 1);
		memcpy(rdp->encrypt_key, salt, 1);
		rdp->rc4_key_len = 8;
	}
	else if (settings->EncryptionMethods == ENCRYPTION_METHOD_128BIT)
	{
		rdp->rc4_key_len = 16;
	}

	memcpy(rdp->decrypt_update_key, rdp->decrypt_key, 16);
	memcpy(rdp->encrypt_update_key, rdp->encrypt_key, 16);
	rdp->decrypt_use_count = 0;
	rdp->decrypt_checksum_use_count = 0;
	rdp->encrypt_use_count =0;
	rdp->encrypt_checksum_use_count =0;
	return TRUE;
}

BOOL security_key_update(BYTE* key, BYTE* update_key, int key_len, rdpRdp* rdp)
{
	BYTE sha1h[WINPR_SHA1_DIGEST_LENGTH];
	WINPR_DIGEST_CTX* sha1 = NULL;
	WINPR_DIGEST_CTX* md5 = NULL;
	WINPR_RC4_CTX* rc4 = NULL;
	BYTE salt[] = { 0xD1, 0x26, 0x9E }; /* 40 bits: 3 bytes, 56 bits: 1 byte */
	BOOL result = FALSE;
	WLog_DBG(TAG, "updating RDP key");

	if (!(sha1 = winpr_Digest_New()))
		goto out;

	if (!winpr_Digest_Init(sha1, WINPR_MD_SHA1))
		goto out;

	if (!winpr_Digest_Update(sha1, update_key, key_len))
		goto out;

	if (!winpr_Digest_Update(sha1, pad1, sizeof(pad1)))
		goto out;

	if (!winpr_Digest_Update(sha1, key, key_len))
		goto out;

	if (!winpr_Digest_Final(sha1, sha1h, sizeof(sha1h)))
		goto out;

	if (!(md5 = winpr_Digest_New()))
		goto out;

	if (!winpr_Digest_Init(md5, WINPR_MD_MD5))
		goto out;

	if (!winpr_Digest_Update(md5, update_key, key_len))
		goto out;

	if (!winpr_Digest_Update(md5, pad2, sizeof(pad2)))
		goto out;

	if (!winpr_Digest_Update(md5, sha1h, sizeof(sha1h)))
		goto out;

	if (!winpr_Digest_Final(md5, key, WINPR_MD5_DIGEST_LENGTH))
		goto out;

	if (!(rc4 = winpr_RC4_New(key, key_len)))
		goto out;

	if (!winpr_RC4_Update(rc4, key_len, key, key))
		goto out;

	if (rdp->settings->EncryptionMethods == ENCRYPTION_METHOD_40BIT)
		memcpy(key, salt, 3);
	else if (rdp->settings->EncryptionMethods == ENCRYPTION_METHOD_56BIT)
		memcpy(key, salt, 1);

	result = TRUE;
out:
	winpr_Digest_Free(sha1);
	winpr_Digest_Free(md5);
	winpr_RC4_Free(rc4);
	return result;
}

BOOL security_encrypt(BYTE* data, size_t length, rdpRdp* rdp)
{
	if (rdp->encrypt_use_count >= 4096)
	{
		if (!security_key_update(rdp->encrypt_key, rdp->encrypt_update_key, rdp->rc4_key_len, rdp))
			return FALSE;

		winpr_RC4_Free(rdp->rc4_encrypt_key);
		rdp->rc4_encrypt_key = winpr_RC4_New(rdp->encrypt_key, rdp->rc4_key_len);

		if (!rdp->rc4_encrypt_key)
			return FALSE;

		rdp->encrypt_use_count = 0;
	}

	if (!winpr_RC4_Update(rdp->rc4_encrypt_key, length, data, data))
		return FALSE;

	rdp->encrypt_use_count++;
	rdp->encrypt_checksum_use_count++;
	return TRUE;
}

BOOL security_decrypt(BYTE* data, size_t length, rdpRdp* rdp)
{
	if (rdp->rc4_decrypt_key == NULL)
		return FALSE;

	if (rdp->decrypt_use_count >= 4096)
	{
		if (!security_key_update(rdp->decrypt_key, rdp->decrypt_update_key, rdp->rc4_key_len, rdp))
			return FALSE;

		winpr_RC4_Free(rdp->rc4_decrypt_key);
		rdp->rc4_decrypt_key = winpr_RC4_New(rdp->decrypt_key,
						     rdp->rc4_key_len);

		if (!rdp->rc4_decrypt_key)
			return FALSE;

		rdp->decrypt_use_count = 0;
	}

	if (!winpr_RC4_Update(rdp->rc4_decrypt_key, length, data, data))
		return FALSE;

	rdp->decrypt_use_count += 1;
	rdp->decrypt_checksum_use_count++;
	return TRUE;
}

BOOL security_hmac_signature(const BYTE* data, size_t length, BYTE* output, rdpRdp* rdp)
{
	BYTE buf[WINPR_SHA1_DIGEST_LENGTH];
	BYTE use_count_le[4];
	WINPR_HMAC_CTX* hmac;
	BOOL result = FALSE;
	security_UINT32_le(use_count_le, rdp->encrypt_use_count);

	if (!(hmac = winpr_HMAC_New()))
		return FALSE;

	if (!winpr_HMAC_Init(hmac, WINPR_MD_SHA1, rdp->fips_sign_key, WINPR_SHA1_DIGEST_LENGTH))
		goto out;

	if (!winpr_HMAC_Update(hmac, data, length))
		goto out;

	if (!winpr_HMAC_Update(hmac, use_count_le, 4))
		goto out;

	if (!winpr_HMAC_Final(hmac, buf, WINPR_SHA1_DIGEST_LENGTH))
		goto out;

	memmove(output, buf, 8);
	result = TRUE;
out:
	winpr_HMAC_Free(hmac);
	return result;
}

BOOL security_fips_encrypt(BYTE* data, size_t length, rdpRdp* rdp)
{
	size_t olen;

	if (!winpr_Cipher_Update(rdp->fips_encrypt, data, length, data, &olen))
		return FALSE;

	rdp->encrypt_use_count++;
	return TRUE;
}

BOOL security_fips_decrypt(BYTE* data, size_t length, rdpRdp* rdp)
{
	size_t olen;

	if (!winpr_Cipher_Update(rdp->fips_decrypt, data, length, data, &olen))
		return FALSE;

	return TRUE;
}

BOOL security_fips_check_signature(const BYTE* data, size_t length, const BYTE* sig, rdpRdp* rdp)
{
	BYTE buf[WINPR_SHA1_DIGEST_LENGTH];
	BYTE use_count_le[4];
	WINPR_HMAC_CTX* hmac;
	BOOL result = FALSE;
	security_UINT32_le(use_count_le, rdp->decrypt_use_count);

	if (!(hmac = winpr_HMAC_New()))
		return FALSE;

	if (!winpr_HMAC_Init(hmac, WINPR_MD_SHA1, rdp->fips_sign_key, WINPR_SHA1_DIGEST_LENGTH))
		goto out;

	if (!winpr_HMAC_Update(hmac, data, length))
		goto out;

	if (!winpr_HMAC_Update(hmac, use_count_le, 4))
		goto out;

	if (!winpr_HMAC_Final(hmac, buf, WINPR_SHA1_DIGEST_LENGTH))
		goto out;

	rdp->decrypt_use_count++;

	if (!memcmp(sig, buf, 8))
		result = TRUE;

out:
	winpr_HMAC_Free(hmac);
	return result;
}

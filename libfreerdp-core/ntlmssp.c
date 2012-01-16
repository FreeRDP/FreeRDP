/**
 * FreeRDP: A Remote Desktop Protocol Client
 * NT LAN Manager Security Support Provider (NTLMSSP)
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

#include <time.h>
#include <openssl/des.h>
#include <openssl/md4.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/engine.h>
#include <freerdp/utils/memory.h>

#include "ntlmssp.h"

#define NTLMSSP_NEGOTIATE_56					0x80000000 /* W   (0) */
#define NTLMSSP_NEGOTIATE_KEY_EXCH				0x40000000 /* V   (1) */
#define NTLMSSP_NEGOTIATE_128					0x20000000 /* U   (2) */
#define NTLMSSP_RESERVED1					0x10000000 /* r1  (3) */
#define NTLMSSP_RESERVED2					0x08000000 /* r2  (4) */
#define NTLMSSP_RESERVED3					0x04000000 /* r3  (5) */
#define NTLMSSP_NEGOTIATE_VERSION				0x02000000 /* T   (6) */
#define NTLMSSP_RESERVED4					0x01000000 /* r4  (7) */
#define NTLMSSP_NEGOTIATE_TARGET_INFO				0x00800000 /* S   (8) */
#define NTLMSSP_RESERVEDEQUEST_NON_NT_SESSION_KEY		0x00400000 /* R   (9) */
#define NTLMSSP_RESERVED5					0x00200000 /* r5  (10) */
#define NTLMSSP_NEGOTIATE_IDENTIFY				0x00100000 /* Q   (11) */
#define NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY		0x00080000 /* P   (12) */
#define NTLMSSP_RESERVED6					0x00040000 /* r6  (13) */
#define NTLMSSP_TARGET_TYPE_SERVER				0x00020000 /* O   (14) */
#define NTLMSSP_TARGET_TYPE_DOMAIN				0x00010000 /* N   (15) */
#define NTLMSSP_NEGOTIATE_ALWAYS_SIGN				0x00008000 /* M   (16) */
#define NTLMSSP_RESERVED7					0x00004000 /* r7  (17) */
#define NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED			0x00002000 /* L   (18) */
#define NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED			0x00001000 /* K   (19) */
#define NTLMSSP_NEGOTIATE_ANONYMOUS				0x00000800 /* J   (20) */
#define NTLMSSP_RESERVED8					0x00000400 /* r8  (21) */
#define NTLMSSP_NEGOTIATE_NTLM					0x00000200 /* H   (22) */
#define NTLMSSP_RESERVED9					0x00000100 /* r9  (23) */
#define NTLMSSP_NEGOTIATE_LM_KEY				0x00000080 /* G   (24) */
#define NTLMSSP_NEGOTIATE_DATAGRAM				0x00000040 /* F   (25) */
#define NTLMSSP_NEGOTIATE_SEAL					0x00000020 /* E   (26) */
#define NTLMSSP_NEGOTIATE_SIGN					0x00000010 /* D   (27) */
#define NTLMSSP_RESERVED10					0x00000008 /* r10 (28) */
#define NTLMSSP_REQUEST_TARGET					0x00000004 /* C   (29) */
#define NTLMSSP_NEGOTIATE_OEM					0x00000002 /* B   (30) */
#define NTLMSSP_NEGOTIATE_UNICODE				0x00000001 /* A   (31) */

#define WINDOWS_MAJOR_VERSION_5		0x05
#define WINDOWS_MAJOR_VERSION_6		0x06
#define WINDOWS_MINOR_VERSION_0		0x00
#define WINDOWS_MINOR_VERSION_1		0x01
#define WINDOWS_MINOR_VERSION_2		0x02
#define NTLMSSP_REVISION_W2K3		0x0F

static const char ntlm_signature[] = "NTLMSSP";
static const char lm_magic[] = "KGS!@#$%";

static const char client_sign_magic[] = "session key to client-to-server signing key magic constant";
static const char server_sign_magic[] = "session key to server-to-client signing key magic constant";
static const char client_seal_magic[] = "session key to client-to-server sealing key magic constant";
static const char server_seal_magic[] = "session key to server-to-client sealing key magic constant";

static const char* const NTLMSSP_NEGOTIATE_STRINGS[] =
{
	"NTLMSSP_NEGOTIATE_56",
	"NTLMSSP_NEGOTIATE_KEY_EXCH",
	"NTLMSSP_NEGOTIATE_128",
	"NTLMSSP_RESERVED1",
	"NTLMSSP_RESERVED2",
	"NTLMSSP_RESERVED3",
	"NTLMSSP_NEGOTIATE_VERSION",
	"NTLMSSP_RESERVED4",
	"NTLMSSP_NEGOTIATE_TARGET_INFO",
	"NTLMSSP_REQUEST_NON_NT_SESSION_KEY",
	"NTLMSSP_RESERVED5",
	"NTLMSSP_NEGOTIATE_IDENTIFY",
	"NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY",
	"NTLMSSP_RESERVED6",
	"NTLMSSP_TARGET_TYPE_SERVER",
	"NTLMSSP_TARGET_TYPE_DOMAIN",
	"NTLMSSP_NEGOTIATE_ALWAYS_SIGN",
	"NTLMSSP_RESERVED7",
	"NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED",
	"NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED",
	"NTLMSSP_NEGOTIATE_ANONYMOUS",
	"NTLMSSP_RESERVED8",
	"NTLMSSP_NEGOTIATE_NTLM",
	"NTLMSSP_RESERVED9",
	"NTLMSSP_NEGOTIATE_LM_KEY",
	"NTLMSSP_NEGOTIATE_DATAGRAM",
	"NTLMSSP_NEGOTIATE_SEAL",
	"NTLMSSP_NEGOTIATE_SIGN",
	"NTLMSSP_RESERVED10",
	"NTLMSSP_REQUEST_TARGET",
	"NTLMSSP_NEGOTIATE_OEM",
	"NTLMSSP_NEGOTIATE_UNICODE"
};

static const char* const AV_PAIRS_STRINGS[] =
{
	"MsvAvEOL",
	"MsvAvNbComputerName",
	"MsvAvNbDomainName",
	"MsvAvDnsComputerName",
	"MsvAvDnsDomainName",
	"MsvAvDnsTreeName",
	"MsvAvFlags",
	"MsvAvTimestamp",
	"MsvAvRestrictions",
	"MsvAvTargetName",
	"MsvChannelBindings"
};

/**
 * Set NTLMSSP username.
 * @param ntlmssp
 * @param username username
 */

void ntlmssp_set_username(NTLMSSP* ntlmssp, char* username)
{
	freerdp_blob_free(&ntlmssp->username);

	if (username != NULL)
	{
		ntlmssp->username.data = freerdp_uniconv_out(ntlmssp->uniconv, username, (size_t*) &(ntlmssp->username.length));
	}
}

/**
 * Set NTLMSSP domain name.
 * @param ntlmssp
 * @param domain domain name
 */

void ntlmssp_set_domain(NTLMSSP* ntlmssp, char* domain)
{
	freerdp_blob_free(&ntlmssp->domain);

	if (domain != NULL)
	{
		ntlmssp->domain.data = freerdp_uniconv_out(ntlmssp->uniconv, domain, (size_t*) &(ntlmssp->domain.length));
	}
}

/**
 * Set NTLMSSP password.
 * @param ntlmssp
 * @param password password
 */

void ntlmssp_set_password(NTLMSSP* ntlmssp, char* password)
{
	freerdp_blob_free(&ntlmssp->password);

	if (password != NULL)
	{
		ntlmssp->password.data = freerdp_uniconv_out(ntlmssp->uniconv, password, (size_t*) &(ntlmssp->password.length));
	}
}

/**
 * Set NTLMSSP workstation.
 * @param ntlmssp
 * @param workstation workstation
 */

void ntlmssp_set_workstation(NTLMSSP* ntlmssp, char* workstation)
{
	freerdp_blob_free(&ntlmssp->workstation);

	if (workstation != NULL)
	{
		ntlmssp->workstation.data = freerdp_uniconv_out(ntlmssp->uniconv, workstation, (size_t*) &(ntlmssp->workstation.length));
	}
}

/**
 * Generate client challenge (8-byte nonce).
 * @param ntlmssp
 */

void ntlmssp_generate_client_challenge(NTLMSSP* ntlmssp)
{
	/* ClientChallenge in computation of LMv2 and NTLMv2 responses */
	crypto_nonce(ntlmssp->client_challenge, 8);
}

/**
 * Generate KeyExchangeKey (the 128-bit SessionBaseKey).\n
 * @msdn{cc236710}
 * @param ntlmssp
 */

void ntlmssp_generate_key_exchange_key(NTLMSSP* ntlmssp)
{
	/* In NTLMv2, KeyExchangeKey is the 128-bit SessionBaseKey */
	memcpy(ntlmssp->key_exchange_key, ntlmssp->session_base_key, 16);
}

/**
 * Generate RandomSessionKey (16-byte nonce).
 * @param ntlmssp
 */

void ntlmssp_generate_random_session_key(NTLMSSP* ntlmssp)
{
	crypto_nonce(ntlmssp->random_session_key, 16);
}

/**
 * Generate ExportedSessionKey (the RandomSessionKey, exported)
 * @param ntlmssp
 */

void ntlmssp_generate_exported_session_key(NTLMSSP* ntlmssp)
{
	memcpy(ntlmssp->exported_session_key, ntlmssp->random_session_key, 16);
}

/**
 * Encrypt RandomSessionKey (RC4-encrypted RandomSessionKey, using KeyExchangeKey as the key).
 * @param ntlmssp
 */

void ntlmssp_encrypt_random_session_key(NTLMSSP* ntlmssp)
{
	/* In NTLMv2, EncryptedRandomSessionKey is the ExportedSessionKey RC4-encrypted with the KeyExchangeKey */
	credssp_rc4k(ntlmssp->key_exchange_key, 16, ntlmssp->random_session_key, ntlmssp->encrypted_random_session_key);
}

/**
 * Generate timestamp for AUTHENTICATE_MESSAGE.
 * @param ntlmssp
 */

void ntlmssp_generate_timestamp(NTLMSSP* ntlmssp)
{
	credssp_current_time(ntlmssp->timestamp);

	if (ntlmssp->ntlm_v2)
	{
		if (ntlmssp->av_pairs->Timestamp.length == 8)
		{
			memcpy(ntlmssp->av_pairs->Timestamp.value, ntlmssp->timestamp, 8);
			return;
		}
	}
	else
	{
		if (ntlmssp->av_pairs->Timestamp.length != 8)
		{
			ntlmssp->av_pairs->Timestamp.length = 8;
			ntlmssp->av_pairs->Timestamp.value = xmalloc(ntlmssp->av_pairs->Timestamp.length);
		}
		memcpy(ntlmssp->av_pairs->Timestamp.value, ntlmssp->timestamp, 8);
	}
}

/**
 * Generate signing key.\n
 * @msdn{cc236711}
 * @param exported_session_key ExportedSessionKey
 * @param sign_magic Sign magic string
 * @param signing_key Destination signing key
 */

void ntlmssp_generate_signing_key(uint8* exported_session_key, rdpBlob* sign_magic, uint8* signing_key)
{
	int length;
	uint8* value;
	CryptoMd5 md5;

	length = 16 + sign_magic->length;
	value = (uint8*) xmalloc(length);

	/* Concatenate ExportedSessionKey with sign magic */
	memcpy(value, exported_session_key, 16);
	memcpy(&value[16], sign_magic->data, sign_magic->length);

	md5 = crypto_md5_init();
	crypto_md5_update(md5, value, length);
	crypto_md5_final(md5, signing_key);

	xfree(value);
}

/**
 * Generate client signing key (ClientSigningKey).\n
 * @msdn{cc236711}
 * @param ntlmssp
 */

void ntlmssp_generate_client_signing_key(NTLMSSP* ntlmssp)
{
	rdpBlob sign_magic;
	sign_magic.data = (void*) client_sign_magic;
	sign_magic.length = sizeof(client_sign_magic);
	ntlmssp_generate_signing_key(ntlmssp->exported_session_key, &sign_magic, ntlmssp->client_signing_key);
}

/**
 * Generate server signing key (ServerSigningKey).\n
 * @msdn{cc236711}
 * @param ntlmssp
 */

void ntlmssp_generate_server_signing_key(NTLMSSP* ntlmssp)
{
	rdpBlob sign_magic;
	sign_magic.data = (void*) server_sign_magic;
	sign_magic.length = sizeof(server_sign_magic);
	ntlmssp_generate_signing_key(ntlmssp->exported_session_key, &sign_magic, ntlmssp->server_signing_key);
}

/**
 * Generate sealing key.\n
 * @msdn{cc236712}
 * @param exported_session_key ExportedSessionKey
 * @param seal_magic Seal magic string
 * @param sealing_key Destination sealing key
 */

void ntlmssp_generate_sealing_key(uint8* exported_session_key, rdpBlob* seal_magic, uint8* sealing_key)
{
	uint8* p;
	CryptoMd5 md5;
	rdpBlob blob;

	freerdp_blob_alloc(&blob, 16 + seal_magic->length);
	p = (uint8*) blob.data;

	/* Concatenate ExportedSessionKey with seal magic */
	memcpy(p, exported_session_key, 16);
	memcpy(&p[16], seal_magic->data, seal_magic->length);

	md5 = crypto_md5_init();
	crypto_md5_update(md5, blob.data, blob.length);
	crypto_md5_final(md5, sealing_key);

	freerdp_blob_free(&blob);
}

/**
 * Generate client sealing key (ClientSealingKey).\n
 * @msdn{cc236712}
 * @param ntlmssp
 */

void ntlmssp_generate_client_sealing_key(NTLMSSP* ntlmssp)
{
	rdpBlob seal_magic;
	seal_magic.data = (void*) client_seal_magic;
	seal_magic.length = sizeof(client_seal_magic);
	ntlmssp_generate_signing_key(ntlmssp->exported_session_key, &seal_magic, ntlmssp->client_sealing_key);
}

/**
 * Generate server sealing key (ServerSealingKey).\n
 * @msdn{cc236712}
 * @param ntlmssp
 */

void ntlmssp_generate_server_sealing_key(NTLMSSP* ntlmssp)
{
	rdpBlob seal_magic;
	seal_magic.data = (void*) server_seal_magic;
	seal_magic.length = sizeof(server_seal_magic);
	ntlmssp_generate_signing_key(ntlmssp->exported_session_key, &seal_magic, ntlmssp->server_sealing_key);
}

/**
 * Initialize RC4 stream cipher states for sealing.
 * @param ntlmssp
 */

void ntlmssp_init_rc4_seal_states(NTLMSSP* ntlmssp)
{
	ntlmssp->send_rc4_seal = crypto_rc4_init(ntlmssp->client_sealing_key, 16);
	ntlmssp->recv_rc4_seal = crypto_rc4_init(ntlmssp->server_sealing_key, 16);
}

/**
 * Get bit from a byte buffer using a bit offset.
 * @param buffer byte buffer
 * @param bit bit offset
 * @return bit value
 */

static int get_bit(char* buffer, int bit)
{
	return (buffer[(bit - (bit % 8)) / 8] >> (7 - bit % 8) & 1);
}

/**
 * Set bit in a byte buffer using a bit offset.
 * @param buffer byte buffer
 * @param bit bit offset
 * @param value bit value
 */

static void set_bit(char* buffer, int bit, int value)
{
	buffer[(bit - (bit % 8)) / 8] |= value << (7 - bit % 8);
}

static void ntlmssp_compute_des_key(char* text, char* des_key)
{
	int i, j;
	int bit;
	int nbits;

	/* Convert the 7 bytes into a bit stream, and insert a parity-bit (odd parity) after every seven bits. */

	memset(des_key, '\0', 8);

	for (i = 0; i < 8; i++)
	{
		nbits = 0;

		for (j = 0; j < 7; j++)
		{
			/* copy 7 bits, and count the number of bits that are set */

			bit = get_bit(text, i*7 + j);
			set_bit(des_key, i*7 + i + j, bit);
			nbits += bit;
		}

		/* insert parity bit (odd parity) */

		if (nbits % 2 == 0)
			set_bit(des_key, i*7 + i + j, 1);
	}
}

void ntlmssp_compute_lm_hash(char* password, char* hash)
{
	int i;
	int maxlen;
	char text[14];
	char des_key1[8];
	char des_key2[8];
	des_key_schedule ks;

	/* LM("password") = E52CAC67419A9A224A3B108F3FA6CB6D */

	maxlen = (strlen(password) < 14) ? strlen(password) : 14;

	/* convert to uppercase */
	for (i = 0; i < maxlen; i++)
	{
		if ((password[i] >= 'a') && (password[i] <= 'z'))
			text[i] = password[i] - 32;
		else
			text[i] = password[i];
	}

	/* pad with nulls up to 14 bytes */
	for (i = maxlen; i < 14; i++)
		text[i] = '\0';

	ntlmssp_compute_des_key(text, des_key1);
	ntlmssp_compute_des_key(&text[7], des_key2);

	DES_set_key((const_DES_cblock*)des_key1, &ks);
	DES_ecb_encrypt((const_DES_cblock*) lm_magic, (DES_cblock*)hash, &ks, DES_ENCRYPT);

	DES_set_key((const_DES_cblock*)des_key2, &ks);
	DES_ecb_encrypt((const_DES_cblock*) lm_magic, (DES_cblock*)&hash[8], &ks, DES_ENCRYPT);
}

void ntlmssp_compute_ntlm_hash(rdpBlob* password, char* hash)
{
	/* NTLMv1("password") = 8846F7EAEE8FB117AD06BDD830B7586C */

	MD4_CTX md4_ctx;

	/* Password needs to be in unicode */

	/* Apply the MD4 digest algorithm on the password in unicode, the result is the NTLM hash */

	MD4_Init(&md4_ctx);
	MD4_Update(&md4_ctx, password->data, password->length);
	MD4_Final((void*) hash, &md4_ctx);
}

void ntlmssp_compute_ntlm_v2_hash(NTLMSSP* ntlmssp, char* hash)
{
	char* p;
	rdpBlob blob;
	char ntlm_hash[16];

	freerdp_blob_alloc(&blob, ntlmssp->username.length + ntlmssp->domain.length);
	p = (char*) blob.data;

	/* First, compute the NTLMv1 hash of the password */
	ntlmssp_compute_ntlm_hash(&ntlmssp->password, ntlm_hash);

	/* Concatenate(Uppercase(username),domain)*/
	memcpy(p, ntlmssp->username.data, ntlmssp->username.length);
	freerdp_uniconv_uppercase(ntlmssp->uniconv, p, ntlmssp->username.length / 2);

	memcpy(&p[ntlmssp->username.length], ntlmssp->domain.data, ntlmssp->domain.length);

	/* Compute the HMAC-MD5 hash of the above value using the NTLMv1 hash as the key, the result is the NTLMv2 hash */
	HMAC(EVP_md5(), (void*) ntlm_hash, 16, blob.data, blob.length, (void*) hash, NULL);

	freerdp_blob_free(&blob);
}

void ntlmssp_compute_lm_response(char* password, char* challenge, char* response)
{
	char hash[21];
	char des_key1[8];
	char des_key2[8];
	char des_key3[8];
	des_key_schedule ks;

	/* A LM hash is 16-bytes long, but the LM response uses a LM hash null-padded to 21 bytes */
	memset(hash, '\0', 21);
	ntlmssp_compute_lm_hash(password, hash);

	/* Each 7-byte third of the 21-byte null-padded LM hash is used to create a DES key */
	ntlmssp_compute_des_key(hash, des_key1);
	ntlmssp_compute_des_key(&hash[7], des_key2);
	ntlmssp_compute_des_key(&hash[14], des_key3);

	/* Encrypt the LM challenge with each key, and concatenate the result. This is the LM response (24 bytes) */
	DES_set_key((const_DES_cblock*)des_key1, &ks);
	DES_ecb_encrypt((const_DES_cblock*)challenge, (DES_cblock*)response, &ks, DES_ENCRYPT);

	DES_set_key((const_DES_cblock*)des_key2, &ks);
	DES_ecb_encrypt((const_DES_cblock*)challenge, (DES_cblock*)&response[8], &ks, DES_ENCRYPT);

	DES_set_key((const_DES_cblock*)des_key3, &ks);
	DES_ecb_encrypt((const_DES_cblock*)challenge, (DES_cblock*)&response[16], &ks, DES_ENCRYPT);
}

void ntlmssp_compute_lm_v2_response(NTLMSSP* ntlmssp)
{
	char *response;
	char value[16];
	char ntlm_v2_hash[16];

	/* Compute the NTLMv2 hash */
	ntlmssp_compute_ntlm_v2_hash(ntlmssp, ntlm_v2_hash);

	/* Concatenate the server and client challenges */
	memcpy(value, ntlmssp->server_challenge, 8);
	memcpy(&value[8], ntlmssp->client_challenge, 8);

	freerdp_blob_alloc(&ntlmssp->lm_challenge_response, 24);
	response = (char*) ntlmssp->lm_challenge_response.data;

	/* Compute the HMAC-MD5 hash of the resulting value using the NTLMv2 hash as the key */
	HMAC(EVP_md5(), (void*) ntlm_v2_hash, 16, (void*) value, 16, (void*) response, NULL);

	/* Concatenate the resulting HMAC-MD5 hash and the client challenge, giving us the LMv2 response (24 bytes) */
	memcpy(&response[16], ntlmssp->client_challenge, 8);
}

/**
 * Compute NTLMv2 Response.\n
 * NTLMv2_RESPONSE @msdn{cc236653}\n
 * NTLMv2 Authentication @msdn{cc236700}
 * @param ntlmssp
 */

void ntlmssp_compute_ntlm_v2_response(NTLMSSP* ntlmssp)
{
	uint8* blob;
	uint8 ntlm_v2_hash[16];
	uint8 nt_proof_str[16];
	rdpBlob ntlm_v2_temp;
	rdpBlob ntlm_v2_temp_chal;

	freerdp_blob_alloc(&ntlm_v2_temp, ntlmssp->target_info.length + 28);

	memset(ntlm_v2_temp.data, '\0', ntlm_v2_temp.length);
	blob = (uint8*) ntlm_v2_temp.data;

	/* Compute the NTLMv2 hash */
	ntlmssp_compute_ntlm_v2_hash(ntlmssp, (char*) ntlm_v2_hash);

#ifdef WITH_DEBUG_NLA
	printf("Password (length = %d)\n", ntlmssp->password.length);
	freerdp_hexdump(ntlmssp->password.data, ntlmssp->password.length);
	printf("\n");

	printf("Username (length = %d)\n", ntlmssp->username.length);
	freerdp_hexdump(ntlmssp->username.data, ntlmssp->username.length);
	printf("\n");

	printf("Domain (length = %d)\n", ntlmssp->domain.length);
	freerdp_hexdump(ntlmssp->domain.data, ntlmssp->domain.length);
	printf("\n");

	printf("Workstation (length = %d)\n", ntlmssp->workstation.length);
	freerdp_hexdump(ntlmssp->workstation.data, ntlmssp->workstation.length);
	printf("\n");

	printf("NTOWFv2, NTLMv2 Hash\n");
	freerdp_hexdump(ntlm_v2_hash, 16);
	printf("\n");
#endif

	/* Construct temp */
	blob[0] = 1; /* RespType (1 byte) */
	blob[1] = 1; /* HighRespType (1 byte) */
	/* Reserved1 (2 bytes) */
	/* Reserved2 (4 bytes) */
	memcpy(&blob[8], ntlmssp->av_pairs->Timestamp.value, 8); /* Timestamp (8 bytes) */
	memcpy(&blob[16], ntlmssp->client_challenge, 8); /* ClientChallenge (8 bytes) */
	/* Reserved3 (4 bytes) */
	memcpy(&blob[28], ntlmssp->target_info.data, ntlmssp->target_info.length);

#ifdef WITH_DEBUG_NLA
	printf("NTLMv2 Response Temp Blob\n");
	freerdp_hexdump(ntlm_v2_temp.data, ntlm_v2_temp.length);
	printf("\n");
#endif

	/* Concatenate server challenge with temp */
	freerdp_blob_alloc(&ntlm_v2_temp_chal, ntlm_v2_temp.length + 8);
	blob = (uint8*) ntlm_v2_temp_chal.data;
	memcpy(blob, ntlmssp->server_challenge, 8);
	memcpy(&blob[8], ntlm_v2_temp.data, ntlm_v2_temp.length);

	HMAC(EVP_md5(), (void*) ntlm_v2_hash, 16, (void*) ntlm_v2_temp_chal.data,
		ntlm_v2_temp_chal.length, (void*) nt_proof_str, NULL);

	/* NtChallengeResponse, Concatenate NTProofStr with temp */
	freerdp_blob_alloc(&ntlmssp->nt_challenge_response, ntlm_v2_temp.length + 16);
	blob = (uint8*) ntlmssp->nt_challenge_response.data;
	memcpy(blob, nt_proof_str, 16);
	memcpy(&blob[16], ntlm_v2_temp.data, ntlm_v2_temp.length);

	/* Compute SessionBaseKey, the HMAC-MD5 hash of NTProofStr using the NTLMv2 hash as the key */
	HMAC(EVP_md5(), (void*) ntlm_v2_hash, 16,
		(void*) nt_proof_str, 16, (void*) ntlmssp->session_base_key, NULL);

	freerdp_blob_free(&ntlm_v2_temp);
	freerdp_blob_free(&ntlm_v2_temp_chal);
}

/**
 * Input NegotiateFlags, a 4-byte bit map.
 * @param s
 * @param flags
 */

void ntlmssp_input_negotiate_flags(STREAM* s, uint32* flags)
{
	*flags = 0;
	stream_read_uint32(s, *flags);
}

/**
 * Output NegotiateFlags, a 4-byte bit map.
 * @param s
 * @param flags
 */

void ntlmssp_output_negotiate_flags(STREAM* s, uint32 flags)
{
	stream_write_uint32(s, flags);
}

void ntlmssp_print_negotiate_flags(uint32 flags)
{
	int i;
	const char* str;

	printf("negotiateFlags \"0x%08X\"{\n", flags);

	for (i = 31; i >= 0; i--)
	{
		if ((flags >> i) & 1)
		{
			str = NTLMSSP_NEGOTIATE_STRINGS[(31 - i)];
			printf("\t%s (%d),\n", str, (31 - i));
		}
	}

	printf("}\n");
}

/**
 * Output Restriction_Encoding.\n
 * Restriction_Encoding @msdn{cc236647}
 * @param ntlmssp
 */

static void ntlmssp_output_restriction_encoding(NTLMSSP* ntlmssp)
{
	AV_PAIR *restrictions = &ntlmssp->av_pairs->Restrictions;
	STREAM* s = stream_new(0);

	uint8 machineID[32] =
		"\x3A\x15\x8E\xA6\x75\x82\xD8\xF7\x3E\x06\xFA\x7A\xB4\xDF\xFD\x43"
		"\x84\x6C\x02\x3A\xFD\x5A\x94\xFE\xCF\x97\x0F\x3D\x19\x2C\x38\x20";

	restrictions->value = xmalloc(48);
	restrictions->length = 48;

	s->data = restrictions->value;
	s->size = restrictions->length;
	s->p = s->data;

	stream_write_uint32(s, 48); /* Size */
	stream_write_zero(s, 4); /* Z4 (set to zero) */

	/* IntegrityLevel (bit 31 set to 1) */
	stream_write_uint8(s, 1);
	stream_write_zero(s, 3);

	stream_write_uint32(s, 0x00002000); /* SubjectIntegrityLevel */
	stream_write(s, machineID, 32); /* MachineID */

	xfree(s);
}

/**
 * Output TargetName.\n
 * @param ntlmssp
 */

void ntlmssp_output_target_name(NTLMSSP* ntlmssp)
{
	STREAM* s = stream_new(0);
	AV_PAIR* target_name = &ntlmssp->av_pairs->TargetName;

	/*
	 * TODO: No idea what should be set here (observed MsvAvTargetName = MsvAvDnsComputerName or
	 * MsvAvTargetName should be the name of the service be accessed after authentication)
	 * here used: "TERMSRV/192.168.0.123" in unicode (Dmitrij Jasnov)
	 */
	uint8 name[42] =
			"\x54\x00\x45\x00\x52\x00\x4d\x00\x53\x00\x52\x00\x56\x00\x2f\x00\x31\x00\x39\x00\x32"
			"\x00\x2e\x00\x31\x00\x36\x00\x38\x00\x2e\x00\x30\x00\x2e\x00\x31\x00\x32\x00\x33\x00";

	target_name->length = 42;
	target_name->value = (uint8*) xmalloc(target_name->length);

	s->data = target_name->value;
	s->size = target_name->length;
	s->p = s->data;

	stream_write(s, name, target_name->length);

	xfree(s);
}

/**
 * Output ChannelBindings.\n
 * @param ntlmssp
 */

void ntlmssp_output_channel_bindings(NTLMSSP* ntlmssp)
{
	STREAM* s = stream_new(0);
	AV_PAIR* channel_bindings = &ntlmssp->av_pairs->ChannelBindings;

	channel_bindings->value = (uint8*) xmalloc(48);
	channel_bindings->length = 16;

	s->data = channel_bindings->value;
	s->size = channel_bindings->length;
	s->p = s->data;

	stream_write_zero(s, 16); /* an all-zero value of the hash is used to indicate absence of channel bindings */

	xfree(s);
}

/**
 * Populate array of AV_PAIRs.\n
 * AV_PAIR @msdn{cc236646}
 * @param ntlmssp
 */

void ntlmssp_populate_av_pairs(NTLMSSP* ntlmssp)
{
	STREAM* s;
	rdpBlob target_info;
	AV_PAIRS *av_pairs = ntlmssp->av_pairs;

	/* MsvAvFlags */
	av_pairs->Flags = 0x00000002; /* Indicates the present of a Message Integrity Check (MIC) */

	/* Restriction_Encoding */
	ntlmssp_output_restriction_encoding(ntlmssp);

	/* TargetName */
	ntlmssp_output_target_name(ntlmssp);

	/* ChannelBindings */
	ntlmssp_output_channel_bindings(ntlmssp);

	s = stream_new(0);
	s->data = xmalloc(ntlmssp->target_info.length + 512);
	s->p = s->data;

	ntlmssp_output_av_pairs(ntlmssp, s);
	freerdp_blob_alloc(&target_info, s->p - s->data);
	memcpy(target_info.data, s->data, target_info.length);

	ntlmssp->target_info.data = target_info.data;
	ntlmssp->target_info.length = target_info.length;
}

/**
 * Input array of AV_PAIRs.\n
 * AV_PAIR @msdn{cc236646}
 * @param ntlmssp
 * @param s
 */

void ntlmssp_input_av_pairs(NTLMSSP* ntlmssp, STREAM* s)
{
	AV_ID AvId;
	uint16 AvLen;
	uint8* value;
	AV_PAIRS* av_pairs = ntlmssp->av_pairs;

#ifdef WITH_DEBUG_NLA
	printf("AV_PAIRS = {\n");
#endif

	do
	{
		value = NULL;
		stream_read_uint16(s, AvId);
		stream_read_uint16(s, AvLen);

		if (AvLen > 0)
		{
			if (AvId != MsvAvFlags)
			{
				value = xmalloc(AvLen);
				stream_read(s, value, AvLen);
			}
			else
			{
				stream_read_uint32(s, av_pairs->Flags);
			}
		}

		switch (AvId)
		{
			case MsvAvNbComputerName:
				av_pairs->NbComputerName.length = AvLen;
				av_pairs->NbComputerName.value = value;
				break;

			case MsvAvNbDomainName:
				av_pairs->NbDomainName.length = AvLen;
				av_pairs->NbDomainName.value = value;
				break;

			case MsvAvDnsComputerName:
				av_pairs->DnsComputerName.length = AvLen;
				av_pairs->DnsComputerName.value = value;
				break;

			case MsvAvDnsDomainName:
				av_pairs->DnsDomainName.length = AvLen;
				av_pairs->DnsDomainName.value = value;
				break;

			case MsvAvDnsTreeName:
				av_pairs->DnsTreeName.length = AvLen;
				av_pairs->DnsTreeName.value = value;
				break;

			case MsvAvTimestamp:
				av_pairs->Timestamp.length = AvLen;
				av_pairs->Timestamp.value = value;
				break;

			case MsvAvRestrictions:
				av_pairs->Restrictions.length = AvLen;
				av_pairs->Restrictions.value = value;
				break;

			case MsvAvTargetName:
				av_pairs->TargetName.length = AvLen;
				av_pairs->TargetName.value = value;
				break;

			case MsvChannelBindings:
				av_pairs->ChannelBindings.length = AvLen;
				av_pairs->ChannelBindings.value = value;
				break;

			default:
				if (value != NULL)
					xfree(value);
				break;
		}

#ifdef WITH_DEBUG_NLA
		if (AvId < 10)
			printf("\tAvId: %s, AvLen: %d\n", AV_PAIRS_STRINGS[AvId], AvLen);
		else
			printf("\tAvId: %s, AvLen: %d\n", "Unknown", AvLen);

		freerdp_hexdump(value, AvLen);
#endif
	}
	while (AvId != MsvAvEOL);

#ifdef WITH_DEBUG_NLA
	printf("}\n");
#endif
}

/**
 * Output array of AV_PAIRs.\n
 * AV_PAIR @msdn{cc236646}
 * @param ntlmssp
 * @param s
 */

void ntlmssp_output_av_pairs(NTLMSSP* ntlmssp, STREAM* s)
{
	AV_PAIRS* av_pairs = ntlmssp->av_pairs;
	
	if (av_pairs->NbDomainName.length > 0)
	{
		stream_write_uint16(s, MsvAvNbDomainName); /* AvId */
		stream_write_uint16(s, av_pairs->NbDomainName.length); /* AvLen */
		stream_write(s, av_pairs->NbDomainName.value, av_pairs->NbDomainName.length); /* Value */
	}

	if (av_pairs->NbComputerName.length > 0)
	{
		stream_write_uint16(s, MsvAvNbComputerName); /* AvId */
		stream_write_uint16(s, av_pairs->NbComputerName.length); /* AvLen */
		stream_write(s, av_pairs->NbComputerName.value, av_pairs->NbComputerName.length); /* Value */
	}

	if (av_pairs->DnsDomainName.length > 0)
	{
		stream_write_uint16(s, MsvAvDnsDomainName); /* AvId */
		stream_write_uint16(s, av_pairs->DnsDomainName.length); /* AvLen */
		stream_write(s, av_pairs->DnsDomainName.value, av_pairs->DnsDomainName.length); /* Value */
	}

	if (av_pairs->DnsComputerName.length > 0)
	{
		stream_write_uint16(s, MsvAvDnsComputerName); /* AvId */
		stream_write_uint16(s, av_pairs->DnsComputerName.length); /* AvLen */
		stream_write(s, av_pairs->DnsComputerName.value, av_pairs->DnsComputerName.length); /* Value */
	}

	if (av_pairs->DnsTreeName.length > 0)
	{
		stream_write_uint16(s, MsvAvDnsTreeName); /* AvId */
		stream_write_uint16(s, av_pairs->DnsTreeName.length); /* AvLen */
		stream_write(s, av_pairs->DnsTreeName.value, av_pairs->DnsTreeName.length); /* Value */
	}

	if (av_pairs->Timestamp.length > 0)
	{
		stream_write_uint16(s, MsvAvTimestamp); /* AvId */
		stream_write_uint16(s, av_pairs->Timestamp.length); /* AvLen */
		stream_write(s, av_pairs->Timestamp.value, av_pairs->Timestamp.length); /* Value */
	}

	if (av_pairs->Flags > 0)
	{
		stream_write_uint16(s, MsvAvFlags); /* AvId */
		stream_write_uint16(s, 4); /* AvLen */
		stream_write_uint32(s, av_pairs->Flags); /* Value */
	}

	if (av_pairs->Restrictions.length > 0)
	{
		stream_write_uint16(s, MsvAvRestrictions); /* AvId */
		stream_write_uint16(s, av_pairs->Restrictions.length); /* AvLen */
		stream_write(s, av_pairs->Restrictions.value, av_pairs->Restrictions.length); /* Value */
	}

	if (av_pairs->ChannelBindings.length > 0)
	{
		stream_write_uint16(s, MsvChannelBindings); /* AvId */
		stream_write_uint16(s, av_pairs->ChannelBindings.length); /* AvLen */
		stream_write(s, av_pairs->ChannelBindings.value, av_pairs->ChannelBindings.length); /* Value */
	}

	if (av_pairs->TargetName.length > 0)
	{
		stream_write_uint16(s, MsvAvTargetName); /* AvId */
		stream_write_uint16(s, av_pairs->TargetName.length); /* AvLen */
		stream_write(s, av_pairs->TargetName.value, av_pairs->TargetName.length); /* Value */
	}

	/* This indicates the end of the AV_PAIR array */
	stream_write_uint16(s, MsvAvEOL); /* AvId */
	stream_write_uint16(s, 0); /* AvLen */

	if (ntlmssp->ntlm_v2)
	{
		stream_write_zero(s, 8);
	}
}

/**
 * Print array of AV_PAIRs.\n
 * AV_PAIR @msdn{cc236646}
 * @param ntlmssp
 * @param s
 */

void ntlmssp_print_av_pairs(NTLMSSP* ntlmssp)
{
	AV_PAIRS* av_pairs = ntlmssp->av_pairs;

	printf("AV_PAIRS = {\n");

	if (av_pairs->NbDomainName.length > 0)
	{
		printf("\tAvId: MsvAvNbDomainName AvLen: %d\n", av_pairs->NbDomainName.length);
		freerdp_hexdump(av_pairs->NbDomainName.value, av_pairs->NbDomainName.length);
	}

	if (av_pairs->NbComputerName.length > 0)
	{
		printf("\tAvId: MsvAvNbComputerName AvLen: %d\n", av_pairs->NbComputerName.length);
		freerdp_hexdump(av_pairs->NbComputerName.value, av_pairs->NbComputerName.length);
	}

	if (av_pairs->DnsDomainName.length > 0)
	{
		printf("\tAvId: MsvAvDnsDomainName AvLen: %d\n", av_pairs->DnsDomainName.length);
		freerdp_hexdump(av_pairs->DnsDomainName.value, av_pairs->DnsDomainName.length);
	}

	if (av_pairs->DnsComputerName.length > 0)
	{
		printf("\tAvId: MsvAvDnsComputerName AvLen: %d\n", av_pairs->DnsComputerName.length);
		freerdp_hexdump(av_pairs->DnsComputerName.value, av_pairs->DnsComputerName.length);
	}

	if (av_pairs->DnsTreeName.length > 0)
	{
		printf("\tAvId: MsvAvDnsTreeName AvLen: %d\n", av_pairs->DnsTreeName.length);
		freerdp_hexdump(av_pairs->DnsTreeName.value, av_pairs->DnsTreeName.length);
	}

	if (av_pairs->Timestamp.length > 0)
	{
		printf("\tAvId: MsvAvTimestamp AvLen: %d\n", av_pairs->Timestamp.length);
		freerdp_hexdump(av_pairs->Timestamp.value, av_pairs->Timestamp.length);
	}

	if (av_pairs->Flags > 0)
	{
		printf("\tAvId: MsvAvFlags AvLen: %d\n", 4);
		printf("0x%08X\n", av_pairs->Flags);
	}

	if (av_pairs->Restrictions.length > 0)
	{
		printf("\tAvId: MsvAvRestrictions AvLen: %d\n", av_pairs->Restrictions.length);
		freerdp_hexdump(av_pairs->Restrictions.value, av_pairs->Restrictions.length);
	}

	if (av_pairs->ChannelBindings.length > 0)
	{
		printf("\tAvId: MsvChannelBindings AvLen: %d\n", av_pairs->ChannelBindings.length);
		freerdp_hexdump(av_pairs->ChannelBindings.value, av_pairs->ChannelBindings.length);
	}

	if (av_pairs->TargetName.length > 0)
	{
		printf("\tAvId: MsvAvTargetName AvLen: %d\n", av_pairs->TargetName.length);
		freerdp_hexdump(av_pairs->TargetName.value, av_pairs->TargetName.length);
	}

	printf("}\n");
}

/**
 * Free array of AV_PAIRs.\n
 * AV_PAIR @msdn{cc236646}
 * @param ntlmssp
 */

void ntlmssp_free_av_pairs(NTLMSSP* ntlmssp)
{
	AV_PAIRS *av_pairs = ntlmssp->av_pairs;
	
	if (av_pairs != NULL)
	{
		if (av_pairs->NbComputerName.value != NULL)
			xfree(av_pairs->NbComputerName.value);
		if (av_pairs->NbDomainName.value != NULL)
			xfree(av_pairs->NbDomainName.value);
		if (av_pairs->DnsComputerName.value != NULL)
			xfree(av_pairs->DnsComputerName.value);
		if (av_pairs->DnsDomainName.value != NULL)
			xfree(av_pairs->DnsDomainName.value);
		if (av_pairs->DnsTreeName.value != NULL)
			xfree(av_pairs->DnsTreeName.value);
		if (av_pairs->Timestamp.value != NULL)
			xfree(av_pairs->Timestamp.value);
		if (av_pairs->Restrictions.value != NULL)
			xfree(av_pairs->Restrictions.value);
		if (av_pairs->TargetName.value != NULL)
			xfree(av_pairs->TargetName.value);
		if (av_pairs->ChannelBindings.value != NULL)
			xfree(av_pairs->ChannelBindings.value);

		xfree(av_pairs);
	}

	ntlmssp->av_pairs = NULL;
}

/**
 * Output VERSION structure.\n
 * VERSION @msdn{cc236654}
 * @param s
 */

static void ntlmssp_output_version(STREAM* s)
{
	/* The following version information was observed with Windows 7 */

	stream_write_uint8(s, WINDOWS_MAJOR_VERSION_6); /* ProductMajorVersion (1 byte) */
	stream_write_uint8(s, WINDOWS_MINOR_VERSION_1); /* ProductMinorVersion (1 byte) */
	stream_write_uint16(s, 7600); /* ProductBuild (2 bytes) */
	stream_write_zero(s, 3); /* Reserved (3 bytes) */
	stream_write_uint8(s, NTLMSSP_REVISION_W2K3); /* NTLMRevisionCurrent (1 byte) */
}

void ntlmssp_compute_message_integrity_check(NTLMSSP* ntlmssp)
{
	HMAC_CTX hmac_ctx;

	/* 
	 * Compute the HMAC-MD5 hash of ConcatenationOf(NEGOTIATE_MESSAGE,
	 * CHALLENGE_MESSAGE, AUTHENTICATE_MESSAGE) using the ExportedSessionKey
	 */

	HMAC_CTX_init(&hmac_ctx);
	HMAC_Init_ex(&hmac_ctx, ntlmssp->exported_session_key, 16, EVP_md5(), NULL);
	HMAC_Update(&hmac_ctx, ntlmssp->negotiate_message.data, ntlmssp->negotiate_message.length);
	HMAC_Update(&hmac_ctx, ntlmssp->challenge_message.data, ntlmssp->challenge_message.length);
	HMAC_Update(&hmac_ctx, ntlmssp->authenticate_message.data, ntlmssp->authenticate_message.length);
	HMAC_Final(&hmac_ctx, ntlmssp->message_integrity_check, NULL);
}

/**
 * Encrypt and sign message using NTLMSSP.\n
 * GSS_WrapEx() @msdn{cc236718}\n
 * EncryptMessage() @msdn{aa375378}
 * @param ntlmssp
 * @param[in] msg message to encrypt
 * @param[out] encrypted_msg encrypted message
 * @param[out] signature destination signature
 */

void ntlmssp_encrypt_message(NTLMSSP* ntlmssp, rdpBlob* msg, rdpBlob* encrypted_msg, uint8* signature)
{
	HMAC_CTX hmac_ctx;
	uint8 digest[16];
	uint8 checksum[8];
	uint32 version = 1;

	/* Compute the HMAC-MD5 hash of ConcatenationOf(seq_num,msg) using the client signing key */
	HMAC_CTX_init(&hmac_ctx);
	HMAC_Init_ex(&hmac_ctx, ntlmssp->client_signing_key, 16, EVP_md5(), NULL);
	HMAC_Update(&hmac_ctx, (void*) &ntlmssp->send_seq_num, 4);
	HMAC_Update(&hmac_ctx, msg->data, msg->length);
	HMAC_Final(&hmac_ctx, digest, NULL);

	/* Allocate space for encrypted message */
	freerdp_blob_alloc(encrypted_msg, msg->length);

	/* Encrypt message using with RC4 */
	crypto_rc4(ntlmssp->send_rc4_seal, msg->length, msg->data, encrypted_msg->data);

	/* RC4-encrypt first 8 bytes of digest */
	crypto_rc4(ntlmssp->send_rc4_seal, 8, digest, checksum);

	/* Concatenate version, ciphertext and sequence number to build signature */
	memcpy(signature, (void*) &version, 4);
	memcpy(&signature[4], (void*) checksum, 8);
	memcpy(&signature[12], (void*) &(ntlmssp->send_seq_num), 4);

	HMAC_CTX_cleanup(&hmac_ctx);

	ntlmssp->send_seq_num++;
}

/**
 * Decrypt message and verify signature using NTLMSSP.\n
 * GSS_UnwrapEx() @msdn{cc236703}\n
 * DecryptMessage() @msdn{aa375211}
 * @param ntlmssp
 * @param[in] encrypted_msg encrypted message
 * @param[out] msg decrypted message
 * @param[in] signature signature
 * @return
 */

int ntlmssp_decrypt_message(NTLMSSP* ntlmssp, rdpBlob* encrypted_msg, rdpBlob* msg, uint8* signature)
{
	HMAC_CTX hmac_ctx;
	uint8 digest[16];
	uint8 checksum[8];
	uint32 version = 1;
	uint8 expected_signature[16];

	/* Allocate space for encrypted message */
	freerdp_blob_alloc(msg, encrypted_msg->length);

	/* Encrypt message using with RC4 */
	crypto_rc4(ntlmssp->recv_rc4_seal, encrypted_msg->length, encrypted_msg->data, msg->data);

	/* Compute the HMAC-MD5 hash of ConcatenationOf(seq_num,msg) using the client signing key */
	HMAC_CTX_init(&hmac_ctx);
	HMAC_Init_ex(&hmac_ctx, ntlmssp->server_signing_key, 16, EVP_md5(), NULL);
	HMAC_Update(&hmac_ctx, (void*) &ntlmssp->recv_seq_num, 4);
	HMAC_Update(&hmac_ctx, msg->data, msg->length);
	HMAC_Final(&hmac_ctx, digest, NULL);

	/* RC4-encrypt first 8 bytes of digest */
	crypto_rc4(ntlmssp->recv_rc4_seal, 8, digest, checksum);

	/* Concatenate version, ciphertext and sequence number to build signature */
	memcpy(expected_signature, (void*) &version, 4);
	memcpy(&expected_signature[4], (void*) checksum, 8);
	memcpy(&expected_signature[12], (void*) &(ntlmssp->recv_seq_num), 4);

	if (memcmp(signature, expected_signature, 16) != 0)
	{
		/* signature verification failed! */
		printf("signature verification failed, something nasty is going on!\n");
		return 0;
	}

	HMAC_CTX_cleanup(&hmac_ctx);

	ntlmssp->recv_seq_num++;
	return 1;
}

/**
 * Send NTLMSSP NEGOTIATE_MESSAGE.\n
 * NEGOTIATE_MESSAGE @msdn{cc236641}
 * @param ntlmssp
 * @param s
 */

void ntlmssp_send_negotiate_message(NTLMSSP* ntlmssp, STREAM* s)
{
	int length;
	uint32 negotiateFlags = 0;

	stream_write(s, ntlm_signature, 8); /* Signature (8 bytes) */
	stream_write_uint32(s, 1); /* MessageType */

	if (ntlmssp->ntlm_v2)
	{
		DEBUG_NLA("Negotiating NTLMv2");
		/* observed: B7 82 08 E2 (0xE20882B7) (Dmitrij Jasnov) */
		negotiateFlags |= NTLMSSP_NEGOTIATE_56;
		negotiateFlags |= NTLMSSP_NEGOTIATE_KEY_EXCH;
		negotiateFlags |= NTLMSSP_NEGOTIATE_128;
		negotiateFlags |= NTLMSSP_NEGOTIATE_VERSION;
		negotiateFlags |= NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY;
		negotiateFlags |= NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
		negotiateFlags |= NTLMSSP_NEGOTIATE_NTLM;
		negotiateFlags |= NTLMSSP_NEGOTIATE_LM_KEY;
		negotiateFlags |= NTLMSSP_NEGOTIATE_SEAL;
		negotiateFlags |= NTLMSSP_NEGOTIATE_SIGN;
		negotiateFlags |= NTLMSSP_REQUEST_TARGET;
		negotiateFlags |= NTLMSSP_NEGOTIATE_OEM;
		negotiateFlags |= NTLMSSP_NEGOTIATE_UNICODE;
	}
	else
	{
		negotiateFlags |= NTLMSSP_NEGOTIATE_KEY_EXCH;
		negotiateFlags |= NTLMSSP_NEGOTIATE_128;
		negotiateFlags |= NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY;
		negotiateFlags |= NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
		negotiateFlags |= NTLMSSP_NEGOTIATE_NTLM;
		negotiateFlags |= NTLMSSP_NEGOTIATE_SEAL;
		negotiateFlags |= NTLMSSP_NEGOTIATE_SIGN;
		negotiateFlags |= NTLMSSP_REQUEST_TARGET;
		negotiateFlags |= NTLMSSP_NEGOTIATE_UNICODE;
	}

	ntlmssp_output_negotiate_flags(s, negotiateFlags); /* NegotiateFlags (4 bytes) */

#ifdef WITH_DEBUG_NLA
	ntlmssp_print_negotiate_flags(negotiateFlags);
#endif

	/* only set if NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED is set */

	/* DomainNameFields (8 bytes) */
	stream_write_uint16(s, 0); /* DomainNameLen */
	stream_write_uint16(s, 0); /* DomainNameMaxLen */
	stream_write_uint32(s, 0); /* DomainNameBufferOffset */

	/* only set if NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED is set */

	/* WorkstationFields (8 bytes) */
	stream_write_uint16(s, 0); /* WorkstationLen */
	stream_write_uint16(s, 0); /* WorkstationMaxLen */
	stream_write_uint32(s, 0); /* WorkstationBufferOffset */

	if (negotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		/* Only present if NTLMSSP_NEGOTIATE_VERSION is set */
		ntlmssp_output_version(s);

#ifdef WITH_DEBUG_NLA
		printf("Version (length = 8)\n");
		freerdp_hexdump((s->p - 8), 8);
		printf("\n");
#endif
	}

	length = s->p - s->data;
	freerdp_blob_alloc(&ntlmssp->negotiate_message, length);
	memcpy(ntlmssp->negotiate_message.data, s->data, length);

#ifdef WITH_DEBUG_NLA
	printf("NEGOTIATE_MESSAGE (length = %d)\n", length);
	freerdp_hexdump(s->data, length);
	printf("\n");
#endif

	ntlmssp->state = NTLMSSP_STATE_CHALLENGE;
}

/**
 * Receive NTLMSSP CHALLENGE_MESSAGE.\n
 * CHALLENGE_MESSAGE @msdn{cc236642}
 * @param ntlmssp
 * @param s
 */

void ntlmssp_recv_challenge_message(NTLMSSP* ntlmssp, STREAM* s)
{
	uint8* p;
	int length;
	uint8* start_offset;
	uint8* payload_offset;
	uint16 targetNameLen;
	uint16 targetNameMaxLen;
	uint32 targetNameBufferOffset;
	uint16 targetInfoLen;
	uint16 targetInfoMaxLen;
	uint32 targetInfoBufferOffset;

	start_offset = s->p - 12;

	/* TargetNameFields (8 bytes) */
	stream_read_uint16(s, targetNameLen); /* TargetNameLen (2 bytes) */
	stream_read_uint16(s, targetNameMaxLen); /* TargetNameMaxLen (2 bytes) */
	stream_read_uint32(s, targetNameBufferOffset); /* TargetNameBufferOffset (4 bytes) */

	ntlmssp_input_negotiate_flags(s, &(ntlmssp->negotiate_flags)); /* NegotiateFlags (4 bytes) */

#ifdef WITH_DEBUG_NLA
	ntlmssp_print_negotiate_flags(ntlmssp->negotiate_flags);
#endif

	stream_read(s, ntlmssp->server_challenge, 8); /* ServerChallenge (8 bytes) */
	stream_seek(s, 8); /* Reserved (8 bytes), should be ignored */

	/* TargetInfoFields (8 bytes) */
	stream_read_uint16(s, targetInfoLen); /* TargetInfoLen (2 bytes) */
	stream_read_uint16(s, targetInfoMaxLen); /* TargetInfoMaxLen (2 bytes) */
	stream_read_uint32(s, targetInfoBufferOffset); /* TargetInfoBufferOffset (4 bytes) */

	/* only present if NTLMSSP_NEGOTIATE_VERSION is set */

	if (ntlmssp->negotiate_flags & NTLMSSP_NEGOTIATE_VERSION)
	{
		stream_seek(s, 8); /* Version (8 bytes), can be ignored */
	}

	/* Payload (variable) */
	payload_offset = s->p;

	if (targetNameLen > 0)
	{
		p = start_offset + targetNameBufferOffset;
		freerdp_blob_alloc(&ntlmssp->target_name, targetNameLen);
		memcpy(ntlmssp->target_name.data, p, targetNameLen);

#ifdef WITH_DEBUG_NLA
		printf("targetName (length = %d, offset = %d)\n", targetNameLen, targetNameBufferOffset);
		freerdp_hexdump(ntlmssp->target_name.data, ntlmssp->target_name.length);
		printf("\n");
#endif
	}

	if (targetInfoLen > 0)
	{
		p = start_offset + targetInfoBufferOffset;
		freerdp_blob_alloc(&ntlmssp->target_info, targetInfoLen);
		memcpy(ntlmssp->target_info.data, p, targetInfoLen);

#ifdef WITH_DEBUG_NLA
		printf("targetInfo (length = %d, offset = %d)\n", targetInfoLen, targetInfoBufferOffset);
		freerdp_hexdump(ntlmssp->target_info.data, ntlmssp->target_info.length);
		printf("\n");
#endif

		if (ntlmssp->ntlm_v2)
		{
			s->p = p;
			ntlmssp_input_av_pairs(ntlmssp, s);
		}
	}

	length = (payload_offset - start_offset) + targetNameLen + targetInfoLen;

	freerdp_blob_alloc(&ntlmssp->challenge_message, length);
	memcpy(ntlmssp->challenge_message.data, start_offset, length);

#ifdef WITH_DEBUG_NLA
	printf("CHALLENGE_MESSAGE (length = %d)\n", length);
	freerdp_hexdump(start_offset, length);
	printf("\n");
#endif

	/* AV_PAIRs */
	if (ntlmssp->ntlm_v2)
		ntlmssp_populate_av_pairs(ntlmssp);

	/* Timestamp */
	ntlmssp_generate_timestamp(ntlmssp);

	/* LmChallengeResponse */
	ntlmssp_compute_lm_v2_response(ntlmssp);

	if (ntlmssp->ntlm_v2)
		memset(ntlmssp->lm_challenge_response.data, '\0', 24);

	/* NtChallengeResponse */
	ntlmssp_compute_ntlm_v2_response(ntlmssp);

	/* KeyExchangeKey */
	ntlmssp_generate_key_exchange_key(ntlmssp);

	/* EncryptedRandomSessionKey */
	ntlmssp_encrypt_random_session_key(ntlmssp);

	/* Generate signing keys */
	ntlmssp_generate_client_signing_key(ntlmssp);
	ntlmssp_generate_server_signing_key(ntlmssp);

	/* Generate sealing keys */
	ntlmssp_generate_client_sealing_key(ntlmssp);
	ntlmssp_generate_server_sealing_key(ntlmssp);

	/* Initialize RC4 seal state using client sealing key */
	ntlmssp_init_rc4_seal_states(ntlmssp);

#ifdef WITH_DEBUG_NLA
	printf("ClientChallenge\n");
	freerdp_hexdump(ntlmssp->client_challenge, 8);
	printf("\n");

	printf("ServerChallenge\n");
	freerdp_hexdump(ntlmssp->server_challenge, 8);
	printf("\n");

	printf("SessionBaseKey\n");
	freerdp_hexdump(ntlmssp->session_base_key, 16);
	printf("\n");

	printf("KeyExchangeKey\n");
	freerdp_hexdump(ntlmssp->key_exchange_key, 16);
	printf("\n");

	printf("ExportedSessionKey\n");
	freerdp_hexdump(ntlmssp->exported_session_key, 16);
	printf("\n");

	printf("RandomSessionKey\n");
	freerdp_hexdump(ntlmssp->random_session_key, 16);
	printf("\n");

	printf("ClientSignKey\n");
	freerdp_hexdump(ntlmssp->client_signing_key, 16);
	printf("\n");

	printf("ClientSealingKey\n");
	freerdp_hexdump(ntlmssp->client_sealing_key, 16);
	printf("\n");

	printf("Timestamp\n");
	freerdp_hexdump(ntlmssp->timestamp, 8);
	printf("\n");
#endif

	ntlmssp->state = NTLMSSP_STATE_AUTHENTICATE;
}

/**
 * Send NTLMSSP AUTHENTICATE_MESSAGE.\n
 * AUTHENTICATE_MESSAGE @msdn{cc236643}
 * @param ntlmssp
 * @param s
 */

void ntlmssp_send_authenticate_message(NTLMSSP* ntlmssp, STREAM* s)
{
	int length;
	uint32 negotiateFlags = 0;
	uint8* mic_offset = NULL;

	uint16 DomainNameLen;
	uint16 UserNameLen;
	uint16 WorkstationLen;
	uint16 LmChallengeResponseLen;
	uint16 NtChallengeResponseLen;
	uint16 EncryptedRandomSessionKeyLen;

	uint32 PayloadBufferOffset;
	uint32 DomainNameBufferOffset;
	uint32 UserNameBufferOffset;
	uint32 WorkstationBufferOffset;
	uint32 LmChallengeResponseBufferOffset;
	uint32 NtChallengeResponseBufferOffset;
	uint32 EncryptedRandomSessionKeyBufferOffset;

	uint8* UserNameBuffer;
	uint8* DomainNameBuffer;
	uint8* WorkstationBuffer;
	uint8* EncryptedRandomSessionKeyBuffer;

	WorkstationLen = ntlmssp->workstation.length;
	WorkstationBuffer = ntlmssp->workstation.data;

	if (ntlmssp->ntlm_v2 < 1)
		WorkstationLen = 0;

	DomainNameLen = ntlmssp->domain.length;
	DomainNameBuffer = ntlmssp->domain.data;

	UserNameLen = ntlmssp->username.length;
	UserNameBuffer = ntlmssp->username.data;

	LmChallengeResponseLen = ntlmssp->lm_challenge_response.length;
	NtChallengeResponseLen = ntlmssp->nt_challenge_response.length;

	EncryptedRandomSessionKeyLen = 16;
	EncryptedRandomSessionKeyBuffer = ntlmssp->encrypted_random_session_key;

	if (ntlmssp->ntlm_v2)
	{
		/* observed: 35 82 88 e2 (0xE2888235) */
		negotiateFlags |= NTLMSSP_NEGOTIATE_56;
		negotiateFlags |= NTLMSSP_NEGOTIATE_KEY_EXCH;
		negotiateFlags |= NTLMSSP_NEGOTIATE_128;
		negotiateFlags |= NTLMSSP_NEGOTIATE_VERSION;
		negotiateFlags |= NTLMSSP_NEGOTIATE_TARGET_INFO;
		negotiateFlags |= NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY;
		negotiateFlags |= NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
		negotiateFlags |= NTLMSSP_NEGOTIATE_NTLM;
		negotiateFlags |= NTLMSSP_NEGOTIATE_SEAL;
		negotiateFlags |= NTLMSSP_NEGOTIATE_SIGN;
		negotiateFlags |= NTLMSSP_REQUEST_TARGET;
		negotiateFlags |= NTLMSSP_NEGOTIATE_UNICODE;
	}
	else
	{
		negotiateFlags |= NTLMSSP_NEGOTIATE_KEY_EXCH;
		negotiateFlags |= NTLMSSP_NEGOTIATE_128;
		negotiateFlags |= NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY;
		negotiateFlags |= NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
		negotiateFlags |= NTLMSSP_NEGOTIATE_NTLM;
		negotiateFlags |= NTLMSSP_NEGOTIATE_SEAL;
		negotiateFlags |= NTLMSSP_NEGOTIATE_SIGN;
		negotiateFlags |= NTLMSSP_REQUEST_TARGET;
		negotiateFlags |= NTLMSSP_NEGOTIATE_UNICODE;
	}

	if (ntlmssp->ntlm_v2)
		PayloadBufferOffset = 80; /* starting buffer offset */
	else
		PayloadBufferOffset = 64; /* starting buffer offset */

	if (negotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		PayloadBufferOffset += 8;

	DomainNameBufferOffset = PayloadBufferOffset;
	UserNameBufferOffset = DomainNameBufferOffset + DomainNameLen;
	WorkstationBufferOffset = UserNameBufferOffset + UserNameLen;
	LmChallengeResponseBufferOffset = WorkstationBufferOffset + WorkstationLen;
	NtChallengeResponseBufferOffset = LmChallengeResponseBufferOffset + LmChallengeResponseLen;
	EncryptedRandomSessionKeyBufferOffset = NtChallengeResponseBufferOffset + NtChallengeResponseLen;

	stream_write(s, ntlm_signature, 8); /* Signature (8 bytes) */
	stream_write_uint32(s, 3); /* MessageType */

	/* LmChallengeResponseFields (8 bytes) */
	stream_write_uint16(s, LmChallengeResponseLen); /* LmChallengeResponseLen */
	stream_write_uint16(s, LmChallengeResponseLen); /* LmChallengeResponseMaxLen */
	stream_write_uint32(s, LmChallengeResponseBufferOffset); /* LmChallengeResponseBufferOffset */

	/* NtChallengeResponseFields (8 bytes) */
	stream_write_uint16(s, NtChallengeResponseLen); /* NtChallengeResponseLen */
	stream_write_uint16(s, NtChallengeResponseLen); /* NtChallengeResponseMaxLen */
	stream_write_uint32(s, NtChallengeResponseBufferOffset); /* NtChallengeResponseBufferOffset */

	/* only set if NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED is set */

	/* DomainNameFields (8 bytes) */
	stream_write_uint16(s, DomainNameLen); /* DomainNameLen */
	stream_write_uint16(s, DomainNameLen); /* DomainNameMaxLen */
	stream_write_uint32(s, DomainNameBufferOffset); /* DomainNameBufferOffset */

	/* UserNameFields (8 bytes) */
	stream_write_uint16(s, UserNameLen); /* UserNameLen */
	stream_write_uint16(s, UserNameLen); /* UserNameMaxLen */
	stream_write_uint32(s, UserNameBufferOffset); /* UserNameBufferOffset */

	/* only set if NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED is set */

	/* WorkstationFields (8 bytes) */
	stream_write_uint16(s, WorkstationLen); /* WorkstationLen */
	stream_write_uint16(s, WorkstationLen); /* WorkstationMaxLen */
	stream_write_uint32(s, WorkstationBufferOffset); /* WorkstationBufferOffset */

	/* EncryptedRandomSessionKeyFields (8 bytes) */
	stream_write_uint16(s, EncryptedRandomSessionKeyLen); /* EncryptedRandomSessionKeyLen */
	stream_write_uint16(s, EncryptedRandomSessionKeyLen); /* EncryptedRandomSessionKeyMaxLen */
	stream_write_uint32(s, EncryptedRandomSessionKeyBufferOffset); /* EncryptedRandomSessionKeyBufferOffset */

	ntlmssp_output_negotiate_flags(s, negotiateFlags); /* NegotiateFlags (4 bytes) */

#ifdef WITH_DEBUG_NLA
	ntlmssp_print_negotiate_flags(negotiateFlags);
#endif
	
	if (negotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		/* Only present if NTLMSSP_NEGOTIATE_VERSION is set */
		ntlmssp_output_version(s);

#ifdef WITH_DEBUG_NLA
		printf("Version (length = 8)\n");
		freerdp_hexdump((s->p - 8), 8);
		printf("\n");
#endif
	}

	if (ntlmssp->ntlm_v2)
	{
		/* Message Integrity Check */
		mic_offset = s->p;
		stream_write_zero(s, 16);
	}

	/* DomainName */
	if (DomainNameLen > 0)
	{
		stream_write(s, DomainNameBuffer, DomainNameLen);
#ifdef WITH_DEBUG_NLA
		printf("DomainName (length = %d, offset = %d)\n", DomainNameLen, DomainNameBufferOffset);
		freerdp_hexdump(DomainNameBuffer, DomainNameLen);
		printf("\n");
#endif
	}

	/* UserName */
	stream_write(s, UserNameBuffer, UserNameLen);

#ifdef WITH_DEBUG_NLA
	printf("UserName (length = %d, offset = %d)\n", UserNameLen, UserNameBufferOffset);
	freerdp_hexdump(UserNameBuffer, UserNameLen);
	printf("\n");
#endif

	/* Workstation */
	if (WorkstationLen > 0)
	{
		stream_write(s, WorkstationBuffer, WorkstationLen);
#ifdef WITH_DEBUG_NLA
		printf("Workstation (length = %d, offset = %d)\n", WorkstationLen, WorkstationBufferOffset);
		freerdp_hexdump(WorkstationBuffer, WorkstationLen);
		printf("\n");
#endif
	}

	/* LmChallengeResponse */
	stream_write(s, ntlmssp->lm_challenge_response.data, LmChallengeResponseLen);

#ifdef WITH_DEBUG_NLA
	printf("LmChallengeResponse (length = %d, offset = %d)\n", LmChallengeResponseLen, LmChallengeResponseBufferOffset);
	freerdp_hexdump(ntlmssp->lm_challenge_response.data, LmChallengeResponseLen);
	printf("\n");
#endif

	/* NtChallengeResponse */
	stream_write(s, ntlmssp->nt_challenge_response.data, NtChallengeResponseLen);

#ifdef WITH_DEBUG_NLA
	if (ntlmssp->ntlm_v2)
	{
		ntlmssp_print_av_pairs(ntlmssp);

		printf("targetInfo (length = %d)\n", ntlmssp->target_info.length);
		freerdp_hexdump(ntlmssp->target_info.data, ntlmssp->target_info.length);
		printf("\n");
	}
#endif

#ifdef WITH_DEBUG_NLA
	printf("NtChallengeResponse (length = %d, offset = %d)\n", NtChallengeResponseLen, NtChallengeResponseBufferOffset);
	freerdp_hexdump(ntlmssp->nt_challenge_response.data, NtChallengeResponseLen);
	printf("\n");
#endif

	/* EncryptedRandomSessionKey */
	stream_write(s, EncryptedRandomSessionKeyBuffer, EncryptedRandomSessionKeyLen);

#ifdef WITH_DEBUG_NLA
	printf("EncryptedRandomSessionKey (length = %d, offset = %d)\n", EncryptedRandomSessionKeyLen, EncryptedRandomSessionKeyBufferOffset);
	freerdp_hexdump(EncryptedRandomSessionKeyBuffer, EncryptedRandomSessionKeyLen);
	printf("\n");
#endif

	length = s->p - s->data;
	freerdp_blob_alloc(&ntlmssp->authenticate_message, length);
	memcpy(ntlmssp->authenticate_message.data, s->data, length);

	if (ntlmssp->ntlm_v2)
	{
		/* Message Integrity Check */
		ntlmssp_compute_message_integrity_check(ntlmssp);
		
		s->p = mic_offset;
		stream_write(s, ntlmssp->message_integrity_check, 16);
		s->p = s->data + length;

#ifdef WITH_DEBUG_NLA
		printf("MessageIntegrityCheck (length = 16)\n");
		freerdp_hexdump(mic_offset, 16);
		printf("\n");
#endif
	}

#ifdef WITH_DEBUG_NLA
	printf("AUTHENTICATE_MESSAGE (length = %d)\n", length);
	freerdp_hexdump(s->data, length);
	printf("\n");
#endif

	ntlmssp->state = NTLMSSP_STATE_FINAL;
}

/**
 * Send NTLMSSP message.
 * @param ntlmssp
 * @param s
 * @return
 */

int ntlmssp_send(NTLMSSP* ntlmssp, STREAM* s)
{
	if (ntlmssp->state == NTLMSSP_STATE_INITIAL)
		ntlmssp->state = NTLMSSP_STATE_NEGOTIATE;

	if (ntlmssp->state == NTLMSSP_STATE_NEGOTIATE)
		ntlmssp_send_negotiate_message(ntlmssp, s);
	else if (ntlmssp->state == NTLMSSP_STATE_AUTHENTICATE)
		ntlmssp_send_authenticate_message(ntlmssp, s);

	return (ntlmssp->state == NTLMSSP_STATE_FINAL) ? 0 : 1;
}

/**
 * Receive NTLMSSP message.
 * @param ntlmssp
 * @param s
 * @return
 */

int ntlmssp_recv(NTLMSSP* ntlmssp, STREAM* s)
{
	char signature[8]; /* Signature, "NTLMSSP" */
	uint32 messageType; /* MessageType */

	stream_read(s, signature, 8);
	stream_read_uint32(s, messageType);

	if (messageType == 2 && ntlmssp->state == NTLMSSP_STATE_CHALLENGE)
		ntlmssp_recv_challenge_message(ntlmssp, s);

	return 1;
}

/**
 * Create new NTLMSSP state machine instance.
 * @return
 */

NTLMSSP* ntlmssp_new()
{
	NTLMSSP* ntlmssp = (NTLMSSP*) xmalloc(sizeof(NTLMSSP));

	if (ntlmssp != NULL)
	{
		memset(ntlmssp, '\0', sizeof(NTLMSSP));
		ntlmssp->av_pairs = (AV_PAIRS*) xmalloc(sizeof(AV_PAIRS));
		memset(ntlmssp->av_pairs, 0, sizeof(AV_PAIRS));
		ntlmssp_init(ntlmssp);
	}

	return ntlmssp;
}

/**
 * Initialize NTLMSSP state machine.
 * @param ntlmssp
 */

void ntlmssp_init(NTLMSSP* ntlmssp)
{
	ntlmssp->state = NTLMSSP_STATE_INITIAL;
	ntlmssp->uniconv = freerdp_uniconv_new();
}

/**
 * Finalize NTLMSSP state machine.
 * @param ntlmssp
 */

void ntlmssp_uninit(NTLMSSP* ntlmssp)
{
	freerdp_blob_free(&ntlmssp->username);
	freerdp_blob_free(&ntlmssp->password);
	freerdp_blob_free(&ntlmssp->domain);

	freerdp_blob_free(&ntlmssp->spn);
	freerdp_blob_free(&ntlmssp->workstation);
	freerdp_blob_free(&ntlmssp->target_info);
	freerdp_blob_free(&ntlmssp->target_name);

	freerdp_blob_free(&ntlmssp->negotiate_message);
	freerdp_blob_free(&ntlmssp->challenge_message);
	freerdp_blob_free(&ntlmssp->authenticate_message);

	freerdp_blob_free(&ntlmssp->lm_challenge_response);
	freerdp_blob_free(&ntlmssp->nt_challenge_response);

	ntlmssp_free_av_pairs(ntlmssp);
	freerdp_uniconv_free(ntlmssp->uniconv);

	ntlmssp->state = NTLMSSP_STATE_FINAL;
}

/**
 * Free NTLMSSP state machine.
 * @param ntlmssp
 */

void ntlmssp_free(NTLMSSP* ntlmssp)
{
	ntlmssp_uninit(ntlmssp);

	if (ntlmssp->send_rc4_seal)
		crypto_rc4_free(ntlmssp->send_rc4_seal);
	if (ntlmssp->recv_rc4_seal)
		crypto_rc4_free(ntlmssp->recv_rc4_seal);
	xfree(ntlmssp);
}

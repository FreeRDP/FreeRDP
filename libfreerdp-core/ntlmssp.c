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

#define NTLMSSP_INDEX_NEGOTIATE_56				0
#define NTLMSSP_INDEX_NEGOTIATE_KEY_EXCH			1
#define NTLMSSP_INDEX_NEGOTIATE_128				2
#define NTLMSSP_INDEX_NEGOTIATE_VERSION				6
#define NTLMSSP_INDEX_NEGOTIATE_TARGET_INFO			8
#define NTLMSSP_INDEX_REQUEST_NON_NT_SESSION_KEY		9
#define NTLMSSP_INDEX_NEGOTIATE_IDENTIFY			11
#define NTLMSSP_INDEX_NEGOTIATE_EXTENDED_SESSION_SECURITY	12
#define NTLMSSP_INDEX_TARGET_TYPE_SERVER			14
#define NTLMSSP_INDEX_TARGET_TYPE_DOMAIN			15
#define NTLMSSP_INDEX_NEGOTIATE_ALWAYS_SIGN			16
#define NTLMSSP_INDEX_NEGOTIATE_WORKSTATION_SUPPLIED		18
#define NTLMSSP_INDEX_NEGOTIATE_DOMAIN_SUPPLIED			19
#define NTLMSSP_INDEX_NEGOTIATE_NTLM				22
#define NTLMSSP_INDEX_NEGOTIATE_LM_KEY				24
#define NTLMSSP_INDEX_NEGOTIATE_DATAGRAM			25
#define NTLMSSP_INDEX_NEGOTIATE_SEAL				26
#define NTLMSSP_INDEX_NEGOTIATE_SIGN				27
#define NTLMSSP_INDEX_REQUEST_TARGET				29
#define NTLMSSP_INDEX_NEGOTIATE_OEM				30
#define NTLMSSP_INDEX_NEGOTIATE_UNICODE				31

#define NTLMSSP_NEGOTIATE_56					(1 << NTLMSSP_INDEX_NEGOTIATE_56)
#define NTLMSSP_NEGOTIATE_KEY_EXCH				(1 << NTLMSSP_INDEX_NEGOTIATE_KEY_EXCH)
#define NTLMSSP_NEGOTIATE_128					(1 << NTLMSSP_INDEX_NEGOTIATE_128)
#define NTLMSSP_NEGOTIATE_VERSION				(1 << NTLMSSP_INDEX_NEGOTIATE_VERSION)
#define NTLMSSP_NEGOTIATE_TARGET_INFO				(1 << NTLMSSP_INDEX_NEGOTIATE_TARGET_INFO)
#define NTLMSSP_REQUEST_NON_NT_SESSION_KEY			(1 << NTLMSSP_INDEX_REQUEST_NON_NT_SESSION_KEY)
#define NTLMSSP_NEGOTIATE_IDENTIFY				(1 << NTLMSSP_INDEX_NEGOTIATE_IDENTIFY)
#define NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY		(1 << NTLMSSP_INDEX_NEGOTIATE_EXTENDED_SESSION_SECURITY)
#define NTLMSSP_TARGET_TYPE_SERVER				(1 << NTLMSSP_INDEX_TARGET_TYPE_SERVER)
#define NTLMSSP_TARGET_TYPE_DOMAIN				(1 << NTLMSSP_INDEX_TARGET_TYPE_DOMAIN)
#define NTLMSSP_NEGOTIATE_ALWAYS_SIGN				(1 << NTLMSSP_INDEX_NEGOTIATE_ALWAYS_SIGN)
#define NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED			(1 << NTLMSSP_INDEX_NEGOTIATE_WORKSTATION_SUPPLIED)
#define NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED			(1 << NTLMSSP_INDEX_NEGOTIATE_DOMAIN_SUPPLIED)
#define NTLMSSP_NEGOTIATE_NTLM					(1 << NTLMSSP_INDEX_NEGOTIATE_NTLM)
#define NTLMSSP_NEGOTIATE_LM_KEY				(1 << NTLMSSP_INDEX_NEGOTIATE_LM_KEY)
#define NTLMSSP_NEGOTIATE_DATAGRAM				(1 << NTLMSSP_INDEX_NEGOTIATE_DATAGRAM)
#define NTLMSSP_NEGOTIATE_SEAL					(1 << NTLMSSP_INDEX_NEGOTIATE_SEAL)
#define NTLMSSP_NEGOTIATE_SIGN					(1 << NTLMSSP_INDEX_NEGOTIATE_SIGN)
#define NTLMSSP_REQUEST_TARGET					(1 << NTLMSSP_INDEX_REQUEST_TARGET)
#define NTLMSSP_NEGOTIATE_OEM					(1 << NTLMSSP_INDEX_NEGOTIATE_OEM)
#define NTLMSSP_NEGOTIATE_UNICODE				(1 << NTLMSSP_INDEX_NEGOTIATE_UNICODE)

#define WINDOWS_MAJOR_VERSION_5		0x05
#define WINDOWS_MAJOR_VERSION_6		0x06
#define WINDOWS_MINOR_VERSION_0		0x00
#define WINDOWS_MINOR_VERSION_1		0x01
#define WINDOWS_MINOR_VERSION_2		0x02
#define NTLMSSP_REVISION_W2K3		0x0F

const char ntlm_signature[] = "NTLMSSP";
const char lm_magic[] = "KGS!@#$%";

const char client_sign_magic[] = "session key to client-to-server signing key magic constant";
const char server_sign_magic[] = "session key to server-to-client signing key magic constant";
const char client_seal_magic[] = "session key to client-to-server sealing key magic constant";
const char server_seal_magic[] = "session key to server-to-client sealing key magic constant";

/**
 * Set NTLMSSP username.
 * @param ntlmssp
 * @param username username
 */

void ntlmssp_set_username(NTLMSSP *ntlmssp, char* username)
{
	datablob_free(&ntlmssp->username);

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

void ntlmssp_set_domain(NTLMSSP *ntlmssp, char* domain)
{
	datablob_free(&ntlmssp->domain);

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

void ntlmssp_set_password(NTLMSSP *ntlmssp, char* password)
{
	datablob_free(&ntlmssp->password);

	if (password != NULL)
	{
		ntlmssp->password.data = freerdp_uniconv_out(ntlmssp->uniconv, password, (size_t*) &(ntlmssp->password.length));
	}
}

/**
 * Generate client challenge (8-byte nonce).
 * @param ntlmssp
 */

void ntlmssp_generate_client_challenge(NTLMSSP *ntlmssp)
{
	/* ClientChallenge in computation of LMv2 and NTLMv2 responses */
	crypto_nonce(ntlmssp->client_challenge, 8);
}

/**
 * Generate KeyExchangeKey (the 128-bit SessionBaseKey).\n
 * @msdn{cc236710}
 * @param ntlmssp
 */

void ntlmssp_generate_key_exchange_key(NTLMSSP *ntlmssp)
{
	/* In NTLMv2, KeyExchangeKey is the 128-bit SessionBaseKey */
	memcpy(ntlmssp->key_exchange_key, ntlmssp->session_base_key, 16);
}

/**
 * Generate RandomSessionKey (16-byte nonce).
 * @param ntlmssp
 */

void ntlmssp_generate_random_session_key(NTLMSSP *ntlmssp)
{
	crypto_nonce(ntlmssp->random_session_key, 16);
}

/**
 * Generate ExportedSessionKey (the RandomSessionKey, exported)
 * @param ntlmssp
 */

void ntlmssp_generate_exported_session_key(NTLMSSP *ntlmssp)
{
	memcpy(ntlmssp->exported_session_key, ntlmssp->random_session_key, 16);
}

/**
 * Encrypt RandomSessionKey (RC4-encrypted RandomSessionKey, using KeyExchangeKey as the key).
 * @param ntlmssp
 */

void ntlmssp_encrypt_random_session_key(NTLMSSP *ntlmssp)
{
	/* In NTLMv2, EncryptedRandomSessionKey is the ExportedSessionKey RC4-encrypted with the KeyExchangeKey */
	credssp_rc4k(ntlmssp->key_exchange_key, 16, ntlmssp->random_session_key, ntlmssp->encrypted_random_session_key);
}

/**
 * Generate timestamp for AUTHENTICATE_MESSAGE.
 * @param ntlmssp
 */

void ntlmssp_generate_timestamp(NTLMSSP *ntlmssp)
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
}

/**
 * Generate signing key.\n
 * @msdn{cc236711}
 * @param exported_session_key ExportedSessionKey
 * @param sign_magic Sign magic string
 * @param signing_key Destination signing key
 */

void ntlmssp_generate_signing_key(uint8* exported_session_key, DATABLOB *sign_magic, uint8* signing_key)
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

void ntlmssp_generate_client_signing_key(NTLMSSP *ntlmssp)
{
	DATABLOB sign_magic;
	sign_magic.data = (void*) client_sign_magic;
	sign_magic.length = sizeof(client_sign_magic);
	ntlmssp_generate_signing_key(ntlmssp->exported_session_key, &sign_magic, ntlmssp->client_signing_key);
}

/**
 * Generate server signing key (ServerSigningKey).\n
 * @msdn{cc236711}
 * @param ntlmssp
 */

void ntlmssp_generate_server_signing_key(NTLMSSP *ntlmssp)
{
	DATABLOB sign_magic;
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

void ntlmssp_generate_sealing_key(uint8* exported_session_key, DATABLOB *seal_magic, uint8* sealing_key)
{
	uint8* p;
	CryptoMd5 md5;
	DATABLOB blob;

	datablob_alloc(&blob, 16 + seal_magic->length);
	p = (uint8*) blob.data;

	/* Concatenate ExportedSessionKey with seal magic */
	memcpy(p, exported_session_key, 16);
	memcpy(&p[16], seal_magic->data, seal_magic->length);

	md5 = crypto_md5_init();
	crypto_md5_update(md5, blob.data, blob.length);
	crypto_md5_final(md5, sealing_key);

	datablob_free(&blob);
}

/**
 * Generate client sealing key (ClientSealingKey).\n
 * @msdn{cc236712}
 * @param ntlmssp
 */

void ntlmssp_generate_client_sealing_key(NTLMSSP *ntlmssp)
{
	DATABLOB seal_magic;
	seal_magic.data = (void*) client_seal_magic;
	seal_magic.length = sizeof(client_seal_magic);
	ntlmssp_generate_signing_key(ntlmssp->exported_session_key, &seal_magic, ntlmssp->client_sealing_key);
}

/**
 * Generate server sealing key (ServerSealingKey).\n
 * @msdn{cc236712}
 * @param ntlmssp
 */

void ntlmssp_generate_server_sealing_key(NTLMSSP *ntlmssp)
{
	DATABLOB seal_magic;
	seal_magic.data = (void*) server_seal_magic;
	seal_magic.length = sizeof(server_seal_magic);
	ntlmssp_generate_signing_key(ntlmssp->exported_session_key, &seal_magic, ntlmssp->server_sealing_key);
}

/**
 * Initialize RC4 stream cipher states for sealing.
 * @param ntlmssp
 */

void ntlmssp_init_rc4_seal_states(NTLMSSP *ntlmssp)
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

void ntlmssp_compute_ntlm_hash(DATABLOB* password, char* hash)
{
	/* NTLMv1("password") = 8846F7EAEE8FB117AD06BDD830B7586C */

	MD4_CTX md4_ctx;

	/* Password needs to be in unicode */

	/* Apply the MD4 digest algorithm on the password in unicode, the result is the NTLM hash */

	MD4_Init(&md4_ctx);
	MD4_Update(&md4_ctx, password->data, password->length);
	MD4_Final((void*) hash, &md4_ctx);
}

void ntlmssp_compute_ntlm_v2_hash(NTLMSSP *ntlmssp, char* hash)
{
	char* p;
	DATABLOB blob;
	char ntlm_hash[16];

	datablob_alloc(&blob, ntlmssp->username.length + ntlmssp->domain.length);
	p = (char*) blob.data;

	/* First, compute the NTLMv1 hash of the password */
	ntlmssp_compute_ntlm_hash(&ntlmssp->password, ntlm_hash);

	/* Concatenate(Uppercase(username),domain)*/
	memcpy(p, ntlmssp->username.data, ntlmssp->username.length);
	freerdp_uniconv_uppercase(ntlmssp->uniconv, p, ntlmssp->username.length / 2);

	memcpy(&p[ntlmssp->username.length], ntlmssp->domain.data, ntlmssp->domain.length);

	/* Compute the HMAC-MD5 hash of the above value using the NTLMv1 hash as the key, the result is the NTLMv2 hash */
	HMAC(EVP_md5(), (void*) ntlm_hash, 16, blob.data, blob.length, (void*) hash, NULL);

	datablob_free(&blob);
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

void ntlmssp_compute_lm_v2_response(NTLMSSP *ntlmssp)
{
	char *response;
	char value[16];
	char ntlm_v2_hash[16];

	/* Compute the NTLMv2 hash */
	ntlmssp_compute_ntlm_v2_hash(ntlmssp, ntlm_v2_hash);

	/* Concatenate the server and client challenges */
	memcpy(value, ntlmssp->server_challenge, 8);
	memcpy(&value[8], ntlmssp->client_challenge, 8);

	datablob_alloc(&ntlmssp->lm_challenge_response, 24);
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

void ntlmssp_compute_ntlm_v2_response(NTLMSSP *ntlmssp)
{
	uint8* blob;
	uint8 ntlm_v2_hash[16];
	uint8 nt_proof_str[16];
	DATABLOB ntlm_v2_temp;
	DATABLOB ntlm_v2_temp_chal;

	datablob_alloc(&ntlm_v2_temp, ntlmssp->target_info.length + 28);

	memset(ntlm_v2_temp.data, '\0', ntlm_v2_temp.length);
	blob = (uint8*) ntlm_v2_temp.data;

	/* Compute the NTLMv2 hash */
	ntlmssp_compute_ntlm_v2_hash(ntlmssp, (char*) ntlm_v2_hash);

#ifdef WITH_DEBUG_NLA
	printf("Password (length = %d)\n", ntlmssp->password.length);
	hexdump(ntlmssp->password.data, ntlmssp->password.length);
	printf("\n");

	printf("Username (length = %d)\n", ntlmssp->username.length);
	hexdump(ntlmssp->username.data, ntlmssp->username.length);
	printf("\n");

	printf("Domain (length = %d)\n", ntlmssp->domain.length);
	hexdump(ntlmssp->domain.data, ntlmssp->domain.length);
	printf("\n");

	printf("NTOWFv2, NTLMv2 Hash\n");
	hexdump(ntlm_v2_hash, 16);
	printf("\n");
#endif

	/* Construct temp */
	blob[0] = 1; /* RespType (1 byte) */
	blob[1] = 1; /* HighRespType (1 byte) */
	/* Reserved1 (2 bytes) */
	/* Reserved2 (4 bytes) */
	memcpy(&blob[8], ntlmssp->timestamp, 8); /* Timestamp (8 bytes) */
	memcpy(&blob[16], ntlmssp->client_challenge, 8); /* ClientChallenge (8 bytes) */
	/* Reserved3 (4 bytes) */
	memcpy(&blob[28], ntlmssp->target_info.data, ntlmssp->target_info.length);

#ifdef WITH_DEBUG_NLA
	printf("NTLMv2 Response Temp Blob\n");
	hexdump(ntlm_v2_temp.data, ntlm_v2_temp.length);
	printf("\n");
#endif

	/* Concatenate server challenge with temp */
	datablob_alloc(&ntlm_v2_temp_chal, ntlm_v2_temp.length + 8);
	blob = (uint8*) ntlm_v2_temp_chal.data;
	memcpy(blob, ntlmssp->server_challenge, 8);
	memcpy(&blob[8], ntlm_v2_temp.data, ntlm_v2_temp.length);

	HMAC(EVP_md5(), (void*) ntlm_v2_hash, 16, (void*) ntlm_v2_temp_chal.data,
		ntlm_v2_temp_chal.length, (void*) nt_proof_str, NULL);

	/* NtChallengeResponse, Concatenate NTProofStr with temp */
	datablob_alloc(&ntlmssp->nt_challenge_response, ntlm_v2_temp.length + 16);
	blob = (uint8*) ntlmssp->nt_challenge_response.data;
	memcpy(blob, nt_proof_str, 16);
	memcpy(&blob[16], ntlm_v2_temp.data, ntlm_v2_temp.length);

	/* Compute SessionBaseKey, the HMAC-MD5 hash of NTProofStr using the NTLMv2 hash as the key */
	HMAC(EVP_md5(), (void*) ntlm_v2_hash, 16,
		(void*) nt_proof_str, 16, (void*) ntlmssp->session_base_key, NULL);

	datablob_free(&ntlm_v2_temp);
	datablob_free(&ntlm_v2_temp_chal);
}

/**
 * Input NegotiateFlags, a 4-byte bit map.
 * @param s
 * @param flags
 */

void ntlmssp_input_negotiate_flags(STREAM s, uint32 *flags)
{
	uint8* p;
	uint8 tmp;
	uint32 negotiateFlags;

	/*
	 * NegotiateFlags is a 4-byte bit map
	 * Reverse order and then input in Big Endian
	 */

	in_uint32_be(s, negotiateFlags);

	p = (uint8*) &negotiateFlags;
	tmp = p[0];
	p[0] = p[3];
	p[3] = tmp;

	tmp = p[1];
	p[1] = p[2];
	p[2] = tmp;

	*flags = negotiateFlags;
}

/**
 * Output NegotiateFlags, a 4-byte bit map.
 * @param s
 * @param flags
 */

void ntlmssp_output_negotiate_flags(STREAM s, uint32 flags)
{
	uint8* p;
	uint8 tmp;

	/*
	 * NegotiateFlags is a 4-byte bit map
	 * Output in Big Endian and then reverse order
	 */

	p = s->p;
	out_uint32_be(s, flags);

	tmp = p[0];
	p[0] = p[3];
	p[3] = tmp;

	tmp = p[1];
	p[1] = p[2];
	p[2] = tmp;
}

#ifdef WITH_DEBUG_NLA
static void ntlmssp_print_negotiate_flags(uint32 flags)
{
	printf("negotiateFlags \"0x%08X\"{\n", flags);

	if (flags & NTLMSSP_NEGOTIATE_56)
		printf("\tNTLMSSP_NEGOTIATE_56\n");
	if (flags & NTLMSSP_NEGOTIATE_KEY_EXCH)
		printf("\tNTLMSSP_NEGOTIATE_KEY_EXCH\n");
	if (flags & NTLMSSP_NEGOTIATE_128)
		printf("\tNTLMSSP_NEGOTIATE_128\n");
	if (flags & NTLMSSP_NEGOTIATE_VERSION)
		printf("\tNTLMSSP_NEGOTIATE_VERSION\n");
	if (flags & NTLMSSP_NEGOTIATE_TARGET_INFO)
		printf("\tNTLMSSP_NEGOTIATE_TARGET_INFO\n");
	if (flags & NTLMSSP_REQUEST_NON_NT_SESSION_KEY)
		printf("\tNTLMSSP_REQUEST_NON_NT_SESSION_KEY\n");
	if (flags & NTLMSSP_NEGOTIATE_IDENTIFY)
		printf("\tNTLMSSP_NEGOTIATE_IDENTIFY\n");
	if (flags & NTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY)
		printf("\tNTLMSSP_NEGOTIATE_EXTENDED_SESSION_SECURITY\n");
	if (flags & NTLMSSP_TARGET_TYPE_SERVER)
		printf("\tNTLMSSP_TARGET_TYPE_SERVER\n");
	if (flags & NTLMSSP_TARGET_TYPE_DOMAIN)
		printf("\tNTLMSSP_TARGET_TYPE_DOMAIN\n");
	if (flags & NTLMSSP_NEGOTIATE_ALWAYS_SIGN)
		printf("\tNTLMSSP_NEGOTIATE_ALWAYS_SIGN\n");
	if (flags & NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED)
		printf("\tNTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED\n");
	if (flags & NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED)
		printf("\tNTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED\n");
	if (flags & NTLMSSP_NEGOTIATE_NTLM)
		printf("\tNTLMSSP_NEGOTIATE_NTLM\n");
	if (flags & NTLMSSP_NEGOTIATE_LM_KEY)
		printf("\tNTLMSSP_NEGOTIATE_LM_KEY\n");
	if (flags & NTLMSSP_NEGOTIATE_DATAGRAM)
		printf("\tNTLMSSP_NEGOTIATE_DATAGRAM\n");
	if (flags & NTLMSSP_NEGOTIATE_SEAL)
		printf("\tNTLMSSP_NEGOTIATE_SEAL\n");
	if (flags & NTLMSSP_NEGOTIATE_SIGN)
		printf("\tNTLMSSP_NEGOTIATE_SIGN\n");
	if (flags & NTLMSSP_REQUEST_TARGET)
		printf("\tNTLMSSP_REQUEST_TARGET\n");
	if (flags & NTLMSSP_NEGOTIATE_OEM)
		printf("\tNTLMSSP_NEGOTIATE_OEM\n");
	if (flags & NTLMSSP_NEGOTIATE_UNICODE)
		printf("\tNTLMSSP_NEGOTIATE_UNICODE\n");
	printf("}\n");
}
#endif

/**
 * Output Restriction_Encoding.\n
 * Restriction_Encoding @msdn{cc236647}
 * @param ntlmssp
 */

static void ntlmssp_output_restriction_encoding(NTLMSSP *ntlmssp)
{
	AV_PAIR *restrictions = &ntlmssp->av_pairs->Restrictions;
	STREAM s = xmalloc(sizeof(struct stream));

	uint8 machineID[32] =
		"\x3A\x15\x8E\xA6\x75\x82\xD8\xF7\x3E\x06\xFA\x7A\xB4\xDF\xFD\x43"
		"\x84\x6C\x02\x3A\xFD\x5A\x94\xFE\xCF\x97\x0F\x3D\x19\x2C\x38\x20";

	restrictions->value = xmalloc(48);
	restrictions->length = 48;

	s->data = restrictions->value;
	s->size = restrictions->length;
	s->end = s->data + s->size;
	s->p = s->data;

	out_uint32_le(s, 48); /* Size */
	out_uint8s(s, 4); /* Z4 (set to zero) */

	/* IntegrityLevel (bit 31 set to 1) */
	out_uint8(s, 1);
	out_uint8s(s, 3);

	out_uint32_le(s, 0x20000000); /* SubjectIntegrityLevel */
	out_uint8p(s, machineID, 32); /* MachineID */

	xfree(s);
}

/**
 * Populate array of AV_PAIRs.\n
 * AV_PAIR @msdn{cc236646}
 * @param ntlmssp
 */

void ntlmssp_populate_av_pairs(NTLMSSP *ntlmssp)
{
	STREAM s;
	DATABLOB target_info;
	AV_PAIRS *av_pairs = ntlmssp->av_pairs;

	/* MsvAvFlags */
	av_pairs->Flags = 0x00000002; /* Indicates the present of a Message Integrity Check (MIC) */

	/* Restriction_Encoding */
	ntlmssp_output_restriction_encoding(ntlmssp);

	s = xmalloc(sizeof(struct stream));
	s->data = xmalloc(ntlmssp->target_info.length + 512);
	s->p = s->data;

	ntlmssp_output_av_pairs(ntlmssp, s);
	datablob_alloc(&target_info, s->end - s->data);
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

void ntlmssp_input_av_pairs(NTLMSSP *ntlmssp, STREAM s)
{
	AV_ID AvId;
	uint16 AvLen;
	uint8* value;
	AV_PAIRS* av_pairs = ntlmssp->av_pairs;

	do
	{
		value = NULL;
		in_uint16_le(s, AvId);
		in_uint16_le(s, AvLen);

		if (AvLen > 0)
		{
			if (AvId != MsvAvFlags)
			{
				value = xmalloc(AvLen);
				in_uint8a(s, value, AvLen);
			}
			else
			{
				in_uint32_le(s, av_pairs->Flags);
			}
		}

		switch (AvId)
		{
			case MsvAvNbComputerName:
				//printf("AvId: MsvAvNbComputerName, AvLen: %d\n", AvLen);
				av_pairs->NbComputerName.length = AvLen;
				av_pairs->NbComputerName.value = value;
				break;

			case MsvAvNbDomainName:
				//printf("AvId: MsvAvNbDomainName, AvLen: %d\n", AvLen);
				av_pairs->NbDomainName.length = AvLen;
				av_pairs->NbDomainName.value = value;
				break;

			case MsvAvDnsComputerName:
				//printf("AvId: MsvAvDnsComputerName, AvLen: %d\n", AvLen);
				av_pairs->DnsComputerName.length = AvLen;
				av_pairs->DnsComputerName.value = value;
				break;

			case MsvAvDnsDomainName:
				//printf("AvId: MsvAvDnsDomainName, AvLen: %d\n", AvLen);
				av_pairs->DnsDomainName.length = AvLen;
				av_pairs->DnsDomainName.value = value;
				break;

			case MsvAvDnsTreeName:
				//printf("AvId: MsvAvDnsTreeName, AvLen: %d\n", AvLen);
				av_pairs->DnsTreeName.length = AvLen;
				av_pairs->DnsTreeName.value = value;
				break;

			case MsvAvTimestamp:
				//printf("AvId: MsvAvTimestamp, AvLen: %d\n", AvLen);
				av_pairs->Timestamp.length = AvLen;
				av_pairs->Timestamp.value = value;
				break;

			case MsvAvRestrictions:
				//printf("AvId: MsvAvRestrictions, AvLen: %d\n", AvLen);
				av_pairs->Restrictions.length = AvLen;
				av_pairs->Restrictions.value = value;
				break;

			case MsvAvTargetName:
				//printf("AvId: MsvAvTargetName, AvLen: %d\n", AvLen);
				av_pairs->TargetName.length = AvLen;
				av_pairs->TargetName.value = value;
				break;

			case MsvChannelBindings:
				//printf("AvId: MsvAvChannelBindings, AvLen: %d\n", AvLen);
				av_pairs->ChannelBindings.length = AvLen;
				av_pairs->ChannelBindings.value = value;
				break;

			default:
				if (value != NULL)
					xfree(value);
				break;
		}
	}
	while(AvId != MsvAvEOL);
}

/**
 * Output array of AV_PAIRs.\n
 * AV_PAIR @msdn{cc236646}
 * @param ntlmssp
 * @param s
 */

void ntlmssp_output_av_pairs(NTLMSSP *ntlmssp, STREAM s)
{
	AV_PAIRS *av_pairs = ntlmssp->av_pairs;
	
	if (av_pairs->NbDomainName.length > 0)
	{
		out_uint16_le(s, MsvAvNbDomainName); /* AvId */
		out_uint16_le(s, av_pairs->NbDomainName.length); /* AvLen */
		out_uint8a(s, av_pairs->NbDomainName.value, av_pairs->NbDomainName.length); /* Value */
	}

	if (av_pairs->NbComputerName.length > 0)
	{
		out_uint16_le(s, MsvAvNbComputerName); /* AvId */
		out_uint16_le(s, av_pairs->NbComputerName.length); /* AvLen */
		out_uint8a(s, av_pairs->NbComputerName.value, av_pairs->NbComputerName.length); /* Value */
	}

	if (av_pairs->DnsDomainName.length > 0)
	{
		out_uint16_le(s, MsvAvDnsDomainName); /* AvId */
		out_uint16_le(s, av_pairs->DnsDomainName.length); /* AvLen */
		out_uint8a(s, av_pairs->DnsDomainName.value, av_pairs->DnsDomainName.length); /* Value */
	}

	if (av_pairs->DnsComputerName.length > 0)
	{
		out_uint16_le(s, MsvAvDnsComputerName); /* AvId */
		out_uint16_le(s, av_pairs->DnsComputerName.length); /* AvLen */
		out_uint8a(s, av_pairs->DnsComputerName.value, av_pairs->DnsComputerName.length); /* Value */
	}

	if (av_pairs->DnsTreeName.length > 0)
	{
		out_uint16_le(s, MsvAvDnsTreeName); /* AvId */
		out_uint16_le(s, av_pairs->DnsTreeName.length); /* AvLen */
		out_uint8a(s, av_pairs->DnsTreeName.value, av_pairs->DnsTreeName.length); /* Value */
	}

	if (av_pairs->Timestamp.length > 0)
	{
		out_uint16_le(s, MsvAvTimestamp); /* AvId */
		out_uint16_le(s, av_pairs->Timestamp.length); /* AvLen */
		out_uint8a(s, av_pairs->Timestamp.value, av_pairs->Timestamp.length); /* Value */
	}

	if (av_pairs->Flags > 0)
	{
		out_uint16_le(s, MsvAvFlags); /* AvId */
		out_uint16_le(s, 4); /* AvLen */
		out_uint32_le(s, av_pairs->Flags); /* Value */
	}

	if (av_pairs->Restrictions.length > 0)
	{
		out_uint16_le(s, MsvAvRestrictions); /* AvId */
		out_uint16_le(s, av_pairs->Restrictions.length); /* AvLen */
		out_uint8a(s, av_pairs->Restrictions.value, av_pairs->Restrictions.length); /* Value */
	}

	if (av_pairs->ChannelBindings.length > 0)
	{
		out_uint16_le(s, MsvChannelBindings); /* AvId */
		out_uint16_le(s, av_pairs->ChannelBindings.length); /* AvLen */
		out_uint8a(s, av_pairs->ChannelBindings.value, av_pairs->ChannelBindings.length); /* Value */
	}

	if (av_pairs->TargetName.length > 0)
	{
		out_uint16_le(s, MsvAvTargetName); /* AvId */
		out_uint16_le(s, av_pairs->TargetName.length); /* AvLen */
		out_uint8a(s, av_pairs->TargetName.value, av_pairs->TargetName.length); /* Value */
	}

	/* This endicates the end of the AV_PAIR array */
	out_uint16_le(s, MsvAvEOL); /* AvId */
	out_uint16_le(s, 0); /* AvLen */

	if (ntlmssp->ntlm_v2)
	{
		out_uint8s(s, 8);
	}

	s_mark_end(s);
}

/**
 * Free array of AV_PAIRs.\n
 * AV_PAIR @msdn{cc236646}
 * @param ntlmssp
 */

void ntlmssp_free_av_pairs(NTLMSSP *ntlmssp)
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

static void ntlmssp_output_version(STREAM s)
{
	/* The following version information was observed with Windows 7 */

	out_uint8(s, WINDOWS_MAJOR_VERSION_6); /* ProductMajorVersion (1 byte) */
	out_uint8(s, WINDOWS_MINOR_VERSION_1); /* ProductMinorVersion (1 byte) */
	out_uint16_le(s, 7600); /* ProductBuild (2 bytes) */
	out_uint8s(s, 3); /* Reserved (3 bytes) */
	out_uint8(s, NTLMSSP_REVISION_W2K3); /* NTLMRevisionCurrent (1 byte) */
}

void ntlmssp_compute_message_integrity_check(NTLMSSP *ntlmssp)
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

void ntlmssp_encrypt_message(NTLMSSP *ntlmssp, DATABLOB *msg, DATABLOB *encrypted_msg, uint8* signature)
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
	datablob_alloc(encrypted_msg, msg->length);

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

int ntlmssp_decrypt_message(NTLMSSP *ntlmssp, DATABLOB *encrypted_msg, DATABLOB *msg, uint8* signature)
{
	HMAC_CTX hmac_ctx;
	uint8 digest[16];
	uint8 checksum[8];
	uint32 version = 1;
	uint8 expected_signature[16];

	/* Allocate space for encrypted message */
	datablob_alloc(msg, encrypted_msg->length);

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

void ntlmssp_send_negotiate_message(NTLMSSP *ntlmssp, STREAM s)
{
	int length;
	uint32 negotiateFlags = 0;

	out_uint8a(s, ntlm_signature, 8); /* Signature (8 bytes) */
	out_uint32_le(s, 1); /* MessageType */

	if (ntlmssp->ntlm_v2)
	{
		/* Observed: b7 82 08 e2, Using: 07 82 08 e2 */
		negotiateFlags |= NTLMSSP_NEGOTIATE_56;
		negotiateFlags |= NTLMSSP_NEGOTIATE_128;
		negotiateFlags |= NTLMSSP_NEGOTIATE_OEM;
		negotiateFlags |= NTLMSSP_REQUEST_TARGET;
		negotiateFlags |= NTLMSSP_NEGOTIATE_KEY_EXCH;
		negotiateFlags |= NTLMSSP_NEGOTIATE_DATAGRAM;
		negotiateFlags |= NTLMSSP_NEGOTIATE_UNICODE;
		negotiateFlags |= NTLMSSP_NEGOTIATE_VERSION;
		negotiateFlags |= NTLMSSP_TARGET_TYPE_DOMAIN;
		negotiateFlags |= NTLMSSP_REQUEST_NON_NT_SESSION_KEY;
		negotiateFlags |= NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED;
	}
	else
	{
		negotiateFlags |= NTLMSSP_NEGOTIATE_56;
		negotiateFlags |= NTLMSSP_NEGOTIATE_128;
		negotiateFlags |= NTLMSSP_NEGOTIATE_OEM;
		negotiateFlags |= NTLMSSP_REQUEST_TARGET;
		negotiateFlags |= NTLMSSP_TARGET_TYPE_DOMAIN;
		negotiateFlags |= NTLMSSP_REQUEST_NON_NT_SESSION_KEY;
		negotiateFlags |= NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED;
		negotiateFlags |= 0x00000030;
	}

	ntlmssp_output_negotiate_flags(s, negotiateFlags); /* NegotiateFlags (4 bytes) */

#ifdef WITH_DEBUG_NLA
	ntlmssp_print_negotiate_flags(negotiateFlags);
#endif

	/* only set if NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED is set */

	/* DomainNameFields (8 bytes) */
	out_uint16_le(s, 0); /* DomainNameLen */
	out_uint16_le(s, 0); /* DomainNameMaxLen */
	out_uint32_le(s, 0); /* DomainNameBufferOffset */

	/* only set if NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED is set */

	/* WorkstationFields (8 bytes) */
	out_uint16_le(s, 0); /* WorkstationLen */
	out_uint16_le(s, 0); /* WorkstationMaxLen */
	out_uint32_le(s, 0); /* WorkstationBufferOffset */

	if (negotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		/* Only present if NTLMSSP_NEGOTIATE_VERSION is set */
		ntlmssp_output_version(s);
	}

	s_mark_end(s);

	length = s->end - s->data;
	datablob_alloc(&ntlmssp->negotiate_message, length);
	memcpy(ntlmssp->negotiate_message.data, s->data, length);

#ifdef WITH_DEBUG_NLA
	printf("NEGOTIATE_MESSAGE (length = %d)\n", length);
	hexdump(s->data, length);
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

void ntlmssp_recv_challenge_message(NTLMSSP *ntlmssp, STREAM s)
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
	in_uint16_le(s, targetNameLen); /* TargetNameLen (2 bytes) */
	in_uint16_le(s, targetNameMaxLen); /* TargetNameMaxLen (2 bytes) */
	in_uint32_le(s, targetNameBufferOffset); /* TargetNameBufferOffset (4 bytes) */

	ntlmssp_input_negotiate_flags(s, &(ntlmssp->negotiate_flags)); /* NegotiateFlags (4 bytes) */

#ifdef WITH_DEBUG_NLA
	ntlmssp_print_negotiate_flags(ntlmssp->negotiate_flags);
#endif

	in_uint8a(s, ntlmssp->server_challenge, 8); /* ServerChallenge (8 bytes) */
	in_uint8s(s, 8); /* Reserved (8 bytes), should be ignored */

	/* TargetInfoFields (8 bytes) */
	in_uint16_le(s, targetInfoLen); /* TargetInfoLen (2 bytes) */
	in_uint16_le(s, targetInfoMaxLen); /* TargetInfoMaxLen (2 bytes) */
	in_uint32_le(s, targetInfoBufferOffset); /* TargetInfoBufferOffset (4 bytes) */

	/* only present if NTLMSSP_NEGOTIATE_VERSION is set */

	if (ntlmssp->negotiate_flags & NTLMSSP_NEGOTIATE_VERSION)
	{
		in_uint8s(s, 8); /* Version (8 bytes), can be ignored */
	}

	/* Payload (variable) */
	payload_offset = s->p;

	if (targetNameLen > 0)
	{
		p = start_offset + targetNameBufferOffset;
		datablob_alloc(&ntlmssp->target_name, targetNameLen);
		memcpy(ntlmssp->target_name.data, p, targetNameLen);

#ifdef WITH_DEBUG_NLA
		printf("targetName (length = %d, offset = %d)\n", targetNameLen, targetNameBufferOffset);
		hexdump(ntlmssp->target_name.data, ntlmssp->target_name.length);
		printf("\n");
#endif
	}

	if (targetInfoLen > 0)
	{
		p = start_offset + targetInfoBufferOffset;
		datablob_alloc(&ntlmssp->target_info, targetInfoLen);
		memcpy(ntlmssp->target_info.data, p, targetInfoLen);

#ifdef WITH_DEBUG_NLA
		printf("targetInfo (length = %d, offset = %d)\n", targetInfoLen, targetInfoBufferOffset);
		hexdump(ntlmssp->target_info.data, ntlmssp->target_info.length);
		printf("\n");
#endif

		if (ntlmssp->ntlm_v2)
		{
			s->p = p;
			ntlmssp_input_av_pairs(ntlmssp, s);
		}
	}

	length = (payload_offset - start_offset) + targetNameLen + targetInfoLen;

	datablob_alloc(&ntlmssp->challenge_message, length);
	memcpy(ntlmssp->challenge_message.data, start_offset, length);

#ifdef WITH_DEBUG_NLA
	printf("CHALLENGE_MESSAGE (length = %d)\n", length);
	hexdump(start_offset, length);
	printf("\n");
#endif

	/* AV_PAIRs */
	if (ntlmssp->ntlm_v2)
		ntlmssp_populate_av_pairs(ntlmssp);

#ifdef WITH_DEBUG_NLA
			printf("targetInfo (populated) (length = %d)\n", ntlmssp->target_info.length);
			hexdump(ntlmssp->target_info.data, ntlmssp->target_info.length);
			printf("\n");
#endif

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
	hexdump(ntlmssp->client_challenge, 8);
	printf("\n");

	printf("ServerChallenge\n");
	hexdump(ntlmssp->server_challenge, 8);
	printf("\n");

	printf("SessionBaseKey\n");
	hexdump(ntlmssp->session_base_key, 16);
	printf("\n");

	printf("KeyExchangeKey\n");
	hexdump(ntlmssp->key_exchange_key, 16);
	printf("\n");

	printf("ExportedSessionKey\n");
	hexdump(ntlmssp->exported_session_key, 16);
	printf("\n");

	printf("RandomSessionKey\n");
	hexdump(ntlmssp->random_session_key, 16);
	printf("\n");

	printf("ClientSignKey\n");
	hexdump(ntlmssp->client_signing_key, 16);
	printf("\n");

	printf("ClientSealingKey\n");
	hexdump(ntlmssp->client_sealing_key, 16);
	printf("\n");

	printf("Timestamp\n");
	hexdump(ntlmssp->timestamp, 8);
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

void ntlmssp_send_authenticate_message(NTLMSSP *ntlmssp, STREAM s)
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
		/* Observed: 35 82 88 e2, Using: 35 82 88 e2 */
		negotiateFlags |= NTLMSSP_NEGOTIATE_56;
		negotiateFlags |= NTLMSSP_NEGOTIATE_128;
		negotiateFlags |= NTLMSSP_NEGOTIATE_OEM;
		negotiateFlags |= NTLMSSP_REQUEST_TARGET;
		negotiateFlags |= NTLMSSP_NEGOTIATE_VERSION;
		negotiateFlags |= NTLMSSP_NEGOTIATE_DATAGRAM;
		negotiateFlags |= NTLMSSP_NEGOTIATE_UNICODE;
		negotiateFlags |= NTLMSSP_TARGET_TYPE_DOMAIN;
		negotiateFlags |= NTLMSSP_REQUEST_NON_NT_SESSION_KEY;
		negotiateFlags |= NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED;
		negotiateFlags &= ~0x00000040;
		negotiateFlags |= 0x00800030;
	}
	else
	{
		negotiateFlags |= NTLMSSP_NEGOTIATE_56;
		negotiateFlags |= NTLMSSP_NEGOTIATE_128;
		negotiateFlags |= NTLMSSP_NEGOTIATE_OEM;
		negotiateFlags |= NTLMSSP_REQUEST_TARGET;
		negotiateFlags |= NTLMSSP_TARGET_TYPE_DOMAIN;
		negotiateFlags |= NTLMSSP_REQUEST_NON_NT_SESSION_KEY;
		negotiateFlags |= NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED;
		negotiateFlags |= 0x00000030;
	}

	if (ntlmssp->ntlm_v2)
		PayloadBufferOffset = 88; /* starting buffer offset */
	else
		PayloadBufferOffset = 64; /* starting buffer offset */

	DomainNameBufferOffset = PayloadBufferOffset;
	UserNameBufferOffset = DomainNameBufferOffset + DomainNameLen;
	WorkstationBufferOffset = UserNameBufferOffset + UserNameLen;
	LmChallengeResponseBufferOffset = WorkstationBufferOffset + WorkstationLen;
	NtChallengeResponseBufferOffset = LmChallengeResponseBufferOffset + LmChallengeResponseLen;
	EncryptedRandomSessionKeyBufferOffset = NtChallengeResponseBufferOffset + NtChallengeResponseLen;

	out_uint8a(s, ntlm_signature, 8); /* Signature (8 bytes) */
	out_uint32_le(s, 3); /* MessageType */

	/* LmChallengeResponseFields (8 bytes) */
	out_uint16_le(s, LmChallengeResponseLen); /* LmChallengeResponseLen */
	out_uint16_le(s, LmChallengeResponseLen); /* LmChallengeResponseMaxLen */
	out_uint32_le(s, LmChallengeResponseBufferOffset); /* LmChallengeResponseBufferOffset */

	/* NtChallengeResponseFields (8 bytes) */
	out_uint16_le(s, NtChallengeResponseLen); /* NtChallengeResponseLen */
	out_uint16_le(s, NtChallengeResponseLen); /* NtChallengeResponseMaxLen */
	out_uint32_le(s, NtChallengeResponseBufferOffset); /* NtChallengeResponseBufferOffset */

	/* only set if NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED is set */

	/* DomainNameFields (8 bytes) */
	out_uint16_le(s, DomainNameLen); /* DomainNameLen */
	out_uint16_le(s, DomainNameLen); /* DomainNameMaxLen */
	out_uint32_le(s, DomainNameBufferOffset); /* DomainNameBufferOffset */

	/* UserNameFields (8 bytes) */
	out_uint16_le(s, UserNameLen); /* UserNameLen */
	out_uint16_le(s, UserNameLen); /* UserNameMaxLen */
	out_uint32_le(s, UserNameBufferOffset); /* UserNameBufferOffset */

	/* only set if NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED is set */

	/* WorkstationFields (8 bytes) */
	out_uint16_le(s, WorkstationLen); /* WorkstationLen */
	out_uint16_le(s, WorkstationLen); /* WorkstationMaxLen */
	out_uint32_le(s, WorkstationBufferOffset); /* WorkstationBufferOffset */

	/* EncryptedRandomSessionKeyFields (8 bytes) */
	out_uint16_le(s, EncryptedRandomSessionKeyLen); /* EncryptedRandomSessionKeyLen */
	out_uint16_le(s, EncryptedRandomSessionKeyLen); /* EncryptedRandomSessionKeyMaxLen */
	out_uint32_le(s, EncryptedRandomSessionKeyBufferOffset); /* EncryptedRandomSessionKeyBufferOffset */

	ntlmssp_output_negotiate_flags(s, negotiateFlags); /* NegotiateFlags (4 bytes) */

#ifdef WITH_DEBUG_NLA
	ntlmssp_print_negotiate_flags(negotiateFlags);
#endif
	
	if (ntlmssp->ntlm_v2)
	{
		/* Version */
		ntlmssp_output_version(s);

		/* Message Integrity Check */
		mic_offset = s->p;
		out_uint8s(s, 16);
	}

	/* DomainName */
	if (DomainNameLen > 0)
	{
		out_uint8p(s, DomainNameBuffer, DomainNameLen);
#ifdef WITH_DEBUG_NLA
		printf("DomainName (length = %d, offset = %d)\n", DomainNameLen, DomainNameBufferOffset);
		hexdump(DomainNameBuffer, DomainNameLen);
		printf("\n");
#endif
	}

	/* UserName */
	out_uint8p(s, UserNameBuffer, UserNameLen);

#ifdef WITH_DEBUG_NLA
	printf("UserName (length = %d, offset = %d)\n", UserNameLen, UserNameBufferOffset);
	hexdump(UserNameBuffer, UserNameLen);
	printf("\n");
#endif

	/* Workstation */
	if (WorkstationLen > 0)
	{
		out_uint8p(s, WorkstationBuffer, WorkstationLen);
#ifdef WITH_DEBUG_NLA
		printf("Workstation (length = %d, offset = %d)\n", WorkstationLen, WorkstationBufferOffset);
		hexdump(WorkstationBuffer, WorkstationLen);
		printf("\n");
#endif
	}

	/* LmChallengeResponse */
	out_uint8p(s, ntlmssp->lm_challenge_response.data, LmChallengeResponseLen);

#ifdef WITH_DEBUG_NLA
	printf("LmChallengeResponse (length = %d, offset = %d)\n", LmChallengeResponseLen, LmChallengeResponseBufferOffset);
	hexdump(ntlmssp->lm_challenge_response.data, LmChallengeResponseLen);
	printf("\n");
#endif

	/* NtChallengeResponse */
	out_uint8p(s, ntlmssp->nt_challenge_response.data, NtChallengeResponseLen);

#ifdef WITH_DEBUG_NLA
	printf("NtChallengeResponse (length = %d, offset = %d)\n", NtChallengeResponseLen, NtChallengeResponseBufferOffset);
	hexdump(ntlmssp->nt_challenge_response.data, NtChallengeResponseLen);
	printf("\n");
#endif

	/* EncryptedRandomSessionKey */
	out_uint8p(s, EncryptedRandomSessionKeyBuffer, EncryptedRandomSessionKeyLen);
	s_mark_end(s);

#ifdef WITH_DEBUG_NLA
	printf("EncryptedRandomSessionKey (length = %d, offset = %d)\n", EncryptedRandomSessionKeyLen, EncryptedRandomSessionKeyBufferOffset);
	hexdump(EncryptedRandomSessionKeyBuffer, EncryptedRandomSessionKeyLen);
	printf("\n");
#endif

	length = s->end - s->data;
	datablob_alloc(&ntlmssp->authenticate_message, length);
	memcpy(ntlmssp->authenticate_message.data, s->data, length);

	if (ntlmssp->ntlm_v2)
	{
		/* Message Integrity Check */
		ntlmssp_compute_message_integrity_check(ntlmssp);
		
		s->p = mic_offset;
		out_uint8p(s, ntlmssp->message_integrity_check, 16);
	}

#ifdef WITH_DEBUG_NLA
	printf("AUTHENTICATE_MESSAGE (length = %d)\n", length);
	hexdump(s->data, length);
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

int ntlmssp_send(NTLMSSP *ntlmssp, STREAM s)
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

int ntlmssp_recv(NTLMSSP *ntlmssp, STREAM s)
{
	char signature[8]; /* Signature, "NTLMSSP" */
	uint32 messageType; /* MessageType */

	in_uint8a(s, signature, 8);
	in_uint32_le(s, messageType);

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
	NTLMSSP *ntlmssp = (NTLMSSP*) xmalloc(sizeof(NTLMSSP));

	if (ntlmssp != NULL)
	{
		memset(ntlmssp, '\0', sizeof(NTLMSSP));
		ntlmssp->av_pairs = (AV_PAIRS*)xmalloc(sizeof(AV_PAIRS));
		memset(ntlmssp->av_pairs, 0, sizeof(AV_PAIRS));
		ntlmssp_init(ntlmssp);
	}

	return ntlmssp;
}

/**
 * Initialize NTLMSSP state machine.
 * @param ntlmssp
 */

void ntlmssp_init(NTLMSSP *ntlmssp)
{
	ntlmssp->state = NTLMSSP_STATE_INITIAL;
	ntlmssp->uniconv = freerdp_uniconv_new();
}

/**
 * Finalize NTLMSSP state machine.
 * @param ntlmssp
 */

void ntlmssp_uninit(NTLMSSP *ntlmssp)
{
	datablob_free(&ntlmssp->username);
	datablob_free(&ntlmssp->password);
	datablob_free(&ntlmssp->domain);

	datablob_free(&ntlmssp->spn);
	datablob_free(&ntlmssp->workstation);
	datablob_free(&ntlmssp->target_info);
	datablob_free(&ntlmssp->target_name);

	datablob_free(&ntlmssp->negotiate_message);
	datablob_free(&ntlmssp->challenge_message);
	datablob_free(&ntlmssp->authenticate_message);

	datablob_free(&ntlmssp->lm_challenge_response);
	datablob_free(&ntlmssp->nt_challenge_response);

	ntlmssp_free_av_pairs(ntlmssp);
	freerdp_uniconv_free(ntlmssp->uniconv);

	ntlmssp->state = NTLMSSP_STATE_FINAL;
}

/**
 * Free NTLMSSP state machine.
 * @param ntlmssp
 */

void ntlmssp_free(NTLMSSP *ntlmssp)
{
	ntlmssp_uninit(ntlmssp);

	if (ntlmssp->send_rc4_seal)
		crypto_rc4_free(ntlmssp->send_rc4_seal);
	if (ntlmssp->recv_rc4_seal)
		crypto_rc4_free(ntlmssp->recv_rc4_seal);
	xfree(ntlmssp);
}

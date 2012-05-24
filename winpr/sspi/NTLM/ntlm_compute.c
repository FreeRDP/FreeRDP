/**
 * WinPR: Windows Portable Runtime
 * NTLM Security Package (Compute)
 *
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "ntlm.h"
#include "../sspi.h"

#include <time.h>
#include <openssl/des.h>
#include <openssl/md4.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/engine.h>
#include <freerdp/crypto/crypto.h>

#include <winpr/crt.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/hexdump.h>

#include "ntlm_compute.h"

static const char lm_magic[] = "KGS!@#$%";

static const char client_sign_magic[] = "session key to client-to-server signing key magic constant";
static const char server_sign_magic[] = "session key to server-to-client signing key magic constant";
static const char client_seal_magic[] = "session key to client-to-server sealing key magic constant";
static const char server_seal_magic[] = "session key to server-to-client sealing key magic constant";

/**
 * Output Restriction_Encoding.\n
 * Restriction_Encoding @msdn{cc236647}
 * @param NTLM context
 */

void ntlm_output_restriction_encoding(NTLM_CONTEXT* context)
{
	STREAM* s;
	AV_PAIR* restrictions = &context->av_pairs->Restrictions;

	BYTE machineID[32] =
		"\x3A\x15\x8E\xA6\x75\x82\xD8\xF7\x3E\x06\xFA\x7A\xB4\xDF\xFD\x43"
		"\x84\x6C\x02\x3A\xFD\x5A\x94\xFE\xCF\x97\x0F\x3D\x19\x2C\x38\x20";

	restrictions->value = malloc(48);
	restrictions->length = 48;

	s = stream_new(0);
	stream_attach(s, restrictions->value, restrictions->length);

	stream_write_uint32(s, 48); /* Size */
	stream_write_zero(s, 4); /* Z4 (set to zero) */

	/* IntegrityLevel (bit 31 set to 1) */
	stream_write_uint8(s, 1);
	stream_write_zero(s, 3);

	stream_write_uint32(s, 0x00002000); /* SubjectIntegrityLevel */
	stream_write(s, machineID, 32); /* MachineID */

	free(s);
}

/**
 * Output TargetName.\n
 * @param NTLM context
 */

void ntlm_output_target_name(NTLM_CONTEXT* context)
{
	STREAM* s;
	AV_PAIR* TargetName = &context->av_pairs->TargetName;

	/*
	 * TODO: No idea what should be set here (observed MsvAvTargetName = MsvAvDnsComputerName or
	 * MsvAvTargetName should be the name of the service be accessed after authentication)
	 * here used: "TERMSRV/192.168.0.123" in unicode (Dmitrij Jasnov)
	 */
	BYTE name[42] =
			"\x54\x00\x45\x00\x52\x00\x4d\x00\x53\x00\x52\x00\x56\x00\x2f\x00\x31\x00\x39\x00\x32"
			"\x00\x2e\x00\x31\x00\x36\x00\x38\x00\x2e\x00\x30\x00\x2e\x00\x31\x00\x32\x00\x33\x00";

	TargetName->length = 42;
	TargetName->value = (BYTE*) malloc(TargetName->length);

	s = stream_new(0);
	stream_attach(s, TargetName->value, TargetName->length);

	stream_write(s, name, TargetName->length);

	free(s);
}

/**
 * Output ChannelBindings.\n
 * @param NTLM context
 */

void ntlm_output_channel_bindings(NTLM_CONTEXT* context)
{
	STREAM* s;
	AV_PAIR* ChannelBindings = &context->av_pairs->ChannelBindings;

	ChannelBindings->value = (BYTE*) malloc(48);
	ChannelBindings->length = 16;

	s = stream_new(0);
	stream_attach(s, ChannelBindings->value, ChannelBindings->length);

	stream_write_zero(s, 16); /* an all-zero value of the hash is used to indicate absence of channel bindings */

	free(s);
}

/**
 * Get current time, in tenths of microseconds since midnight of January 1, 1601.
 * @param[out] timestamp 64-bit little-endian timestamp
 */

void ntlm_current_time(BYTE* timestamp)
{
	uint64 time64;

	/* Timestamp (8 bytes), represented as the number of tenths of microseconds since midnight of January 1, 1601 */
	time64 = time(NULL) + 11644473600LL; /* Seconds since January 1, 1601 */
	time64 *= 10000000; /* Convert timestamp to tenths of a microsecond */

	CopyMemory(timestamp, &time64, 8); /* Copy into timestamp in little-endian */
}

/**
 * Generate timestamp for AUTHENTICATE_MESSAGE.
 * @param NTLM context
 */

void ntlm_generate_timestamp(NTLM_CONTEXT* context)
{
	ntlm_current_time(context->Timestamp);

	if (context->ntlm_v2)
	{
		if (context->av_pairs->Timestamp.length == 8)
		{
			CopyMemory(context->av_pairs->Timestamp.value, context->Timestamp, 8);
			return;
		}
	}
	else
	{
		if (context->av_pairs->Timestamp.length != 8)
		{
			context->av_pairs->Timestamp.length = 8;
			context->av_pairs->Timestamp.value = malloc(context->av_pairs->Timestamp.length);
		}

		CopyMemory(context->av_pairs->Timestamp.value, context->Timestamp, 8);
	}
}

void ntlm_compute_ntlm_hash(uint16* password, uint32 length, char* hash)
{
	/* NTLMv1("password") = 8846F7EAEE8FB117AD06BDD830B7586C */

	MD4_CTX md4_ctx;

	/* Password needs to be in unicode */

	/* Apply the MD4 digest algorithm on the password in unicode, the result is the NTLM hash */

	MD4_Init(&md4_ctx);
	MD4_Update(&md4_ctx, password, length);
	MD4_Final((void*) hash, &md4_ctx);
}

static void ascii_hex_string_to_binary(char* str, unsigned char* hex)
{
	int i;
	int length;

	CharUpperA(str);

	length = strlen(str);

	for (i = 0; i < length / 2; i++)
	{
		hex[i] = 0;

		if ((str[i * 2] >= '0') && (str[i * 2] <= '9'))
			hex[i] |= (str[i * 2] - '0') << 4;

		if ((str[i * 2] >= 'A') && (str[i * 2] <= 'F'))
			hex[i] |= (str[i * 2] - 'A' + 10) << 4;

		if ((str[i * 2 + 1] >= '0') && (str[i * 2 + 1] <= '9'))
			hex[i] |= (str[i * 2 + 1] - '0');

		if ((str[i * 2 + 1] >= 'A') && (str[i * 2 + 1] <= 'F'))
			hex[i] |= (str[i * 2 + 1] - 'A' + 10);
	}
}

/*
 * username // password
 * username:661e58eb6743798326f388fc5edb0b3a
 */

void ntlm_fetch_ntlm_v2_hash(NTLM_CONTEXT* context, char* hash)
{
	FILE* fp;
	char* data;
	char* line;
	int length;
	char* db_user;
	char* db_hash;
	UINT16* User;
	UINT32 UserLength;
	long int file_size;
	BYTE db_hash_bin[16];

	/* Fetch NTLMv2 hash from database */

	fp = fopen("/etc/winpr/SAM.txt", "r");

	fseek(fp, 0, SEEK_END);
	file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (file_size < 1)
		return;

	data = (char*) malloc(file_size + 2);

	if (fread(data, file_size, 1, fp) != 1)
	{
		free(data);
		return;
	}

	data[file_size] = '\n';
	data[file_size + 1] = '\0';
	line = strtok(data, "\n");

	while (line != NULL)
	{
		length = strlen(line);

		if (length > 0)
		{
			length = strcspn(line, ":");
			line[length] = '\0';

			db_user = line;
			db_hash = &line[length + 1];

			UserLength = strlen(db_user) * 2;
			User = (uint16*) malloc(UserLength);
			MultiByteToWideChar(CP_ACP, 0, db_user, strlen(db_user),
					(LPWSTR) User, UserLength / 2);

			if (UserLength == context->identity.UserLength)
			{
				if (memcmp(User, context->identity.User, UserLength) == 0)
				{
					ascii_hex_string_to_binary(db_hash, db_hash_bin);
					CopyMemory(hash, db_hash_bin, 16);
				}
			}
		}

		line = strtok(NULL, "\n");
	}

	fclose(fp);
	free(data);
}

void ntlm_compute_ntlm_v2_hash(NTLM_CONTEXT* context, char* hash)
{
	char* p;
	SecBuffer buffer;
	char ntlm_hash[16];

	if (context->identity.PasswordLength > 0)
	{
		/* First, compute the NTLMv1 hash of the password */
		ntlm_compute_ntlm_hash(context->identity.Password, context->identity.PasswordLength, ntlm_hash);
	}

	sspi_SecBufferAlloc(&buffer, context->identity.UserLength + context->identity.DomainLength);
	p = (char*) buffer.pvBuffer;

	/* Concatenate(Uppercase(username),domain)*/
	CopyMemory(p, context->identity.User, context->identity.UserLength);
	CharUpperBuffW((LPWSTR) p, context->identity.UserLength / 2);

	CopyMemory(&p[context->identity.UserLength], context->identity.Domain, context->identity.DomainLength);

	if (context->identity.PasswordLength > 0)
	{
		/* Compute the HMAC-MD5 hash of the above value using the NTLMv1 hash as the key, the result is the NTLMv2 hash */
		HMAC(EVP_md5(), (void*) ntlm_hash, 16, buffer.pvBuffer, buffer.cbBuffer, (void*) hash, NULL);
	}
	else
	{
		ntlm_fetch_ntlm_v2_hash(context, hash);
	}

	sspi_SecBufferFree(&buffer);
}

void ntlm_compute_lm_v2_response(NTLM_CONTEXT* context)
{
	char* response;
	char value[16];
	char ntlm_v2_hash[16];

	/* Compute the NTLMv2 hash */
	ntlm_compute_ntlm_v2_hash(context, ntlm_v2_hash);

	/* Concatenate the server and client challenges */
	CopyMemory(value, context->ServerChallenge, 8);
	CopyMemory(&value[8], context->ClientChallenge, 8);

	sspi_SecBufferAlloc(&context->LmChallengeResponse, 24);
	response = (char*) context->LmChallengeResponse.pvBuffer;

	/* Compute the HMAC-MD5 hash of the resulting value using the NTLMv2 hash as the key */
	HMAC(EVP_md5(), (void*) ntlm_v2_hash, 16, (void*) value, 16, (void*) response, NULL);

	/* Concatenate the resulting HMAC-MD5 hash and the client challenge, giving us the LMv2 response (24 bytes) */
	CopyMemory(&response[16], context->ClientChallenge, 8);
}

/**
 * Compute NTLMv2 Response.\n
 * NTLMv2_RESPONSE @msdn{cc236653}\n
 * NTLMv2 Authentication @msdn{cc236700}
 * @param NTLM context
 */

void ntlm_compute_ntlm_v2_response(NTLM_CONTEXT* context)
{
	BYTE* blob;
	BYTE ntlm_v2_hash[16];
	BYTE nt_proof_str[16];
	SecBuffer ntlm_v2_temp;
	SecBuffer ntlm_v2_temp_chal;

	sspi_SecBufferAlloc(&ntlm_v2_temp, context->TargetInfo.cbBuffer + 28);

	memset(ntlm_v2_temp.pvBuffer, '\0', ntlm_v2_temp.cbBuffer);
	blob = (BYTE*) ntlm_v2_temp.pvBuffer;

	/* Compute the NTLMv2 hash */
	ntlm_compute_ntlm_v2_hash(context, (char*) ntlm_v2_hash);

#ifdef WITH_DEBUG_NTLM
	printf("Password (length = %d)\n", context->identity.PasswordLength);
	freerdp_hexdump((BYTE*) context->identity.Password, context->identity.PasswordLength);
	printf("\n");

	printf("Username (length = %d)\n", context->identity.UserLength);
	freerdp_hexdump((BYTE*) context->identity.User, context->identity.UserLength);
	printf("\n");

	printf("Domain (length = %d)\n", context->identity.DomainLength);
	freerdp_hexdump((BYTE*) context->identity.Domain, context->identity.DomainLength);
	printf("\n");

	printf("Workstation (length = %d)\n", context->WorkstationLength);
	freerdp_hexdump((BYTE*) context->Workstation, context->WorkstationLength);
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
	CopyMemory(&blob[8], context->av_pairs->Timestamp.value, 8); /* Timestamp (8 bytes) */
	CopyMemory(&blob[16], context->ClientChallenge, 8); /* ClientChallenge (8 bytes) */
	/* Reserved3 (4 bytes) */
	CopyMemory(&blob[28], context->TargetInfo.pvBuffer, context->TargetInfo.cbBuffer);

#ifdef WITH_DEBUG_NTLM
	printf("NTLMv2 Response Temp Blob\n");
	freerdp_hexdump(ntlm_v2_temp.pvBuffer, ntlm_v2_temp.cbBuffer);
	printf("\n");
#endif

	/* Concatenate server challenge with temp */
	sspi_SecBufferAlloc(&ntlm_v2_temp_chal, ntlm_v2_temp.cbBuffer + 8);
	blob = (BYTE*) ntlm_v2_temp_chal.pvBuffer;
	CopyMemory(blob, context->ServerChallenge, 8);
	CopyMemory(&blob[8], ntlm_v2_temp.pvBuffer, ntlm_v2_temp.cbBuffer);

	HMAC(EVP_md5(), (void*) ntlm_v2_hash, 16, ntlm_v2_temp_chal.pvBuffer,
		ntlm_v2_temp_chal.cbBuffer, (void*) nt_proof_str, NULL);

	/* NtChallengeResponse, Concatenate NTProofStr with temp */
	sspi_SecBufferAlloc(&context->NtChallengeResponse, ntlm_v2_temp.cbBuffer + 16);
	blob = (BYTE*) context->NtChallengeResponse.pvBuffer;
	CopyMemory(blob, nt_proof_str, 16);
	CopyMemory(&blob[16], ntlm_v2_temp.pvBuffer, ntlm_v2_temp.cbBuffer);

	/* Compute SessionBaseKey, the HMAC-MD5 hash of NTProofStr using the NTLMv2 hash as the key */
	HMAC(EVP_md5(), (void*) ntlm_v2_hash, 16, (void*) nt_proof_str, 16, (void*) context->SessionBaseKey, NULL);

	sspi_SecBufferFree(&ntlm_v2_temp);
	sspi_SecBufferFree(&ntlm_v2_temp_chal);
}

/**
 * Encrypt the given plain text using RC4 and the given key.
 * @param key RC4 key
 * @param length text length
 * @param plaintext plain text
 * @param ciphertext cipher text
 */

void ntlm_rc4k(BYTE* key, int length, BYTE* plaintext, BYTE* ciphertext)
{
	CryptoRc4 rc4;

	/* Initialize RC4 cipher with key */
	rc4 = crypto_rc4_init((void*) key, 16);

	/* Encrypt plaintext with key */
	crypto_rc4(rc4, length, (void*) plaintext, (void*) ciphertext);

	/* Free RC4 Cipher */
	crypto_rc4_free(rc4);
}

/**
 * Generate client challenge (8-byte nonce).
 * @param NTLM context
 */

void ntlm_generate_client_challenge(NTLM_CONTEXT* context)
{
	/* ClientChallenge is used in computation of LMv2 and NTLMv2 responses */
	crypto_nonce(context->ClientChallenge, 8);
}

/**
 * Generate server challenge (8-byte nonce).
 * @param NTLM context
 */

void ntlm_generate_server_challenge(NTLM_CONTEXT* context)
{
	crypto_nonce(context->ServerChallenge, 8);
}

/**
 * Generate KeyExchangeKey (the 128-bit SessionBaseKey).\n
 * @msdn{cc236710}
 * @param NTLM context
 */

void ntlm_generate_key_exchange_key(NTLM_CONTEXT* context)
{
	/* In NTLMv2, KeyExchangeKey is the 128-bit SessionBaseKey */
	CopyMemory(context->KeyExchangeKey, context->SessionBaseKey, 16);
}

/**
 * Generate RandomSessionKey (16-byte nonce).
 * @param NTLM context
 */

void ntlm_generate_random_session_key(NTLM_CONTEXT* context)
{
	crypto_nonce(context->RandomSessionKey, 16);
}

/**
 * Generate ExportedSessionKey (the RandomSessionKey, exported)
 * @param NTLM context
 */

void ntlm_generate_exported_session_key(NTLM_CONTEXT* context)
{
	CopyMemory(context->ExportedSessionKey, context->RandomSessionKey, 16);
}

/**
 * Encrypt RandomSessionKey (RC4-encrypted RandomSessionKey, using KeyExchangeKey as the key).
 * @param NTLM context
 */

void ntlm_encrypt_random_session_key(NTLM_CONTEXT* context)
{
	/* In NTLMv2, EncryptedRandomSessionKey is the ExportedSessionKey RC4-encrypted with the KeyExchangeKey */
	ntlm_rc4k(context->KeyExchangeKey, 16, context->RandomSessionKey, context->EncryptedRandomSessionKey);
}

/**
 * Decrypt RandomSessionKey (RC4-encrypted RandomSessionKey, using KeyExchangeKey as the key).
 * @param NTLM context
 */

void ntlm_decrypt_random_session_key(NTLM_CONTEXT* context)
{
	/* In NTLMv2, EncryptedRandomSessionKey is the ExportedSessionKey RC4-encrypted with the KeyExchangeKey */
	ntlm_rc4k(context->KeyExchangeKey, 16, context->EncryptedRandomSessionKey, context->RandomSessionKey);
}

/**
 * Generate signing key.\n
 * @msdn{cc236711}
 * @param exported_session_key ExportedSessionKey
 * @param sign_magic Sign magic string
 * @param signing_key Destination signing key
 */

void ntlm_generate_signing_key(BYTE* exported_session_key, PSecBuffer sign_magic, BYTE* signing_key)
{
	int length;
	BYTE* value;
	CryptoMd5 md5;

	length = 16 + sign_magic->cbBuffer;
	value = (BYTE*) malloc(length);

	/* Concatenate ExportedSessionKey with sign magic */
	CopyMemory(value, exported_session_key, 16);
	CopyMemory(&value[16], sign_magic->pvBuffer, sign_magic->cbBuffer);

	md5 = crypto_md5_init();
	crypto_md5_update(md5, value, length);
	crypto_md5_final(md5, signing_key);

	free(value);
}

/**
 * Generate client signing key (ClientSigningKey).\n
 * @msdn{cc236711}
 * @param NTLM context
 */

void ntlm_generate_client_signing_key(NTLM_CONTEXT* context)
{
	SecBuffer sign_magic;
	sign_magic.pvBuffer = (void*) client_sign_magic;
	sign_magic.cbBuffer = sizeof(client_sign_magic);
	ntlm_generate_signing_key(context->ExportedSessionKey, &sign_magic, context->ClientSigningKey);
}

/**
 * Generate server signing key (ServerSigningKey).\n
 * @msdn{cc236711}
 * @param NTLM context
 */

void ntlm_generate_server_signing_key(NTLM_CONTEXT* context)
{
	SecBuffer sign_magic;
	sign_magic.pvBuffer = (void*) server_sign_magic;
	sign_magic.cbBuffer = sizeof(server_sign_magic);
	ntlm_generate_signing_key(context->ExportedSessionKey, &sign_magic, context->ServerSigningKey);
}

/**
 * Generate sealing key.\n
 * @msdn{cc236712}
 * @param exported_session_key ExportedSessionKey
 * @param seal_magic Seal magic string
 * @param sealing_key Destination sealing key
 */

void ntlm_generate_sealing_key(BYTE* exported_session_key, PSecBuffer seal_magic, BYTE* sealing_key)
{
	BYTE* p;
	CryptoMd5 md5;
	SecBuffer buffer;

	sspi_SecBufferAlloc(&buffer, 16 + seal_magic->cbBuffer);
	p = (BYTE*) buffer.pvBuffer;

	/* Concatenate ExportedSessionKey with seal magic */
	CopyMemory(p, exported_session_key, 16);
	CopyMemory(&p[16], seal_magic->pvBuffer, seal_magic->cbBuffer);

	md5 = crypto_md5_init();
	crypto_md5_update(md5, buffer.pvBuffer, buffer.cbBuffer);
	crypto_md5_final(md5, sealing_key);

	sspi_SecBufferFree(&buffer);
}

/**
 * Generate client sealing key (ClientSealingKey).\n
 * @msdn{cc236712}
 * @param NTLM context
 */

void ntlm_generate_client_sealing_key(NTLM_CONTEXT* context)
{
	SecBuffer seal_magic;
	seal_magic.pvBuffer = (void*) client_seal_magic;
	seal_magic.cbBuffer = sizeof(client_seal_magic);
	ntlm_generate_signing_key(context->ExportedSessionKey, &seal_magic, context->ClientSealingKey);
}

/**
 * Generate server sealing key (ServerSealingKey).\n
 * @msdn{cc236712}
 * @param NTLM context
 */

void ntlm_generate_server_sealing_key(NTLM_CONTEXT* context)
{
	SecBuffer seal_magic;
	seal_magic.pvBuffer = (void*) server_seal_magic;
	seal_magic.cbBuffer = sizeof(server_seal_magic);
	ntlm_generate_signing_key(context->ExportedSessionKey, &seal_magic, context->ServerSealingKey);
}

/**
 * Initialize RC4 stream cipher states for sealing.
 * @param NTLM context
 */

void ntlm_init_rc4_seal_states(NTLM_CONTEXT* context)
{
	if (context->server)
	{
		context->SendSigningKey = context->ServerSigningKey;
		context->RecvSigningKey = context->ClientSigningKey;
		context->SendSealingKey = context->ClientSealingKey;
		context->RecvSealingKey = context->ServerSealingKey;
		context->SendRc4Seal = crypto_rc4_init(context->ServerSealingKey, 16);
		context->RecvRc4Seal = crypto_rc4_init(context->ClientSealingKey, 16);
	}
	else
	{
		context->SendSigningKey = context->ClientSigningKey;
		context->RecvSigningKey = context->ServerSigningKey;
		context->SendSealingKey = context->ServerSealingKey;
		context->RecvSealingKey = context->ClientSealingKey;
		context->SendRc4Seal = crypto_rc4_init(context->ClientSealingKey, 16);
		context->RecvRc4Seal = crypto_rc4_init(context->ServerSealingKey, 16);
	}
}

void ntlm_compute_message_integrity_check(NTLM_CONTEXT* context)
{
	HMAC_CTX hmac_ctx;

	/*
	 * Compute the HMAC-MD5 hash of ConcatenationOf(NEGOTIATE_MESSAGE,
	 * CHALLENGE_MESSAGE, AUTHENTICATE_MESSAGE) using the ExportedSessionKey
	 */

	HMAC_CTX_init(&hmac_ctx);
	HMAC_Init_ex(&hmac_ctx, context->ExportedSessionKey, 16, EVP_md5(), NULL);
	HMAC_Update(&hmac_ctx, context->NegotiateMessage.pvBuffer, context->NegotiateMessage.cbBuffer);
	HMAC_Update(&hmac_ctx, context->ChallengeMessage.pvBuffer, context->ChallengeMessage.cbBuffer);
	HMAC_Update(&hmac_ctx, context->AuthenticateMessage.pvBuffer, context->AuthenticateMessage.cbBuffer);
	HMAC_Final(&hmac_ctx, context->MessageIntegrityCheck, NULL);
}


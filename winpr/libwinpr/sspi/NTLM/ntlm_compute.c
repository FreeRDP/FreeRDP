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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ntlm.h"
#include "../sspi.h"

#include <winpr/crt.h>
#include <winpr/sam.h>
#include <winpr/ntlm.h>
#include <winpr/print.h>
#include <winpr/sysinfo.h>

#include "ntlm_compute.h"

const char lm_magic[] = "KGS!@#$%";

static const char client_sign_magic[] = "session key to client-to-server signing key magic constant";
static const char server_sign_magic[] = "session key to server-to-client signing key magic constant";
static const char client_seal_magic[] = "session key to client-to-server sealing key magic constant";
static const char server_seal_magic[] = "session key to server-to-client sealing key magic constant";

/**
 * Populate VERSION structure.\n
 * VERSION @msdn{cc236654}
 * @param s
 */

void ntlm_get_version_info(NTLM_VERSION_INFO* versionInfo)
{
	OSVERSIONINFOA osVersionInfo;

	osVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);

	GetVersionExA(&osVersionInfo);

	versionInfo->ProductMajorVersion = (UINT8) osVersionInfo.dwMajorVersion;
	versionInfo->ProductMinorVersion = (UINT8) osVersionInfo.dwMinorVersion;
	versionInfo->ProductBuild = (UINT16) osVersionInfo.dwBuildNumber;
	ZeroMemory(versionInfo->Reserved, sizeof(versionInfo->Reserved));
	versionInfo->NTLMRevisionCurrent = NTLMSSP_REVISION_W2K3;
}

/**
 * Read VERSION structure.\n
 * VERSION @msdn{cc236654}
 * @param s
 */

void ntlm_read_version_info(wStream* s, NTLM_VERSION_INFO* versionInfo)
{
	Stream_Read_UINT8(s, versionInfo->ProductMajorVersion); /* ProductMajorVersion (1 byte) */
	Stream_Read_UINT8(s, versionInfo->ProductMinorVersion); /* ProductMinorVersion (1 byte) */
	Stream_Read_UINT16(s, versionInfo->ProductBuild); /* ProductBuild (2 bytes) */
	Stream_Read(s, versionInfo->Reserved, sizeof(versionInfo->Reserved)); /* Reserved (3 bytes) */
	Stream_Read_UINT8(s, versionInfo->NTLMRevisionCurrent); /* NTLMRevisionCurrent (1 byte) */
}

/**
 * Write VERSION structure.\n
 * VERSION @msdn{cc236654}
 * @param s
 */

void ntlm_write_version_info(wStream* s, NTLM_VERSION_INFO* versionInfo)
{
	Stream_Write_UINT8(s, versionInfo->ProductMajorVersion); /* ProductMajorVersion (1 byte) */
	Stream_Write_UINT8(s, versionInfo->ProductMinorVersion); /* ProductMinorVersion (1 byte) */
	Stream_Write_UINT16(s, versionInfo->ProductBuild); /* ProductBuild (2 bytes) */
	Stream_Write(s, versionInfo->Reserved, sizeof(versionInfo->Reserved)); /* Reserved (3 bytes) */
	Stream_Write_UINT8(s, versionInfo->NTLMRevisionCurrent); /* NTLMRevisionCurrent (1 byte) */
}

/**
 * Print VERSION structure.\n
 * VERSION @msdn{cc236654}
 * @param s
 */

void ntlm_print_version_info(NTLM_VERSION_INFO* versionInfo)
{
	fprintf(stderr, "VERSION =\n{\n");
	fprintf(stderr, "\tProductMajorVersion: %d\n", versionInfo->ProductMajorVersion);
	fprintf(stderr, "\tProductMinorVersion: %d\n", versionInfo->ProductMinorVersion);
	fprintf(stderr, "\tProductBuild: %d\n", versionInfo->ProductBuild);
	fprintf(stderr, "\tReserved: 0x%02X%02X%02X\n", versionInfo->Reserved[0],
			versionInfo->Reserved[1], versionInfo->Reserved[2]);
	fprintf(stderr, "\tNTLMRevisionCurrent: 0x%02X\n", versionInfo->NTLMRevisionCurrent);
}

void ntlm_read_ntlm_v2_client_challenge(wStream* s, NTLMv2_CLIENT_CHALLENGE* challenge)
{
	size_t size;

	Stream_Read_UINT8(s, challenge->RespType);
	Stream_Read_UINT8(s, challenge->HiRespType);
	Stream_Read_UINT16(s, challenge->Reserved1);
	Stream_Read_UINT32(s, challenge->Reserved2);
	Stream_Read(s, challenge->Timestamp, 8);
	Stream_Read(s, challenge->ClientChallenge, 8);
	Stream_Read_UINT32(s, challenge->Reserved3);

	size = Stream_Length(s) - Stream_GetPosition(s);
	challenge->AvPairs = (NTLM_AV_PAIR*) malloc(size);
	Stream_Read(s, challenge->AvPairs, size);
}

void ntlm_write_ntlm_v2_client_challenge(wStream* s, NTLMv2_CLIENT_CHALLENGE* challenge)
{
	ULONG length;

	Stream_Write_UINT8(s, challenge->RespType);
	Stream_Write_UINT8(s, challenge->HiRespType);
	Stream_Write_UINT16(s, challenge->Reserved1);
	Stream_Write_UINT32(s, challenge->Reserved2);
	Stream_Write(s, challenge->Timestamp, 8);
	Stream_Write(s, challenge->ClientChallenge, 8);
	Stream_Write_UINT32(s, challenge->Reserved3);

	length = ntlm_av_pair_list_length(challenge->AvPairs);
	Stream_Write(s, challenge->AvPairs, length);
}

void ntlm_read_ntlm_v2_response(wStream* s, NTLMv2_RESPONSE* response)
{
	Stream_Read(s, response->Response, 16);
	ntlm_read_ntlm_v2_client_challenge(s, &(response->Challenge));
}

void ntlm_write_ntlm_v2_response(wStream* s, NTLMv2_RESPONSE* response)
{
	Stream_Write(s, response->Response, 16);
	ntlm_write_ntlm_v2_client_challenge(s, &(response->Challenge));
}

#if 0

/**
 * Output Restriction_Encoding.\n
 * Restriction_Encoding @msdn{cc236647}
 * @param NTLM context
 */

void ntlm_output_restriction_encoding(NTLM_CONTEXT* context)
{
	wStream* s;
	AV_PAIR* restrictions = &context->av_pairs->Restrictions;

	BYTE machineID[32] =
		"\x3A\x15\x8E\xA6\x75\x82\xD8\xF7\x3E\x06\xFA\x7A\xB4\xDF\xFD\x43"
		"\x84\x6C\x02\x3A\xFD\x5A\x94\xFE\xCF\x97\x0F\x3D\x19\x2C\x38\x20";

	restrictions->value = malloc(48);
	restrictions->length = 48;

	s = PStreamAllocAttach(restrictions->value, restrictions->length);

	Stream_Write_UINT32(s, 48); /* Size */
	Stream_Zero(s, 4); /* Z4 (set to zero) */

	/* IntegrityLevel (bit 31 set to 1) */
	Stream_Write_UINT8(s, 1);
	Stream_Zero(s, 3);

	Stream_Write_UINT32(s, 0x00002000); /* SubjectIntegrityLevel */
	Stream_Write(s, machineID, 32); /* MachineID */

	PStreamFreeDetach(s);
}

#endif

/**
 * Get current time, in tenths of microseconds since midnight of January 1, 1601.
 * @param[out] timestamp 64-bit little-endian timestamp
 */

void ntlm_current_time(BYTE* timestamp)
{
	FILETIME filetime;
	ULARGE_INTEGER time64;

	GetSystemTimeAsFileTime(&filetime);

	time64.LowPart = filetime.dwLowDateTime;
	time64.HighPart = filetime.dwHighDateTime;

	CopyMemory(timestamp, &(time64.QuadPart), 8);
}

/**
 * Generate timestamp for AUTHENTICATE_MESSAGE.
 * @param NTLM context
 */

void ntlm_generate_timestamp(NTLM_CONTEXT* context)
{
	BYTE ZeroTimestamp[8];

	ZeroMemory(ZeroTimestamp, 8);

	if (memcmp(ZeroTimestamp, context->ChallengeTimestamp, 8) != 0)
		CopyMemory(context->Timestamp, context->ChallengeTimestamp, 8);
	else
		ntlm_current_time(context->Timestamp);
}

void ntlm_fetch_ntlm_v2_hash(NTLM_CONTEXT* context, char* hash)
{
	WINPR_SAM* sam;
	WINPR_SAM_ENTRY* entry;

	sam = SamOpen(1);
	if (sam == NULL)
		return;

	entry = SamLookupUserW(sam,
			(LPWSTR) context->identity.User, context->identity.UserLength * 2,
			(LPWSTR) context->identity.Domain, context->identity.DomainLength * 2);

	if (entry != NULL)
	{
#ifdef WITH_DEBUG_NTLM
		fprintf(stderr, "NTLM Hash:\n");
		winpr_HexDump(entry->NtHash, 16);
#endif

		NTOWFv2FromHashW(entry->NtHash,
			(LPWSTR) context->identity.User, context->identity.UserLength * 2,
			(LPWSTR) context->identity.Domain, context->identity.DomainLength * 2,
			(BYTE*) hash);

		SamFreeEntry(sam, entry);
		SamClose(sam);

		return;
	}

	entry = SamLookupUserW(sam,
		(LPWSTR) context->identity.User, context->identity.UserLength * 2, NULL, 0);

	if (entry != NULL)
	{
#ifdef WITH_DEBUG_NTLM
		fprintf(stderr, "NTLM Hash:\n");
		winpr_HexDump(entry->NtHash, 16);
#endif

		NTOWFv2FromHashW(entry->NtHash,
			(LPWSTR) context->identity.User, context->identity.UserLength * 2,
			(LPWSTR) context->identity.Domain, context->identity.DomainLength * 2,
			(BYTE*) hash);

		SamFreeEntry(sam, entry);
		SamClose(sam);

		return;
	}
	else
	{
		fprintf(stderr, "Error: Could not find user in SAM database\n");
	}
}

void ntlm_compute_ntlm_v2_hash(NTLM_CONTEXT* context, char* hash)
{
	if (context->identity.PasswordLength > 0)
	{
		NTOWFv2W((LPWSTR) context->identity.Password, context->identity.PasswordLength * 2,
				(LPWSTR) context->identity.User, context->identity.UserLength * 2,
				(LPWSTR) context->identity.Domain, context->identity.DomainLength * 2, (BYTE*) hash);
	}
	else
	{
		ntlm_fetch_ntlm_v2_hash(context, hash);
	}
}

void ntlm_compute_lm_v2_response(NTLM_CONTEXT* context)
{
	char* response;
	char value[16];
	char ntlm_v2_hash[16];

	if (context->LmCompatibilityLevel < 2)
	{
		sspi_SecBufferAlloc(&context->LmChallengeResponse, 24);
		ZeroMemory(context->LmChallengeResponse.pvBuffer, 24);
		return;
	}

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
	PSecBuffer TargetInfo;

	TargetInfo = &context->ChallengeTargetInfo;

	sspi_SecBufferAlloc(&ntlm_v2_temp, TargetInfo->cbBuffer + 28);

	ZeroMemory(ntlm_v2_temp.pvBuffer, ntlm_v2_temp.cbBuffer);
	blob = (BYTE*) ntlm_v2_temp.pvBuffer;

	/* Compute the NTLMv2 hash */
	ntlm_compute_ntlm_v2_hash(context, (char*) ntlm_v2_hash);

#ifdef WITH_DEBUG_NTLM
	fprintf(stderr, "Password (length = %d)\n", context->identity.PasswordLength * 2);
	winpr_HexDump((BYTE*) context->identity.Password, context->identity.PasswordLength * 2);
	fprintf(stderr, "\n");

	fprintf(stderr, "Username (length = %d)\n", context->identity.UserLength * 2);
	winpr_HexDump((BYTE*) context->identity.User, context->identity.UserLength * 2);
	fprintf(stderr, "\n");

	fprintf(stderr, "Domain (length = %d)\n", context->identity.DomainLength * 2);
	winpr_HexDump((BYTE*) context->identity.Domain, context->identity.DomainLength * 2);
	fprintf(stderr, "\n");

	fprintf(stderr, "Workstation (length = %d)\n", context->Workstation.Length);
	winpr_HexDump((BYTE*) context->Workstation.Buffer, context->Workstation.Length);
	fprintf(stderr, "\n");

	fprintf(stderr, "NTOWFv2, NTLMv2 Hash\n");
	winpr_HexDump(ntlm_v2_hash, 16);
	fprintf(stderr, "\n");
#endif

	/* Construct temp */
	blob[0] = 1; /* RespType (1 byte) */
	blob[1] = 1; /* HighRespType (1 byte) */
	/* Reserved1 (2 bytes) */
	/* Reserved2 (4 bytes) */
	CopyMemory(&blob[8], context->Timestamp, 8); /* Timestamp (8 bytes) */
	CopyMemory(&blob[16], context->ClientChallenge, 8); /* ClientChallenge (8 bytes) */
	/* Reserved3 (4 bytes) */
	CopyMemory(&blob[28], TargetInfo->pvBuffer, TargetInfo->cbBuffer);

#ifdef WITH_DEBUG_NTLM
	fprintf(stderr, "NTLMv2 Response Temp Blob\n");
	winpr_HexDump(ntlm_v2_temp.pvBuffer, ntlm_v2_temp.cbBuffer);
	fprintf(stderr, "\n");
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
	RC4_KEY rc4;

	/* Initialize RC4 cipher with key */
	RC4_set_key(&rc4, 16, (void*) key);

	/* Encrypt plaintext with key */
	RC4(&rc4, length, (void*) plaintext, (void*) ciphertext);
}

/**
 * Generate client challenge (8-byte nonce).
 * @param NTLM context
 */

void ntlm_generate_client_challenge(NTLM_CONTEXT* context)
{
	/* ClientChallenge is used in computation of LMv2 and NTLMv2 responses */
	RAND_bytes(context->ClientChallenge, 8);
}

/**
 * Generate server challenge (8-byte nonce).
 * @param NTLM context
 */

void ntlm_generate_server_challenge(NTLM_CONTEXT* context)
{
	RAND_bytes(context->ServerChallenge, 8);
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
	RAND_bytes(context->RandomSessionKey, 16);
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
	MD5_CTX md5;

	length = 16 + sign_magic->cbBuffer;
	value = (BYTE*) malloc(length);

	/* Concatenate ExportedSessionKey with sign magic */
	CopyMemory(value, exported_session_key, 16);
	CopyMemory(&value[16], sign_magic->pvBuffer, sign_magic->cbBuffer);

	MD5_Init(&md5);
	MD5_Update(&md5, value, length);
	MD5_Final(signing_key, &md5);

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
	MD5_CTX md5;
	SecBuffer buffer;

	sspi_SecBufferAlloc(&buffer, 16 + seal_magic->cbBuffer);
	p = (BYTE*) buffer.pvBuffer;

	/* Concatenate ExportedSessionKey with seal magic */
	CopyMemory(p, exported_session_key, 16);
	CopyMemory(&p[16], seal_magic->pvBuffer, seal_magic->cbBuffer);

	MD5_Init(&md5);
	MD5_Update(&md5, buffer.pvBuffer, buffer.cbBuffer);
	MD5_Final(sealing_key, &md5);

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
		RC4_set_key(&context->SendRc4Seal, 16, context->ServerSealingKey);
		RC4_set_key(&context->RecvRc4Seal, 16, context->ClientSealingKey);
	}
	else
	{
		context->SendSigningKey = context->ClientSigningKey;
		context->RecvSigningKey = context->ServerSigningKey;
		context->SendSealingKey = context->ServerSealingKey;
		context->RecvSealingKey = context->ClientSealingKey;
		RC4_set_key(&context->SendRc4Seal, 16, context->ClientSealingKey);
		RC4_set_key(&context->RecvRc4Seal, 16, context->ServerSealingKey);
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
	HMAC_CTX_cleanup(&hmac_ctx);
}


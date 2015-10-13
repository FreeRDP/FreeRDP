/**
 * WinPR: Windows Portable Runtime
 * NTLM Security Package (Compute)
 *
 * Copyright 2011-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
#include <winpr/crypto.h>
#include <winpr/sysinfo.h>

#include "ntlm_compute.h"

#include "../../log.h"
#define TAG WINPR_TAG("sspi.NTLM")

const char LM_MAGIC[] = "KGS!@#$%";

static const char NTLM_CLIENT_SIGN_MAGIC[] = "session key to client-to-server signing key magic constant";
static const char NTLM_SERVER_SIGN_MAGIC[] = "session key to server-to-client signing key magic constant";
static const char NTLM_CLIENT_SEAL_MAGIC[] = "session key to client-to-server sealing key magic constant";
static const char NTLM_SERVER_SEAL_MAGIC[] = "session key to server-to-client sealing key magic constant";

static const BYTE NTLM_NULL_BUFFER[16] =
{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/**
 * Populate VERSION structure.
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
 * Read VERSION structure.
 * VERSION @msdn{cc236654}
 * @param s
 */

int ntlm_read_version_info(wStream* s, NTLM_VERSION_INFO* versionInfo)
{
	if (Stream_GetRemainingLength(s) < 8)
		return -1;

	Stream_Read_UINT8(s, versionInfo->ProductMajorVersion); /* ProductMajorVersion (1 byte) */
	Stream_Read_UINT8(s, versionInfo->ProductMinorVersion); /* ProductMinorVersion (1 byte) */
	Stream_Read_UINT16(s, versionInfo->ProductBuild); /* ProductBuild (2 bytes) */
	Stream_Read(s, versionInfo->Reserved, sizeof(versionInfo->Reserved)); /* Reserved (3 bytes) */
	Stream_Read_UINT8(s, versionInfo->NTLMRevisionCurrent); /* NTLMRevisionCurrent (1 byte) */
	return 1;
}

/**
 * Write VERSION structure.
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
 * Print VERSION structure.
 * VERSION @msdn{cc236654}
 * @param s
 */

void ntlm_print_version_info(NTLM_VERSION_INFO* versionInfo)
{
	WLog_INFO(TAG, "VERSION ={");
	WLog_INFO(TAG, "\tProductMajorVersion: %d", versionInfo->ProductMajorVersion);
	WLog_INFO(TAG, "\tProductMinorVersion: %d", versionInfo->ProductMinorVersion);
	WLog_INFO(TAG, "\tProductBuild: %d", versionInfo->ProductBuild);
	WLog_INFO(TAG, "\tReserved: 0x%02X%02X%02X", versionInfo->Reserved[0],
			  versionInfo->Reserved[1], versionInfo->Reserved[2]);
	WLog_INFO(TAG, "\tNTLMRevisionCurrent: 0x%02X", versionInfo->NTLMRevisionCurrent);
}

int ntlm_read_ntlm_v2_client_challenge(wStream* s, NTLMv2_CLIENT_CHALLENGE* challenge)
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

	if (!challenge->AvPairs)
		return -1;

	Stream_Read(s, challenge->AvPairs, size);
	return 1;
}

int ntlm_write_ntlm_v2_client_challenge(wStream* s, NTLMv2_CLIENT_CHALLENGE* challenge)
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
	return 1;
}

int ntlm_read_ntlm_v2_response(wStream* s, NTLMv2_RESPONSE* response)
{
	Stream_Read(s, response->Response, 16);
	return ntlm_read_ntlm_v2_client_challenge(s, &(response->Challenge));
}

int ntlm_write_ntlm_v2_response(wStream* s, NTLMv2_RESPONSE* response)
{
	Stream_Write(s, response->Response, 16);
	return ntlm_write_ntlm_v2_client_challenge(s, &(response->Challenge));
}

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
	if (memcmp(context->ChallengeTimestamp, NTLM_NULL_BUFFER, 8) != 0)
		CopyMemory(context->Timestamp, context->ChallengeTimestamp, 8);
	else
		ntlm_current_time(context->Timestamp);
}

int ntlm_fetch_ntlm_v2_hash(NTLM_CONTEXT* context, BYTE* hash)
{
	WINPR_SAM* sam;
	WINPR_SAM_ENTRY* entry;
	SSPI_CREDENTIALS* credentials = context->credentials;

	sam = SamOpen(TRUE);

	if (!sam)
		return -1;

	entry = SamLookupUserW(sam, (LPWSTR) credentials->identity.User, credentials->identity.UserLength * 2,
			(LPWSTR) credentials->identity.Domain, credentials->identity.DomainLength * 2);

	if (entry)
	{
#ifdef WITH_DEBUG_NTLM
		WLog_DBG(TAG, "NTLM Hash:");
		winpr_HexDump(TAG, WLOG_DEBUG, entry->NtHash, 16);
#endif
		NTOWFv2FromHashW(entry->NtHash,
				(LPWSTR) credentials->identity.User, credentials->identity.UserLength * 2,
				(LPWSTR) credentials->identity.Domain, credentials->identity.DomainLength * 2,
				(BYTE*) hash);
		SamFreeEntry(sam, entry);
		SamClose(sam);
		return 1;
	}

	entry = SamLookupUserW(sam, (LPWSTR) credentials->identity.User, credentials->identity.UserLength * 2, NULL, 0);

	if (entry)
	{
#ifdef WITH_DEBUG_NTLM
		WLog_DBG(TAG, "NTLM Hash:");
		winpr_HexDump(TAG, WLOG_DEBUG, entry->NtHash, 16);
#endif
		NTOWFv2FromHashW(entry->NtHash,
				(LPWSTR) credentials->identity.User, credentials->identity.UserLength * 2,
				(LPWSTR) credentials->identity.Domain, credentials->identity.DomainLength * 2,
				(BYTE*) hash);
		SamFreeEntry(sam, entry);
		SamClose(sam);
		return 1;
	}
	else
	{
		WLog_ERR(TAG, "Error: Could not find user in SAM database");
		return 0;
	}

	SamClose(sam);
	return 1;
}

int ntlm_convert_password_hash(NTLM_CONTEXT* context, BYTE* hash)
{
	int status;
	int i, hn, ln;
	char* PasswordHash = NULL;
	UINT32 PasswordHashLength = 0;
	SSPI_CREDENTIALS* credentials = context->credentials;
	/* Password contains a password hash of length (PasswordLength / SSPI_CREDENTIALS_HASH_LENGTH_FACTOR) */
	PasswordHashLength = credentials->identity.PasswordLength / SSPI_CREDENTIALS_HASH_LENGTH_FACTOR;
	status = ConvertFromUnicode(CP_UTF8, 0, (LPCWSTR) credentials->identity.Password,
								PasswordHashLength, &PasswordHash, 0, NULL, NULL);

	if (status <= 0)
		return -1;

	CharUpperBuffA(PasswordHash, PasswordHashLength);

	for (i = 0; i < 32; i += 2)
	{
		hn = PasswordHash[i] > '9' ? PasswordHash[i] - 'A' + 10 : PasswordHash[i] - '0';
		ln = PasswordHash[i + 1] > '9' ? PasswordHash[i + 1] - 'A' + 10 : PasswordHash[i + 1] - '0';
		hash[i / 2] = (hn << 4) | ln;
	}

	free(PasswordHash);
	return 1;
}

int ntlm_compute_ntlm_v2_hash(NTLM_CONTEXT* context, BYTE* hash)
{
	SSPI_CREDENTIALS* credentials = context->credentials;

	if (memcmp(context->NtlmV2Hash, NTLM_NULL_BUFFER, 16) != 0)
		return 1;

	if (memcmp(context->NtlmHash, NTLM_NULL_BUFFER, 16) != 0)
	{
		NTOWFv2FromHashW(context->NtlmHash,
				(LPWSTR) credentials->identity.User, credentials->identity.UserLength * 2,
				(LPWSTR) credentials->identity.Domain, credentials->identity.DomainLength * 2,
				(BYTE*) hash);
	}
	else if (credentials->identity.PasswordLength > 256)
	{
		/* Special case for WinPR: password hash */
		if (ntlm_convert_password_hash(context, context->NtlmHash) < 0)
			return -1;

		NTOWFv2FromHashW(context->NtlmHash,
				(LPWSTR) credentials->identity.User, credentials->identity.UserLength * 2,
				(LPWSTR) credentials->identity.Domain, credentials->identity.DomainLength * 2,
				(BYTE*) hash);
	}
	else if (credentials->identity.Password)
	{
		NTOWFv2W((LPWSTR) credentials->identity.Password, credentials->identity.PasswordLength * 2,
				(LPWSTR) credentials->identity.User, credentials->identity.UserLength * 2,
				(LPWSTR) credentials->identity.Domain, credentials->identity.DomainLength * 2, (BYTE*) hash);
	}
	else if (context->UseSamFileDatabase)
	{
		ntlm_fetch_ntlm_v2_hash(context, hash);
	}

	return 1;
}

int ntlm_compute_lm_v2_response(NTLM_CONTEXT* context)
{
	BYTE* response;
	BYTE value[16];

	if (context->LmCompatibilityLevel < 2)
	{
		if (!sspi_SecBufferAlloc(&context->LmChallengeResponse, 24))
			return -1;

		ZeroMemory(context->LmChallengeResponse.pvBuffer, 24);
		return 1;
	}

	/* Compute the NTLMv2 hash */

	if (ntlm_compute_ntlm_v2_hash(context, context->NtlmV2Hash) < 0)
		return -1;

	/* Concatenate the server and client challenges */
	CopyMemory(value, context->ServerChallenge, 8);
	CopyMemory(&value[8], context->ClientChallenge, 8);

	if (!sspi_SecBufferAlloc(&context->LmChallengeResponse, 24))
		return -1;

	response = (BYTE*) context->LmChallengeResponse.pvBuffer;
	/* Compute the HMAC-MD5 hash of the resulting value using the NTLMv2 hash as the key */
	winpr_HMAC(WINPR_MD_MD5, (void*) context->NtlmV2Hash, 16, (BYTE*) value, 16, (BYTE*) response);
	/* Concatenate the resulting HMAC-MD5 hash and the client challenge, giving us the LMv2 response (24 bytes) */
	CopyMemory(&response[16], context->ClientChallenge, 8);
	return 1;
}

/**
 * Compute NTLMv2 Response.
 * NTLMv2_RESPONSE @msdn{cc236653}
 * NTLMv2 Authentication @msdn{cc236700}
 * @param NTLM context
 */

int ntlm_compute_ntlm_v2_response(NTLM_CONTEXT* context)
{
	BYTE* blob;
	BYTE nt_proof_str[16];
	SecBuffer ntlm_v2_temp;
	SecBuffer ntlm_v2_temp_chal;
	PSecBuffer TargetInfo;
	SSPI_CREDENTIALS* credentials;
	credentials = context->credentials;
	TargetInfo = &context->ChallengeTargetInfo;

	if (!sspi_SecBufferAlloc(&ntlm_v2_temp, TargetInfo->cbBuffer + 28))
		return -1;

	ZeroMemory(ntlm_v2_temp.pvBuffer, ntlm_v2_temp.cbBuffer);
	blob = (BYTE*) ntlm_v2_temp.pvBuffer;

	/* Compute the NTLMv2 hash */

	if (ntlm_compute_ntlm_v2_hash(context, (BYTE*) context->NtlmV2Hash) < 0)
		return -1;

#ifdef WITH_DEBUG_NTLM
	WLog_DBG(TAG, "Password (length = %d)", credentials->identity.PasswordLength * 2);
	winpr_HexDump(TAG, WLOG_DEBUG, (BYTE*) credentials->identity.Password, credentials->identity.PasswordLength * 2);
	WLog_DBG(TAG, "Username (length = %d)", credentials->identity.UserLength * 2);
	winpr_HexDump(TAG, WLOG_DEBUG, (BYTE*) credentials->identity.User, credentials->identity.UserLength * 2);
	WLog_DBG(TAG, "Domain (length = %d)", credentials->identity.DomainLength * 2);
	winpr_HexDump(TAG, WLOG_DEBUG, (BYTE*) credentials->identity.Domain, credentials->identity.DomainLength * 2);
	WLog_DBG(TAG, "Workstation (length = %d)", context->Workstation.Length);
	winpr_HexDump(TAG, WLOG_DEBUG, (BYTE*) context->Workstation.Buffer, context->Workstation.Length);
	WLog_DBG(TAG, "NTOWFv2, NTLMv2 Hash");
	winpr_HexDump(TAG, WLOG_DEBUG, context->NtlmV2Hash, 16);
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
	WLog_DBG(TAG, "NTLMv2 Response Temp Blob");
	winpr_HexDump(TAG, WLOG_DEBUG, ntlm_v2_temp.pvBuffer, ntlm_v2_temp.cbBuffer);
#endif

	/* Concatenate server challenge with temp */

	if (!sspi_SecBufferAlloc(&ntlm_v2_temp_chal, ntlm_v2_temp.cbBuffer + 8))
		return -1;

	blob = (BYTE*) ntlm_v2_temp_chal.pvBuffer;
	CopyMemory(blob, context->ServerChallenge, 8);
	CopyMemory(&blob[8], ntlm_v2_temp.pvBuffer, ntlm_v2_temp.cbBuffer);
	winpr_HMAC(WINPR_MD_MD5, (BYTE*) context->NtlmV2Hash, 16, (BYTE*) ntlm_v2_temp_chal.pvBuffer,
		 ntlm_v2_temp_chal.cbBuffer, (BYTE*) nt_proof_str);

	/* NtChallengeResponse, Concatenate NTProofStr with temp */

	if (!sspi_SecBufferAlloc(&context->NtChallengeResponse, ntlm_v2_temp.cbBuffer + 16))
		return -1;

	blob = (BYTE*) context->NtChallengeResponse.pvBuffer;
	CopyMemory(blob, nt_proof_str, 16);
	CopyMemory(&blob[16], ntlm_v2_temp.pvBuffer, ntlm_v2_temp.cbBuffer);
	/* Compute SessionBaseKey, the HMAC-MD5 hash of NTProofStr using the NTLMv2 hash as the key */
	winpr_HMAC(WINPR_MD_MD5, (BYTE*) context->NtlmV2Hash, 16, (BYTE*) nt_proof_str, 16, (BYTE*) context->SessionBaseKey);
	sspi_SecBufferFree(&ntlm_v2_temp);
	sspi_SecBufferFree(&ntlm_v2_temp_chal);
	return 1;
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
	WINPR_RC4_CTX rc4;
	winpr_RC4_Init(&rc4, (void*) key, 16);
	winpr_RC4_Update(&rc4, length, (void*) plaintext, (void*) ciphertext);
	winpr_RC4_Final(&rc4);
}

/**
 * Generate client challenge (8-byte nonce).
 * @param NTLM context
 */

void ntlm_generate_client_challenge(NTLM_CONTEXT* context)
{
	/* ClientChallenge is used in computation of LMv2 and NTLMv2 responses */
	if (memcmp(context->ClientChallenge, NTLM_NULL_BUFFER, 8) == 0)
		winpr_RAND(context->ClientChallenge, 8);
}

/**
 * Generate server challenge (8-byte nonce).
 * @param NTLM context
 */

void ntlm_generate_server_challenge(NTLM_CONTEXT* context)
{
	if (memcmp(context->ServerChallenge, NTLM_NULL_BUFFER, 8) == 0)
		winpr_RAND(context->ServerChallenge, 8);
}

/**
 * Generate KeyExchangeKey (the 128-bit SessionBaseKey).
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
	winpr_RAND(context->RandomSessionKey, 16);
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

	/**
	 * 	if (NegotiateFlags & NTLMSSP_NEGOTIATE_KEY_EXCH)
	 * 		Set RandomSessionKey to RC4K(KeyExchangeKey, AUTHENTICATE_MESSAGE.EncryptedRandomSessionKey)
	 * 	else
	 * 		Set RandomSessionKey to KeyExchangeKey
	 */
	if (context->NegotiateKeyExchange)
		ntlm_rc4k(context->KeyExchangeKey, 16, context->EncryptedRandomSessionKey, context->RandomSessionKey);
	else
		CopyMemory(context->RandomSessionKey, context->KeyExchangeKey, 16);
}

/**
 * Generate signing key.
 * @msdn{cc236711}
 * @param exported_session_key ExportedSessionKey
 * @param sign_magic Sign magic string
 * @param signing_key Destination signing key
 */

int ntlm_generate_signing_key(BYTE* exported_session_key, PSecBuffer sign_magic, BYTE* signing_key)
{
	int length;
	BYTE* value;
	WINPR_MD5_CTX md5;

	length = 16 + sign_magic->cbBuffer;
	value = (BYTE*) malloc(length);

	if (!value)
		return -1;

	/* Concatenate ExportedSessionKey with sign magic */
	CopyMemory(value, exported_session_key, 16);
	CopyMemory(&value[16], sign_magic->pvBuffer, sign_magic->cbBuffer);
	winpr_MD5_Init(&md5);
	winpr_MD5_Update(&md5, value, length);
	winpr_MD5_Final(&md5, signing_key);
	free(value);
	return 1;
}

/**
 * Generate client signing key (ClientSigningKey).
 * @msdn{cc236711}
 * @param NTLM context
 */

void ntlm_generate_client_signing_key(NTLM_CONTEXT* context)
{
	SecBuffer signMagic;
	signMagic.pvBuffer = (void*) NTLM_CLIENT_SIGN_MAGIC;
	signMagic.cbBuffer = sizeof(NTLM_CLIENT_SIGN_MAGIC);
	ntlm_generate_signing_key(context->ExportedSessionKey, &signMagic, context->ClientSigningKey);
}

/**
 * Generate server signing key (ServerSigningKey).
 * @msdn{cc236711}
 * @param NTLM context
 */

void ntlm_generate_server_signing_key(NTLM_CONTEXT* context)
{
	SecBuffer signMagic;
	signMagic.pvBuffer = (void*) NTLM_SERVER_SIGN_MAGIC;
	signMagic.cbBuffer = sizeof(NTLM_SERVER_SIGN_MAGIC);
	ntlm_generate_signing_key(context->ExportedSessionKey, &signMagic, context->ServerSigningKey);
}

/**
 * Generate sealing key.
 * @msdn{cc236712}
 * @param exported_session_key ExportedSessionKey
 * @param seal_magic Seal magic string
 * @param sealing_key Destination sealing key
 */

int ntlm_generate_sealing_key(BYTE* exported_session_key, PSecBuffer seal_magic, BYTE* sealing_key)
{
	BYTE* p;
	WINPR_MD5_CTX md5;
	SecBuffer buffer;

	if (!sspi_SecBufferAlloc(&buffer, 16 + seal_magic->cbBuffer))
		return -1;

	p = (BYTE*) buffer.pvBuffer;
	/* Concatenate ExportedSessionKey with seal magic */
	CopyMemory(p, exported_session_key, 16);
	CopyMemory(&p[16], seal_magic->pvBuffer, seal_magic->cbBuffer);
	winpr_MD5_Init(&md5);
	winpr_MD5_Update(&md5, buffer.pvBuffer, buffer.cbBuffer);
	winpr_MD5_Final(&md5, sealing_key);
	sspi_SecBufferFree(&buffer);
	return 1;
}

/**
 * Generate client sealing key (ClientSealingKey).
 * @msdn{cc236712}
 * @param NTLM context
 */

void ntlm_generate_client_sealing_key(NTLM_CONTEXT* context)
{
	SecBuffer sealMagic;
	sealMagic.pvBuffer = (void*) NTLM_CLIENT_SEAL_MAGIC;
	sealMagic.cbBuffer = sizeof(NTLM_CLIENT_SEAL_MAGIC);
	ntlm_generate_signing_key(context->ExportedSessionKey, &sealMagic, context->ClientSealingKey);
}

/**
 * Generate server sealing key (ServerSealingKey).
 * @msdn{cc236712}
 * @param NTLM context
 */

void ntlm_generate_server_sealing_key(NTLM_CONTEXT* context)
{
	SecBuffer sealMagic;
	sealMagic.pvBuffer = (void*) NTLM_SERVER_SEAL_MAGIC;
	sealMagic.cbBuffer = sizeof(NTLM_SERVER_SEAL_MAGIC);
	ntlm_generate_signing_key(context->ExportedSessionKey, &sealMagic, context->ServerSealingKey);
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
		winpr_RC4_Init(&context->SendRc4Seal, context->ServerSealingKey, 16);
		winpr_RC4_Init(&context->RecvRc4Seal, context->ClientSealingKey, 16);
	}
	else
	{
		context->SendSigningKey = context->ClientSigningKey;
		context->RecvSigningKey = context->ServerSigningKey;
		context->SendSealingKey = context->ServerSealingKey;
		context->RecvSealingKey = context->ClientSealingKey;
		winpr_RC4_Init(&context->SendRc4Seal, context->ClientSealingKey, 16);
		winpr_RC4_Init(&context->RecvRc4Seal, context->ServerSealingKey, 16);
	}
}

void ntlm_compute_message_integrity_check(NTLM_CONTEXT* context)
{
	WINPR_HMAC_CTX hmac;
	/*
	 * Compute the HMAC-MD5 hash of ConcatenationOf(NEGOTIATE_MESSAGE,
	 * CHALLENGE_MESSAGE, AUTHENTICATE_MESSAGE) using the ExportedSessionKey
	 */

	winpr_HMAC_Init(&hmac, WINPR_MD_MD5, context->ExportedSessionKey, 16);
	winpr_HMAC_Update(&hmac, (BYTE*) context->NegotiateMessage.pvBuffer, context->NegotiateMessage.cbBuffer);
	winpr_HMAC_Update(&hmac, (BYTE*) context->ChallengeMessage.pvBuffer, context->ChallengeMessage.cbBuffer);
	winpr_HMAC_Update(&hmac, (BYTE*) context->AuthenticateMessage.pvBuffer, context->AuthenticateMessage.cbBuffer);
	winpr_HMAC_Final(&hmac, context->MessageIntegrityCheck);
}

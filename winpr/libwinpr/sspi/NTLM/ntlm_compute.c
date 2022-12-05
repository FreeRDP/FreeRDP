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

#include <winpr/config.h>

#include <winpr/assert.h>

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

static char NTLM_CLIENT_SIGN_MAGIC[] = "session key to client-to-server signing key magic constant";
static char NTLM_SERVER_SIGN_MAGIC[] = "session key to server-to-client signing key magic constant";
static char NTLM_CLIENT_SEAL_MAGIC[] = "session key to client-to-server sealing key magic constant";
static char NTLM_SERVER_SEAL_MAGIC[] = "session key to server-to-client sealing key magic constant";

static const BYTE NTLM_NULL_BUFFER[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	                                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/**
 * Populate VERSION structure msdn{cc236654}
 * @param versionInfo A pointer to the version struct
 *
 * @return \b TRUE for success, \b FALSE for failure
 */

BOOL ntlm_get_version_info(NTLM_VERSION_INFO* versionInfo)
{
	OSVERSIONINFOA osVersionInfo = { 0 };

	WINPR_ASSERT(versionInfo);

	osVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
	if (!GetVersionExA(&osVersionInfo))
		return FALSE;
	versionInfo->ProductMajorVersion = (UINT8)osVersionInfo.dwMajorVersion;
	versionInfo->ProductMinorVersion = (UINT8)osVersionInfo.dwMinorVersion;
	versionInfo->ProductBuild = (UINT16)osVersionInfo.dwBuildNumber;
	ZeroMemory(versionInfo->Reserved, sizeof(versionInfo->Reserved));
	versionInfo->NTLMRevisionCurrent = NTLMSSP_REVISION_W2K3;
	return TRUE;
}

/**
 * Read VERSION structure. msdn{cc236654}
 * @param s A pointer to a stream to read
 * @param versionInfo A pointer to the struct to read data to
 *
 * @return \b TRUE for success, \b FALSE for failure
 */

BOOL ntlm_read_version_info(wStream* s, NTLM_VERSION_INFO* versionInfo)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(versionInfo);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	Stream_Read_UINT8(s, versionInfo->ProductMajorVersion); /* ProductMajorVersion (1 byte) */
	Stream_Read_UINT8(s, versionInfo->ProductMinorVersion); /* ProductMinorVersion (1 byte) */
	Stream_Read_UINT16(s, versionInfo->ProductBuild);       /* ProductBuild (2 bytes) */
	Stream_Read(s, versionInfo->Reserved, sizeof(versionInfo->Reserved)); /* Reserved (3 bytes) */
	Stream_Read_UINT8(s, versionInfo->NTLMRevisionCurrent); /* NTLMRevisionCurrent (1 byte) */
	return TRUE;
}

/**
 * Write VERSION structure. msdn{cc236654}
 * @param s A pointer to the stream to write to
 * @param versionInfo A pointer to the buffer to read the data from
 *
 * @return \b TRUE for success, \b FALSE for failure
 */

BOOL ntlm_write_version_info(wStream* s, const NTLM_VERSION_INFO* versionInfo)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(versionInfo);

	if (Stream_GetRemainingCapacity(s) < 5 + sizeof(versionInfo->Reserved))
	{
		WLog_ERR(TAG, "NTLM_VERSION_INFO short header %" PRIuz ", expected %" PRIuz,
		         Stream_GetRemainingCapacity(s), 5 + sizeof(versionInfo->Reserved));
		return FALSE;
	}

	Stream_Write_UINT8(s, versionInfo->ProductMajorVersion); /* ProductMajorVersion (1 byte) */
	Stream_Write_UINT8(s, versionInfo->ProductMinorVersion); /* ProductMinorVersion (1 byte) */
	Stream_Write_UINT16(s, versionInfo->ProductBuild);       /* ProductBuild (2 bytes) */
	Stream_Write(s, versionInfo->Reserved, sizeof(versionInfo->Reserved)); /* Reserved (3 bytes) */
	Stream_Write_UINT8(s, versionInfo->NTLMRevisionCurrent); /* NTLMRevisionCurrent (1 byte) */
	return TRUE;
}

/**
 * Print VERSION structure. msdn{cc236654}
 * @param versionInfo A pointer to the struct containing the data to print
 */
#ifdef WITH_DEBUG_NTLM
void ntlm_print_version_info(const NTLM_VERSION_INFO* versionInfo)
{
	WINPR_ASSERT(versionInfo);

	WLog_VRB(TAG, "VERSION ={");
	WLog_VRB(TAG, "\tProductMajorVersion: %" PRIu8 "", versionInfo->ProductMajorVersion);
	WLog_VRB(TAG, "\tProductMinorVersion: %" PRIu8 "", versionInfo->ProductMinorVersion);
	WLog_VRB(TAG, "\tProductBuild: %" PRIu16 "", versionInfo->ProductBuild);
	WLog_VRB(TAG, "\tReserved: 0x%02" PRIX8 "%02" PRIX8 "%02" PRIX8 "", versionInfo->Reserved[0],
	         versionInfo->Reserved[1], versionInfo->Reserved[2]);
	WLog_VRB(TAG, "\tNTLMRevisionCurrent: 0x%02" PRIX8 "", versionInfo->NTLMRevisionCurrent);
}
#endif

static BOOL ntlm_read_ntlm_v2_client_challenge(wStream* s, NTLMv2_CLIENT_CHALLENGE* challenge)
{
	size_t size;
	WINPR_ASSERT(s);
	WINPR_ASSERT(challenge);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 28))
		return FALSE;

	Stream_Read_UINT8(s, challenge->RespType);
	Stream_Read_UINT8(s, challenge->HiRespType);
	Stream_Read_UINT16(s, challenge->Reserved1);
	Stream_Read_UINT32(s, challenge->Reserved2);
	Stream_Read(s, challenge->Timestamp, 8);
	Stream_Read(s, challenge->ClientChallenge, 8);
	Stream_Read_UINT32(s, challenge->Reserved3);
	size = Stream_Length(s) - Stream_GetPosition(s);

	if (size > UINT32_MAX)
	{
		WLog_ERR(TAG, "NTLMv2_CLIENT_CHALLENGE::cbAvPairs too large, got %" PRIuz "bytes", size);
		return FALSE;
	}

	challenge->cbAvPairs = (UINT32)size;
	challenge->AvPairs = (NTLM_AV_PAIR*)malloc(challenge->cbAvPairs);

	if (!challenge->AvPairs)
	{
		WLog_ERR(TAG, "NTLMv2_CLIENT_CHALLENGE::AvPairs failed to allocate %" PRIu32 "bytes",
		         challenge->cbAvPairs);
		return FALSE;
	}

	Stream_Read(s, challenge->AvPairs, size);
	return TRUE;
}

static BOOL ntlm_write_ntlm_v2_client_challenge(wStream* s,
                                                const NTLMv2_CLIENT_CHALLENGE* challenge)
{
	ULONG length;

	WINPR_ASSERT(s);
	WINPR_ASSERT(challenge);

	if (Stream_GetRemainingCapacity(s) < 28)
	{
		WLog_ERR(TAG, "NTLMv2_CLIENT_CHALLENGE expected 28bytes, have %" PRIuz "bytes",
		         Stream_GetRemainingCapacity(s));
		return FALSE;
	}
	Stream_Write_UINT8(s, challenge->RespType);
	Stream_Write_UINT8(s, challenge->HiRespType);
	Stream_Write_UINT16(s, challenge->Reserved1);
	Stream_Write_UINT32(s, challenge->Reserved2);
	Stream_Write(s, challenge->Timestamp, 8);
	Stream_Write(s, challenge->ClientChallenge, 8);
	Stream_Write_UINT32(s, challenge->Reserved3);
	length = ntlm_av_pair_list_length(challenge->AvPairs, challenge->cbAvPairs);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, length))
		return FALSE;

	Stream_Write(s, challenge->AvPairs, length);
	return TRUE;
}

BOOL ntlm_read_ntlm_v2_response(wStream* s, NTLMv2_RESPONSE* response)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(response);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 16))
		return FALSE;

	Stream_Read(s, response->Response, 16);
	return ntlm_read_ntlm_v2_client_challenge(s, &(response->Challenge));
}

BOOL ntlm_write_ntlm_v2_response(wStream* s, const NTLMv2_RESPONSE* response)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(response);

	if (Stream_GetRemainingCapacity(s) < 16)
	{
		WLog_ERR(TAG, "NTLMv2_RESPONSE expected 16bytes, have %" PRIuz "bytes",
		         Stream_GetRemainingCapacity(s));
		return FALSE;
	}
	Stream_Write(s, response->Response, 16);
	return ntlm_write_ntlm_v2_client_challenge(s, &(response->Challenge));
}

/**
 * Get current time, in tenths of microseconds since midnight of January 1, 1601.
 * @param[out] timestamp 64-bit little-endian timestamp
 */

void ntlm_current_time(BYTE* timestamp)
{
	FILETIME filetime = { 0 };
	ULARGE_INTEGER time64 = { 0 };

	WINPR_ASSERT(timestamp);

	GetSystemTimeAsFileTime(&filetime);
	time64.u.LowPart = filetime.dwLowDateTime;
	time64.u.HighPart = filetime.dwHighDateTime;
	CopyMemory(timestamp, &(time64.QuadPart), 8);
}

/**
 * Generate timestamp for AUTHENTICATE_MESSAGE.
 *
 * @param context A pointer to the NTLM context
 */

void ntlm_generate_timestamp(NTLM_CONTEXT* context)
{
	WINPR_ASSERT(context);

	if (memcmp(context->ChallengeTimestamp, NTLM_NULL_BUFFER, 8) != 0)
		CopyMemory(context->Timestamp, context->ChallengeTimestamp, 8);
	else
		ntlm_current_time(context->Timestamp);
}

static BOOL ntlm_fetch_ntlm_v2_hash(NTLM_CONTEXT* context, BYTE* hash)
{
	BOOL rc = FALSE;
	WINPR_SAM* sam = NULL;
	WINPR_SAM_ENTRY* entry = NULL;
	SSPI_CREDENTIALS* credentials;

	WINPR_ASSERT(context);
	WINPR_ASSERT(hash);

	credentials = context->credentials;
	sam = SamOpen(context->SamFile, TRUE);

	if (!sam)
		goto fail;

	entry = SamLookupUserW(
	    sam, (LPWSTR)credentials->identity.User, credentials->identity.UserLength * sizeof(WCHAR),
	    (LPWSTR)credentials->identity.Domain, credentials->identity.DomainLength * sizeof(WCHAR));

	if (!entry)
	{
		entry = SamLookupUserW(sam, (LPWSTR)credentials->identity.User,
		                       credentials->identity.UserLength * sizeof(WCHAR), NULL, 0);
	}

	if (!entry)
		goto fail;

#ifdef WITH_DEBUG_NTLM
	WLog_VRB(TAG, "NTLM Hash:");
	winpr_HexDump(TAG, WLOG_DEBUG, entry->NtHash, 16);
#endif
	NTOWFv2FromHashW(entry->NtHash, (LPWSTR)credentials->identity.User,
	                 credentials->identity.UserLength * sizeof(WCHAR),
	                 (LPWSTR)credentials->identity.Domain,
	                 credentials->identity.DomainLength * sizeof(WCHAR), (BYTE*)hash);

	rc = TRUE;

fail:
	SamFreeEntry(sam, entry);
	SamClose(sam);
	if (!rc)
		WLog_ERR(TAG, "Error: Could not find user in SAM database");

	return rc;
}

static int ntlm_convert_password_hash(NTLM_CONTEXT* context, BYTE* hash)
{
	char PasswordHash[32] = { 0 };
	INT64 PasswordHashLength = 0;
	SSPI_CREDENTIALS* credentials = NULL;

	WINPR_ASSERT(context);
	WINPR_ASSERT(hash);

	credentials = context->credentials;
	/* Password contains a password hash of length (PasswordLength -
	 * SSPI_CREDENTIALS_HASH_LENGTH_OFFSET) */
	PasswordHashLength = credentials->identity.PasswordLength - SSPI_CREDENTIALS_HASH_LENGTH_OFFSET;

	WINPR_ASSERT(PasswordHashLength >= 0);
	WINPR_ASSERT((size_t)PasswordHashLength < ARRAYSIZE(PasswordHash));
	if (ConvertWCharNToUtf8(credentials->identity.Password, PasswordHashLength, PasswordHash,
	                        ARRAYSIZE(PasswordHash)) <= 0)
		return -1;

	CharUpperBuffA(PasswordHash, (DWORD)PasswordHashLength);

	for (size_t i = 0; i < ARRAYSIZE(PasswordHash); i += 2)
	{
		BYTE hn =
		    (BYTE)(PasswordHash[i] > '9' ? PasswordHash[i] - 'A' + 10 : PasswordHash[i] - '0');
		BYTE ln = (BYTE)(PasswordHash[i + 1] > '9' ? PasswordHash[i + 1] - 'A' + 10
		                                           : PasswordHash[i + 1] - '0');
		hash[i / 2] = (BYTE)((hn << 4) | ln);
	}

	return 1;
}

static BOOL ntlm_compute_ntlm_v2_hash(NTLM_CONTEXT* context, BYTE* hash)
{
	SSPI_CREDENTIALS* credentials;

	WINPR_ASSERT(context);
	WINPR_ASSERT(hash);

	credentials = context->credentials;
#ifdef WITH_DEBUG_NTLM

	if (credentials)
	{
		WLog_VRB(TAG, "Password (length = %" PRIu32 ")", credentials->identity.PasswordLength * 2);
		winpr_HexDump(TAG, WLOG_TRACE, (BYTE*)credentials->identity.Password,
		              credentials->identity.PasswordLength * 2);
		WLog_VRB(TAG, "Username (length = %" PRIu32 ")", credentials->identity.UserLength * 2);
		winpr_HexDump(TAG, WLOG_TRACE, (BYTE*)credentials->identity.User,
		              credentials->identity.UserLength * 2);
		WLog_VRB(TAG, "Domain (length = %" PRIu32 ")", credentials->identity.DomainLength * 2);
		winpr_HexDump(TAG, WLOG_TRACE, (BYTE*)credentials->identity.Domain,
		              credentials->identity.DomainLength * 2);
	}
	else
		WLog_VRB(TAG, "Strange, NTLM_CONTEXT is missing valid credentials...");

	WLog_VRB(TAG, "Workstation (length = %" PRIu16 ")", context->Workstation.Length);
	winpr_HexDump(TAG, WLOG_TRACE, (BYTE*)context->Workstation.Buffer, context->Workstation.Length);
	WLog_VRB(TAG, "NTOWFv2, NTLMv2 Hash");
	winpr_HexDump(TAG, WLOG_TRACE, context->NtlmV2Hash, WINPR_MD5_DIGEST_LENGTH);
#endif

	if (memcmp(context->NtlmV2Hash, NTLM_NULL_BUFFER, 16) != 0)
		return TRUE;

	if (!credentials)
		return FALSE;
	else if (memcmp(context->NtlmHash, NTLM_NULL_BUFFER, 16) != 0)
	{
		NTOWFv2FromHashW(context->NtlmHash, (LPWSTR)credentials->identity.User,
		                 credentials->identity.UserLength * 2, (LPWSTR)credentials->identity.Domain,
		                 credentials->identity.DomainLength * 2, (BYTE*)hash);
	}
	else if (credentials->identity.PasswordLength > SSPI_CREDENTIALS_HASH_LENGTH_OFFSET)
	{
		/* Special case for WinPR: password hash */
		if (ntlm_convert_password_hash(context, context->NtlmHash) < 0)
			return FALSE;

		NTOWFv2FromHashW(context->NtlmHash, (LPWSTR)credentials->identity.User,
		                 credentials->identity.UserLength * 2, (LPWSTR)credentials->identity.Domain,
		                 credentials->identity.DomainLength * 2, (BYTE*)hash);
	}
	else if (credentials->identity.Password)
	{
		NTOWFv2W((LPWSTR)credentials->identity.Password, credentials->identity.PasswordLength * 2,
		         (LPWSTR)credentials->identity.User, credentials->identity.UserLength * 2,
		         (LPWSTR)credentials->identity.Domain, credentials->identity.DomainLength * 2,
		         (BYTE*)hash);
	}
	else if (context->HashCallback)
	{
		int ret;
		SecBuffer proofValue, micValue;

		if (ntlm_computeProofValue(context, &proofValue) != SEC_E_OK)
			return FALSE;

		if (ntlm_computeMicValue(context, &micValue) != SEC_E_OK)
		{
			sspi_SecBufferFree(&proofValue);
			return FALSE;
		}

		ret = context->HashCallback(context->HashCallbackArg, &credentials->identity, &proofValue,
		                            context->EncryptedRandomSessionKey,
		                            context->AUTHENTICATE_MESSAGE.MessageIntegrityCheck, &micValue,
		                            hash);
		sspi_SecBufferFree(&proofValue);
		sspi_SecBufferFree(&micValue);
		return ret ? TRUE : FALSE;
	}
	else if (context->UseSamFileDatabase)
	{
		return ntlm_fetch_ntlm_v2_hash(context, hash);
	}

	return TRUE;
}

BOOL ntlm_compute_lm_v2_response(NTLM_CONTEXT* context)
{
	BYTE* response;
	BYTE value[WINPR_MD5_DIGEST_LENGTH] = { 0 };

	WINPR_ASSERT(context);

	if (context->LmCompatibilityLevel < 2)
	{
		if (!sspi_SecBufferAlloc(&context->LmChallengeResponse, 24))
			return FALSE;

		ZeroMemory(context->LmChallengeResponse.pvBuffer, 24);
		return TRUE;
	}

	/* Compute the NTLMv2 hash */

	if (!ntlm_compute_ntlm_v2_hash(context, context->NtlmV2Hash))
		return FALSE;

	/* Concatenate the server and client challenges */
	CopyMemory(value, context->ServerChallenge, 8);
	CopyMemory(&value[8], context->ClientChallenge, 8);

	if (!sspi_SecBufferAlloc(&context->LmChallengeResponse, 24))
		return FALSE;

	response = (BYTE*)context->LmChallengeResponse.pvBuffer;
	/* Compute the HMAC-MD5 hash of the resulting value using the NTLMv2 hash as the key */
	winpr_HMAC(WINPR_MD_MD5, (void*)context->NtlmV2Hash, WINPR_MD5_DIGEST_LENGTH, (BYTE*)value,
	           WINPR_MD5_DIGEST_LENGTH, (BYTE*)response, WINPR_MD5_DIGEST_LENGTH);
	/* Concatenate the resulting HMAC-MD5 hash and the client challenge, giving us the LMv2 response
	 * (24 bytes) */
	CopyMemory(&response[16], context->ClientChallenge, 8);
	return TRUE;
}

/**
 * Compute NTLMv2 Response.
 *
 * NTLMv2_RESPONSE msdn{cc236653}
 * NTLMv2 Authentication msdn{cc236700}
 *
 * @param context A pointer to the NTLM context
 * @return \b TRUE for success, \b FALSE for failure
 */

BOOL ntlm_compute_ntlm_v2_response(NTLM_CONTEXT* context)
{
	BYTE* blob;
	SecBuffer ntlm_v2_temp = { 0 };
	SecBuffer ntlm_v2_temp_chal = { 0 };
	PSecBuffer TargetInfo;

	WINPR_ASSERT(context);

	TargetInfo = &context->ChallengeTargetInfo;
	BOOL ret = FALSE;

	if (!sspi_SecBufferAlloc(&ntlm_v2_temp, TargetInfo->cbBuffer + 28))
		goto exit;

	ZeroMemory(ntlm_v2_temp.pvBuffer, ntlm_v2_temp.cbBuffer);
	blob = (BYTE*)ntlm_v2_temp.pvBuffer;

	/* Compute the NTLMv2 hash */
	if (!ntlm_compute_ntlm_v2_hash(context, (BYTE*)context->NtlmV2Hash))
		goto exit;

	/* Construct temp */
	blob[0] = 1; /* RespType (1 byte) */
	blob[1] = 1; /* HighRespType (1 byte) */
	/* Reserved1 (2 bytes) */
	/* Reserved2 (4 bytes) */
	CopyMemory(&blob[8], context->Timestamp, 8);        /* Timestamp (8 bytes) */
	CopyMemory(&blob[16], context->ClientChallenge, 8); /* ClientChallenge (8 bytes) */
	/* Reserved3 (4 bytes) */
	CopyMemory(&blob[28], TargetInfo->pvBuffer, TargetInfo->cbBuffer);
#ifdef WITH_DEBUG_NTLM
	WLog_VRB(TAG, "NTLMv2 Response Temp Blob");
	winpr_HexDump(TAG, WLOG_TRACE, ntlm_v2_temp.pvBuffer, ntlm_v2_temp.cbBuffer);
#endif

	/* Concatenate server challenge with temp */

	if (!sspi_SecBufferAlloc(&ntlm_v2_temp_chal, ntlm_v2_temp.cbBuffer + 8))
		goto exit;

	blob = (BYTE*)ntlm_v2_temp_chal.pvBuffer;
	CopyMemory(blob, context->ServerChallenge, 8);
	CopyMemory(&blob[8], ntlm_v2_temp.pvBuffer, ntlm_v2_temp.cbBuffer);
	winpr_HMAC(WINPR_MD_MD5, (BYTE*)context->NtlmV2Hash, WINPR_MD5_DIGEST_LENGTH,
	           (BYTE*)ntlm_v2_temp_chal.pvBuffer, ntlm_v2_temp_chal.cbBuffer,
	           context->NtProofString, WINPR_MD5_DIGEST_LENGTH);

	/* NtChallengeResponse, Concatenate NTProofStr with temp */

	if (!sspi_SecBufferAlloc(&context->NtChallengeResponse, ntlm_v2_temp.cbBuffer + 16))
		goto exit;

	blob = (BYTE*)context->NtChallengeResponse.pvBuffer;
	CopyMemory(blob, context->NtProofString, WINPR_MD5_DIGEST_LENGTH);
	CopyMemory(&blob[16], ntlm_v2_temp.pvBuffer, ntlm_v2_temp.cbBuffer);
	/* Compute SessionBaseKey, the HMAC-MD5 hash of NTProofStr using the NTLMv2 hash as the key */
	winpr_HMAC(WINPR_MD_MD5, (BYTE*)context->NtlmV2Hash, WINPR_MD5_DIGEST_LENGTH,
	           context->NtProofString, WINPR_MD5_DIGEST_LENGTH, context->SessionBaseKey,
	           WINPR_MD5_DIGEST_LENGTH);
	ret = TRUE;
exit:
	sspi_SecBufferFree(&ntlm_v2_temp);
	sspi_SecBufferFree(&ntlm_v2_temp_chal);
	return ret;
}

/**
 * Encrypt the given plain text using RC4 and the given key.
 * @param key RC4 key
 * @param length text length
 * @param plaintext plain text
 * @param ciphertext cipher text
 */

void ntlm_rc4k(BYTE* key, size_t length, BYTE* plaintext, BYTE* ciphertext)
{
	WINPR_RC4_CTX* rc4 = winpr_RC4_New(key, 16);

	if (rc4)
	{
		winpr_RC4_Update(rc4, length, plaintext, ciphertext);
		winpr_RC4_Free(rc4);
	}
}

/**
 * Generate client challenge (8-byte nonce).
 * @param context A pointer to the NTLM context
 */

void ntlm_generate_client_challenge(NTLM_CONTEXT* context)
{
	WINPR_ASSERT(context);

	/* ClientChallenge is used in computation of LMv2 and NTLMv2 responses */
	if (memcmp(context->ClientChallenge, NTLM_NULL_BUFFER, sizeof(context->ClientChallenge)) == 0)
		winpr_RAND(context->ClientChallenge, sizeof(context->ClientChallenge));
}

/**
 * Generate server challenge (8-byte nonce).
 * @param context A pointer to the NTLM context
 */

void ntlm_generate_server_challenge(NTLM_CONTEXT* context)
{
	WINPR_ASSERT(context);

	if (memcmp(context->ServerChallenge, NTLM_NULL_BUFFER, sizeof(context->ServerChallenge)) == 0)
		winpr_RAND(context->ServerChallenge, sizeof(context->ServerChallenge));
}

/**
 * Generate KeyExchangeKey (the 128-bit SessionBaseKey). msdn{cc236710}
 * @param context A pointer to the NTLM context
 */

void ntlm_generate_key_exchange_key(NTLM_CONTEXT* context)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(sizeof(context->KeyExchangeKey) == sizeof(context->SessionBaseKey));

	/* In NTLMv2, KeyExchangeKey is the 128-bit SessionBaseKey */
	CopyMemory(context->KeyExchangeKey, context->SessionBaseKey, sizeof(context->KeyExchangeKey));
}

/**
 * Generate RandomSessionKey (16-byte nonce).
 * @param context A pointer to the NTLM context
 */

void ntlm_generate_random_session_key(NTLM_CONTEXT* context)
{
	WINPR_ASSERT(context);
	winpr_RAND(context->RandomSessionKey, sizeof(context->RandomSessionKey));
}

/**
 * Generate ExportedSessionKey (the RandomSessionKey, exported)
 * @param context A pointer to the NTLM context
 */

void ntlm_generate_exported_session_key(NTLM_CONTEXT* context)
{
	WINPR_ASSERT(context);

	CopyMemory(context->ExportedSessionKey, context->RandomSessionKey,
	           sizeof(context->ExportedSessionKey));
}

/**
 * Encrypt RandomSessionKey (RC4-encrypted RandomSessionKey, using KeyExchangeKey as the key).
 * @param context A pointer to the NTLM context
 */

void ntlm_encrypt_random_session_key(NTLM_CONTEXT* context)
{
	/* In NTLMv2, EncryptedRandomSessionKey is the ExportedSessionKey RC4-encrypted with the
	 * KeyExchangeKey */
	WINPR_ASSERT(context);
	ntlm_rc4k(context->KeyExchangeKey, 16, context->RandomSessionKey,
	          context->EncryptedRandomSessionKey);
}

/**
 * Decrypt RandomSessionKey (RC4-encrypted RandomSessionKey, using KeyExchangeKey as the key).
 * @param context A pointer to the NTLM context
 */

void ntlm_decrypt_random_session_key(NTLM_CONTEXT* context)
{
	WINPR_ASSERT(context);

	/* In NTLMv2, EncryptedRandomSessionKey is the ExportedSessionKey RC4-encrypted with the
	 * KeyExchangeKey */

	/**
	 * 	if (NegotiateFlags & NTLMSSP_NEGOTIATE_KEY_EXCH)
	 * 		Set RandomSessionKey to RC4K(KeyExchangeKey,
	 * AUTHENTICATE_MESSAGE.EncryptedRandomSessionKey) else Set RandomSessionKey to KeyExchangeKey
	 */
	if (context->NegotiateKeyExchange)
	{
		WINPR_ASSERT(sizeof(context->EncryptedRandomSessionKey) ==
		             sizeof(context->RandomSessionKey));
		ntlm_rc4k(context->KeyExchangeKey, sizeof(context->EncryptedRandomSessionKey),
		          context->EncryptedRandomSessionKey, context->RandomSessionKey);
	}
	else
	{
		WINPR_ASSERT(sizeof(context->RandomSessionKey) == sizeof(context->KeyExchangeKey));
		CopyMemory(context->RandomSessionKey, context->KeyExchangeKey,
		           sizeof(context->RandomSessionKey));
	}
}

/**
 * Generate signing key msdn{cc236711}
 *
 * @param exported_session_key ExportedSessionKey
 * @param sign_magic Sign magic string
 * @param signing_key Destination signing key
 *
 * @return \b TRUE for success, \b FALSE for failure
 */

static BOOL ntlm_generate_signing_key(BYTE* exported_session_key, const SecBuffer* sign_magic,
                                      BYTE* signing_key)
{
	BOOL rc = FALSE;
	size_t length;
	BYTE* value = NULL;

	WINPR_ASSERT(exported_session_key);
	WINPR_ASSERT(sign_magic);
	WINPR_ASSERT(signing_key);

	length = WINPR_MD5_DIGEST_LENGTH + sign_magic->cbBuffer;
	value = (BYTE*)malloc(length);

	if (!value)
		goto out;

	/* Concatenate ExportedSessionKey with sign magic */
	CopyMemory(value, exported_session_key, WINPR_MD5_DIGEST_LENGTH);
	CopyMemory(&value[WINPR_MD5_DIGEST_LENGTH], sign_magic->pvBuffer, sign_magic->cbBuffer);

	rc = winpr_Digest(WINPR_MD_MD5, value, length, signing_key, WINPR_MD5_DIGEST_LENGTH);

out:
	free(value);
	return rc;
}

/**
 * Generate client signing key (ClientSigningKey). msdn{cc236711}
 * @param context A pointer to the NTLM context
 *
 * @return \b TRUE for success, \b FALSE for failure
 */

BOOL ntlm_generate_client_signing_key(NTLM_CONTEXT* context)
{
	const SecBuffer signMagic = { sizeof(NTLM_CLIENT_SIGN_MAGIC), 0, NTLM_CLIENT_SIGN_MAGIC };

	WINPR_ASSERT(context);
	return ntlm_generate_signing_key(context->ExportedSessionKey, &signMagic,
	                                 context->ClientSigningKey);
}

/**
 * Generate server signing key (ServerSigningKey). msdn{cc236711}
 * @param context A pointer to the NTLM context
 *
 * @return \b TRUE for success, \b FALSE for failure
 */

BOOL ntlm_generate_server_signing_key(NTLM_CONTEXT* context)
{
	const SecBuffer signMagic = { sizeof(NTLM_SERVER_SIGN_MAGIC), 0, NTLM_SERVER_SIGN_MAGIC };

	WINPR_ASSERT(context);
	return ntlm_generate_signing_key(context->ExportedSessionKey, &signMagic,
	                                 context->ServerSigningKey);
}

/**
 * Generate client sealing key (ClientSealingKey). msdn{cc236712}
 * @param context A pointer to the NTLM context
 *
 * @return \b TRUE for success, \b FALSE for failure
 */

BOOL ntlm_generate_client_sealing_key(NTLM_CONTEXT* context)
{
	const SecBuffer sealMagic = { sizeof(NTLM_CLIENT_SEAL_MAGIC), 0, NTLM_CLIENT_SEAL_MAGIC };

	WINPR_ASSERT(context);
	return ntlm_generate_signing_key(context->ExportedSessionKey, &sealMagic,
	                                 context->ClientSealingKey);
}

/**
 * Generate server sealing key (ServerSealingKey). msdn{cc236712}
 * @param context A pointer to the NTLM context
 *
 * @return \b TRUE for success, \b FALSE for failure
 */

BOOL ntlm_generate_server_sealing_key(NTLM_CONTEXT* context)
{
	const SecBuffer sealMagic = { sizeof(NTLM_SERVER_SEAL_MAGIC), 0, NTLM_SERVER_SEAL_MAGIC };

	WINPR_ASSERT(context);
	return ntlm_generate_signing_key(context->ExportedSessionKey, &sealMagic,
	                                 context->ServerSealingKey);
}

/**
 * Initialize RC4 stream cipher states for sealing.
 * @param context A pointer to the NTLM context
 */

void ntlm_init_rc4_seal_states(NTLM_CONTEXT* context)
{
	WINPR_ASSERT(context);
	if (context->server)
	{
		context->SendSigningKey = context->ServerSigningKey;
		context->RecvSigningKey = context->ClientSigningKey;
		context->SendSealingKey = context->ClientSealingKey;
		context->RecvSealingKey = context->ServerSealingKey;
		context->SendRc4Seal =
		    winpr_RC4_New(context->ServerSealingKey, sizeof(context->ServerSealingKey));
		context->RecvRc4Seal =
		    winpr_RC4_New(context->ClientSealingKey, sizeof(context->ClientSealingKey));
	}
	else
	{
		context->SendSigningKey = context->ClientSigningKey;
		context->RecvSigningKey = context->ServerSigningKey;
		context->SendSealingKey = context->ServerSealingKey;
		context->RecvSealingKey = context->ClientSealingKey;
		context->SendRc4Seal =
		    winpr_RC4_New(context->ClientSealingKey, sizeof(context->ClientSealingKey));
		context->RecvRc4Seal =
		    winpr_RC4_New(context->ServerSealingKey, sizeof(context->ServerSealingKey));
	}
}

BOOL ntlm_compute_message_integrity_check(NTLM_CONTEXT* context, BYTE* mic, UINT32 size)
{
	BOOL rc = FALSE;
	/*
	 * Compute the HMAC-MD5 hash of ConcatenationOf(NEGOTIATE_MESSAGE,
	 * CHALLENGE_MESSAGE, AUTHENTICATE_MESSAGE) using the ExportedSessionKey
	 */
	WINPR_HMAC_CTX* hmac = winpr_HMAC_New();

	WINPR_ASSERT(context);
	WINPR_ASSERT(mic);
	WINPR_ASSERT(size >= WINPR_MD5_DIGEST_LENGTH);

	memset(mic, 0, size);
	if (!hmac)
		return FALSE;

	if (winpr_HMAC_Init(hmac, WINPR_MD_MD5, context->ExportedSessionKey, WINPR_MD5_DIGEST_LENGTH))
	{
		winpr_HMAC_Update(hmac, (BYTE*)context->NegotiateMessage.pvBuffer,
		                  context->NegotiateMessage.cbBuffer);
		winpr_HMAC_Update(hmac, (BYTE*)context->ChallengeMessage.pvBuffer,
		                  context->ChallengeMessage.cbBuffer);

		if (context->MessageIntegrityCheckOffset > 0)
		{
			const BYTE* auth = (BYTE*)context->AuthenticateMessage.pvBuffer;
			const BYTE data[WINPR_MD5_DIGEST_LENGTH] = { 0 };
			const size_t rest = context->MessageIntegrityCheckOffset + sizeof(data);

			WINPR_ASSERT(rest <= context->AuthenticateMessage.cbBuffer);
			winpr_HMAC_Update(hmac, &auth[0], context->MessageIntegrityCheckOffset);
			winpr_HMAC_Update(hmac, data, sizeof(data));
			winpr_HMAC_Update(hmac, &auth[rest], context->AuthenticateMessage.cbBuffer - rest);
		}
		else
		{
			winpr_HMAC_Update(hmac, (BYTE*)context->AuthenticateMessage.pvBuffer,
			                  context->AuthenticateMessage.cbBuffer);
		}
		winpr_HMAC_Final(hmac, mic, WINPR_MD5_DIGEST_LENGTH);
		rc = TRUE;
	}

	winpr_HMAC_Free(hmac);
	return rc;
}

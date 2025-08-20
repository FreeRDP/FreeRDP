/**
 * WinPR: Windows Portable Runtime
 * Credentials Management
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2025 David Fort <contact@hardening-consulting.com>
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
#include <winpr/crt.h>
#include <winpr/cred.h>
#include <winpr/error.h>
#include <winpr/wlog.h>
#include "../log.h"

#define TAG WINPR_TAG("Cred")

/*
 * Low-Level Credentials Management Functions:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa374731(v=vs.85).aspx#low_level_credentials_management_functions
 */

#ifndef _WIN32

static BYTE char_decode(char c)
{
	if (c >= 'A' && c <= 'Z')
		return (BYTE)(c - 'A');
	if (c >= 'a' && c <= 'z')
		return (BYTE)(c - 'a' + 26);
	if (c >= '0' && c <= '9')
		return (BYTE)(c - '0' + 52);
	if (c == '#')
		return 62;
	if (c == '-')
		return 63;
	return 64;
}

static BOOL cred_decode(const char* cred, size_t len, BYTE* buf)
{
	size_t i = 0;
	const char* p = cred;

	while (len >= 4)
	{
		BYTE c0 = char_decode(p[0]);
		if (c0 > 63)
			return FALSE;

		BYTE c1 = char_decode(p[1]);
		if (c1 > 63)
			return FALSE;

		BYTE c2 = char_decode(p[2]);
		if (c2 > 63)
			return FALSE;

		BYTE c3 = char_decode(p[3]);
		if (c3 > 63)
			return FALSE;

		buf[i + 0] = (BYTE)((c1 << 6) | c0);
		buf[i + 1] = (BYTE)((c2 << 4) | (c1 >> 2));
		buf[i + 2] = (BYTE)((c3 << 2) | (c2 >> 4));
		len -= 4;
		i += 3;
		p += 4;
	}

	if (len == 3)
	{
		BYTE c0 = char_decode(p[0]);
		if (c0 > 63)
			return FALSE;

		BYTE c1 = char_decode(p[1]);
		if (c1 > 63)
			return FALSE;

		BYTE c2 = char_decode(p[2]);
		if (c2 > 63)
			return FALSE;

		buf[i + 0] = (BYTE)((c1 << 6) | c0);
		buf[i + 1] = (BYTE)((c2 << 4) | (c1 >> 2));
	}
	else if (len == 2)
	{
		BYTE c0 = char_decode(p[0]);
		if (c0 > 63)
			return FALSE;

		BYTE c1 = char_decode(p[1]);
		if (c1 > 63)
			return FALSE;

		buf[i + 0] = (BYTE)((c1 << 6) | c0);
	}
	else if (len == 1)
	{
		WLog_ERR(TAG, "invalid string length");
		return FALSE;
	}
	return TRUE;
}

static size_t cred_encode(const BYTE* bin, size_t len, char* cred, size_t credlen)
{
	static const char encodingChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	                                    "abcdefghijklmnopqrstuvwxyz"
	                                    "0123456789"
	                                    "#-";
	size_t n = 0;
	size_t offset = 0;
	while (offset < len)
	{
		if (n >= credlen)
			break;

		cred[n++] = encodingChars[bin[offset] & 0x3f];
		BYTE x = (bin[offset] & 0xc0) >> 6;
		offset++;

		if (offset >= len)
		{
			cred[n++] = encodingChars[x];
			break;
		}

		if (n >= credlen)
			break;
		cred[n++] = encodingChars[((bin[offset] & 0xf) << 2) | x];
		x = (bin[offset] & 0xf0) >> 4;
		offset++;

		if (offset >= len)
		{
			cred[n++] = encodingChars[x];
			break;
		}

		if (n >= credlen)
			break;
		cred[n++] = encodingChars[((bin[offset] & 0x3) << 4) | x];

		if (n >= credlen)
			break;
		cred[n++] = encodingChars[(bin[offset] & 0xfc) >> 2];
		offset++;
	}
	return n;
}

BOOL CredMarshalCredentialW(CRED_MARSHAL_TYPE CredType, PVOID Credential,
                            LPWSTR* MarshaledCredential)
{
	char* b = NULL;
	if (!CredMarshalCredentialA(CredType, Credential, &b) || !b)
		return FALSE;

	*MarshaledCredential = ConvertUtf8ToWCharAlloc(b, NULL);
	free(b);
	return (*MarshaledCredential != NULL);
}

BOOL CredMarshalCredentialA(CRED_MARSHAL_TYPE CredType, PVOID Credential,
                            LPSTR* MarshaledCredential)
{
	CERT_CREDENTIAL_INFO* cert = Credential;

	if (!cert || ((CredType == CertCredential) && (cert->cbSize < sizeof(CERT_CREDENTIAL_INFO))) ||
	    !MarshaledCredential)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	switch (CredType)
	{
		case CertCredential:
		{
			char buffer[3ULL + (sizeof(cert->rgbHashOfCert) * 4 / 3) +
			            1ULL /* rounding error */] = { 0 };

			const char c = WINPR_ASSERTING_INT_CAST(char, 'A' + CredType);
			(void)_snprintf(buffer, sizeof(buffer), "@@%c", c);
			size_t len = cred_encode(cert->rgbHashOfCert, sizeof(cert->rgbHashOfCert), &buffer[3],
			                         sizeof(buffer) - 3);
			*MarshaledCredential = strndup(buffer, len + 3);
			return TRUE;
		}
		default:
			WLog_ERR(TAG, "unhandled type 0x%x", CredType);
			SetLastError(ERROR_INVALID_PARAMETER);
			return FALSE;
	}
}

BOOL CredUnmarshalCredentialW(LPCWSTR cred, PCRED_MARSHAL_TYPE pcredType, PVOID* out)
{
	char* str = NULL;
	if (cred)
		str = ConvertWCharToUtf8Alloc(cred, NULL);
	const BOOL rc = CredUnmarshalCredentialA(str, pcredType, out);
	free(str);
	return rc;
}

BOOL CredUnmarshalCredentialA(LPCSTR cred, PCRED_MARSHAL_TYPE CredType, PVOID* Credential)
{
	if (!cred)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	const size_t len = strlen(cred);
	if ((len < 3) || !CredType || !Credential || (cred[0] != '@') || (cred[1] != '@'))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	BYTE b = char_decode(cred[2]);
	if (!b || (b > BinaryBlobForSystem))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	*CredType = (CRED_MARSHAL_TYPE)b;

	switch (*CredType)
	{
		case CertCredential:
		{
			BYTE hash[CERT_HASH_LENGTH] = { 0 };

			if ((len != 30) || !cred_decode(&cred[3], len - 3, hash))
			{
				SetLastError(ERROR_INVALID_PARAMETER);
				return FALSE;
			}

			CERT_CREDENTIAL_INFO* cert = calloc(1, sizeof(CERT_CREDENTIAL_INFO));
			if (!cert)
				return FALSE;

			cert->cbSize = sizeof(CERT_CREDENTIAL_INFO);
			memcpy(cert->rgbHashOfCert, hash, sizeof(cert->rgbHashOfCert));
			*Credential = cert;
			break;
		}
		default:
			WLog_ERR(TAG, "unhandled credType 0x%x", *CredType);
			SetLastError(ERROR_INVALID_PARAMETER);
			return FALSE;
	}
	return TRUE;
}

BOOL CredIsMarshaledCredentialW(LPCWSTR MarshaledCredential)
{
	CRED_MARSHAL_TYPE t = BinaryBlobForSystem;
	void* out = NULL;

	BOOL ret = CredUnmarshalCredentialW(MarshaledCredential, &t, &out);
	if (out)
		CredFree(out);

	return ret;
}

BOOL CredIsMarshaledCredentialA(LPCSTR MarshaledCredential)
{
	CRED_MARSHAL_TYPE t = BinaryBlobForSystem;
	void* out = NULL;
	BOOL ret = CredUnmarshalCredentialA(MarshaledCredential, &t, &out);
	if (out)
		CredFree(out);

	return ret;
}

VOID CredFree(PVOID Buffer)
{
	free(Buffer);
}

#endif /* _WIN32 */

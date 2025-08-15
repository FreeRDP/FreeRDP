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

static BYTE wchar_decode(WCHAR c)
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

static BOOL cred_decode(const WCHAR* cred, size_t len, BYTE* buf)
{
	size_t i = 0;
	const WCHAR* p = cred;

	while (len >= 4)
	{
		BYTE c0 = wchar_decode(p[0]);
		if (c0 > 63)
			return FALSE;

		BYTE c1 = wchar_decode(p[1]);
		if (c1 > 63)
			return FALSE;

		BYTE c2 = wchar_decode(p[2]);
		if (c2 > 63)
			return FALSE;

		BYTE c3 = wchar_decode(p[3]);
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
		BYTE c0 = wchar_decode(p[0]);
		if (c0 > 63)
			return FALSE;

		BYTE c1 = wchar_decode(p[1]);
		if (c1 > 63)
			return FALSE;

		BYTE c2 = wchar_decode(p[2]);
		if (c2 > 63)
			return FALSE;

		buf[i + 0] = (BYTE)((c1 << 6) | c0);
		buf[i + 1] = (BYTE)((c2 << 4) | (c1 >> 2));
	}
	else if (len == 2)
	{
		BYTE c0 = wchar_decode(p[0]);
		if (c0 > 63)
			return FALSE;

		BYTE c1 = wchar_decode(p[1]);
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

static size_t cred_encode(const BYTE* bin, size_t len, WCHAR* cred)
{
	static const WCHAR encodingChars[] = {
		/* ABCDEFGHIJKLMNOPQRSTUVWXYZ */
		0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
		0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A,
		/* abcdefghijklmnopqrstuvwxyz */
		0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
		0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A,
		/* 0123456789 */
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
		/* #- */
		0x23, 0x2d
	};
	size_t n = 0;

	while (len > 0)
	{
		cred[n++] = encodingChars[bin[0] & 0x3f];
		BYTE x = (bin[0] & 0xc0) >> 6;
		if (len == 1)
		{
			cred[n++] = encodingChars[x];
			break;
		}

		cred[n++] = encodingChars[((bin[1] & 0xf) << 2) | x];
		x = (bin[1] & 0xf0) >> 4;
		if (len == 2)
		{
			cred[n++] = encodingChars[x];
			break;
		}

		cred[n++] = encodingChars[((bin[2] & 0x3) << 4) | x];
		cred[n++] = encodingChars[(bin[2] & 0xfc) >> 2];
		bin += 3;
		len -= 3;
	}
	return n;
}

BOOL CredMarshalCredentialW(CRED_MARSHAL_TYPE CredType, PVOID cred, LPWSTR* MarshaledCredential)
{
	CERT_CREDENTIAL_INFO* cert = cred;
	WCHAR* p = NULL;

	if (!cred || (CredType == CertCredential && cert->cbSize < sizeof(*cert)))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	switch (CredType)
	{
		case CertCredential:
		{
			size_t size = (sizeof(cert->rgbHashOfCert) + 2) * 4 / 3;
			if (!(p = malloc((size + 4) * sizeof(WCHAR))))
				return FALSE;
			p[0] = '@';
			p[1] = '@';
			p[2] = (WCHAR)('A' + CredType);
			size_t len = cred_encode(cert->rgbHashOfCert, sizeof(cert->rgbHashOfCert), p + 3);
			p[len + 3] = 0;
			break;
		}
		default:
			WLog_ERR(TAG, "unhandled type 0x%x", CredType);
			SetLastError(ERROR_INVALID_PARAMETER);
			return FALSE;
	}

	*MarshaledCredential = p;
	return TRUE;
}

BOOL CredMarshalCredentialA(CRED_MARSHAL_TYPE CredType, PVOID Credential,
                            LPSTR* MarshaledCredential)
{
	WCHAR* b = NULL;
	if (!CredMarshalCredentialW(CredType, Credential, &b) || !b)
		return FALSE;

#ifdef __BIG_ENDIAN__
	ByteSwapUnicode(b, _wcslen(b));
#endif
	*MarshaledCredential = ConvertWCharNToUtf8Alloc(b, _wcslen(b), NULL);
	free(b);
	return (*MarshaledCredential != NULL);
}

BOOL CredUnmarshalCredentialW(LPCWSTR cred, PCRED_MARSHAL_TYPE pcredType, PVOID* out)
{
	if (!cred || !pcredType || !out || cred[0] != '@' || cred[1] != '@')
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	BYTE b = wchar_decode(cred[2]);
	if (!b || b > BinaryBlobForSystem)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	*pcredType = (CRED_MARSHAL_TYPE)b;

	size_t len = _wcslen(cred + 3);
	switch (*pcredType)
	{
		case CertCredential:
		{
			BYTE hash[CERT_HASH_LENGTH];

			if (len != 27 || !cred_decode(cred + 3, len, hash))
			{
				SetLastError(ERROR_INVALID_PARAMETER);
				return FALSE;
			}

			CERT_CREDENTIAL_INFO* cert = malloc(sizeof(*cert));
			if (!cert)
				return FALSE;

			memcpy(cert->rgbHashOfCert, hash, sizeof(cert->rgbHashOfCert));
			cert->cbSize = sizeof(*cert);
			*out = cert;
			break;
		}
		default:
			WLog_ERR(TAG, "unhandled credType 0x%x", *pcredType);
			SetLastError(ERROR_INVALID_PARAMETER);
			return FALSE;
	}
	return TRUE;
}

BOOL CredUnmarshalCredentialA(LPCSTR cred, PCRED_MARSHAL_TYPE CredType, PVOID* Credential)
{
	WCHAR* b = ConvertUtf8NToWCharAlloc(cred, strlen(cred), NULL);
	if (!b)
		return FALSE;

#ifdef __BIG_ENDIAN__
	ByteSwapUnicode(b, _wcslen(b));
#endif
	BOOL ret = CredUnmarshalCredentialW(b, CredType, Credential);
	free(b);
	return ret;
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

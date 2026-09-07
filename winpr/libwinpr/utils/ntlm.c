/**
 * WinPR: Windows Portable Runtime
 * NTLM Utils
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/ntlm.h>
#include <winpr/assert.h>

#include <winpr/crt.h>
#include <winpr/crypto.h>

/**
 * Define NTOWFv1(Password, User, Domain) as
 * 	MD4(UNICODE(Password))
 * EndDefine
 */

BOOL NTOWFv1W(LPCWSTR Password, UINT32 PasswordLengthInBytes, BYTE* NtHash)
{
	if (!Password || !NtHash)
		return FALSE;

	if (!winpr_Digest(WINPR_MD_MD4, Password, PasswordLengthInBytes, NtHash,
	                  WINPR_MD4_DIGEST_LENGTH))
		return FALSE;

	return TRUE;
}

BOOL NTOWFv1A(LPCSTR Password, UINT32 PasswordLengthInBytes, BYTE* NtHash)
{
	LPWSTR PasswordW = nullptr;
	BOOL result = FALSE;
	size_t pwdCharLength = 0;

	if (!NtHash)
		return FALSE;

	if (!Password && (PasswordLengthInBytes > 0))
		return FALSE;

	if (Password)
	{
		PasswordW = ConvertUtf8NToWCharAlloc(Password, PasswordLengthInBytes, &pwdCharLength);
		if (!PasswordW)
			return FALSE;
	}

	if (!NTOWFv1W(PasswordW, WINPR_ASSERTING_INT_CAST(UINT32, pwdCharLength * sizeof(WCHAR)),
	              NtHash))
		goto out_fail;

	result = TRUE;
out_fail:
	free(PasswordW);
	return result;
}

/**
 * Define NTOWFv2(Password, User, Domain) as
 * 	HMAC_MD5(MD4(UNICODE(Password)),
 * 		UNICODE(ConcatenationOf(UpperCase(User), Domain)))
 * EndDefine
 */

BOOL NTOWFv2W(LPCWSTR Password, UINT32 PasswordLengthInBytes, LPCWSTR User,
              UINT32 UserLengthInBytes, LPCWSTR Domain, UINT32 DomainLengthInBytes, BYTE* NtHash)
{
	BYTE NtHashV1[WINPR_MD5_DIGEST_LENGTH] = WINPR_C_ARRAY_INIT;

	if (!Domain && (DomainLengthInBytes > 0))
		return FALSE;
	if (!User || !Password || !NtHash)
		return FALSE;

	if (!NTOWFv1W(Password, PasswordLengthInBytes, NtHashV1))
		return FALSE;

	return NTOWFv2FromHashW(NtHashV1, User, UserLengthInBytes, Domain, DomainLengthInBytes, NtHash);
}

BOOL NTOWFv2A(LPCSTR Password, UINT32 PasswordLengthInBytes, LPCSTR User, UINT32 UserLengthInBytes,
              LPCSTR Domain, UINT32 DomainLengthInBytes, BYTE* NtHash)
{
	LPWSTR UserW = nullptr;
	LPWSTR DomainW = nullptr;
	LPWSTR PasswordW = nullptr;
	BOOL result = FALSE;
	size_t userCharLength = 0;
	size_t domainCharLength = 0;
	size_t pwdCharLength = 0;

	if (!NtHash)
		return FALSE;

	if (!User && (UserLengthInBytes > 0))
		return FALSE;
	if (!Password && (PasswordLengthInBytes > 0))
		return FALSE;
	if (!Domain && (DomainLengthInBytes > 0))
		return FALSE;

	if (User)
	{
		UserW = ConvertUtf8NToWCharAlloc(User, UserLengthInBytes, &userCharLength);
		if (!UserW)
			goto out_fail;
	}
	if (Domain)
	{
		DomainW = ConvertUtf8NToWCharAlloc(Domain, DomainLengthInBytes, &domainCharLength);
		if (!DomainW)
			goto out_fail;
	}
	if (Password)
	{
		PasswordW = ConvertUtf8NToWCharAlloc(Password, PasswordLengthInBytes, &pwdCharLength);
		if (!PasswordW)
			goto out_fail;
	}

	if (!NTOWFv2W(PasswordW, (UINT32)pwdCharLength * sizeof(WCHAR), UserW,
	              (UINT32)userCharLength * sizeof(WCHAR), DomainW,
	              (UINT32)domainCharLength * sizeof(WCHAR), NtHash))
		goto out_fail;

	result = TRUE;
out_fail:
	free(UserW);
	free(DomainW);
	free(PasswordW);
	return result;
}

BOOL NTOWFv2FromHashW(const BYTE* NtHashV1, LPCWSTR User, UINT32 UserLengthInBytes, LPCWSTR Domain,
                      UINT32 DomainLengthInBytes, BYTE* NtHash)
{
	BYTE result = FALSE;

	if (!NtHash || !NtHashV1)
		return FALSE;

	if (!User && (UserLengthInBytes > 0))
		return FALSE;
	if (!Domain && (DomainLengthInBytes > 0))
		return FALSE;

	if (!User)
		return FALSE;

	BYTE* buffer = (BYTE*)malloc(UserLengthInBytes + DomainLengthInBytes);
	if (!buffer)
		return FALSE;

	/* Concatenate(UpperCase(User), Domain) */
	CopyMemory(buffer, User, UserLengthInBytes);
	CharUpperBuffW(WINPR_PACKED_ALIGN_CAST(LPWSTR, buffer), UserLengthInBytes / sizeof(WCHAR));

	if (DomainLengthInBytes > 0)
	{
		CopyMemory(&buffer[UserLengthInBytes], Domain, DomainLengthInBytes);
	}

	/* Compute the HMAC-MD5 hash of the above value using the NTLMv1 hash as the key, the result is
	 * the NTLMv2 hash */
	if (!winpr_HMAC(WINPR_MD_MD5, NtHashV1, 16, buffer, UserLengthInBytes + DomainLengthInBytes,
	                NtHash, WINPR_MD5_DIGEST_LENGTH))
		goto out_fail;

	result = TRUE;
out_fail:
	free(buffer);
	return result;
}

BOOL NTOWFv2FromHashA(const BYTE* NtHashV1, LPCSTR User, UINT32 UserLengthInBytes, LPCSTR Domain,
                      UINT32 DomainLengthInBytes, BYTE* NtHash)
{
	LPWSTR UserW = nullptr;
	LPWSTR DomainW = nullptr;
	BOOL result = FALSE;
	size_t userCharLength = 0;
	size_t domainCharLength = 0;

	if (!NtHash || !NtHashV1)
		return FALSE;

	if (!User && (UserLengthInBytes > 0))
		return FALSE;
	if (!Domain && (DomainLengthInBytes > 0))
		return FALSE;

	if (User)
	{
		UserW = ConvertUtf8NToWCharAlloc(User, UserLengthInBytes, &userCharLength);
		if (!UserW)
			goto out_fail;
	}

	if (Domain)
	{
		DomainW = ConvertUtf8NToWCharAlloc(Domain, DomainLengthInBytes, &domainCharLength);
		if (!DomainW)
			goto out_fail;
	}

	if (!NTOWFv2FromHashW(NtHashV1, UserW, (UINT32)userCharLength * sizeof(WCHAR), DomainW,
	                      (UINT32)domainCharLength * sizeof(WCHAR), NtHash))
		goto out_fail;

	result = TRUE;
out_fail:
	free(UserW);
	free(DomainW);
	return result;
}

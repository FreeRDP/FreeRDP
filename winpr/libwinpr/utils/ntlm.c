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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/ntlm.h>

#include <winpr/crt.h>
#include <winpr/crypto.h>

/**
 * Define NTOWFv1(Password, User, Domain) as
 * 	MD4(UNICODE(Password))
 * EndDefine
 */

BOOL NTOWFv1W(LPWSTR Password, UINT32 PasswordLength, BYTE* NtHash)
{
	if (!Password || !NtHash)
		return FALSE;

	if (!winpr_Digest(WINPR_MD_MD4, (BYTE*)Password, (size_t)PasswordLength, NtHash,
	                  WINPR_MD4_DIGEST_LENGTH))
		return FALSE;

	return TRUE;
}

BOOL NTOWFv1A(LPSTR Password, UINT32 PasswordLength, BYTE* NtHash)
{
	LPWSTR PasswordW = NULL;
	BOOL result = FALSE;

	if (!NtHash)
		return FALSE;

	if (!(PasswordW = (LPWSTR)calloc(PasswordLength, 2)))
		return FALSE;

	MultiByteToWideChar(CP_ACP, 0, Password, PasswordLength, PasswordW, PasswordLength);

	if (!NTOWFv1W(PasswordW, PasswordLength * 2, NtHash))
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

BOOL NTOWFv2W(LPWSTR Password, UINT32 PasswordLength, LPWSTR User, UINT32 UserLength, LPWSTR Domain,
              UINT32 DomainLength, BYTE* NtHash)
{
	BYTE* buffer;
	BYTE NtHashV1[16];
	BOOL result = FALSE;

	if ((!User) || (!Password) || (!NtHash))
		return FALSE;

	if (!NTOWFv1W(Password, PasswordLength, NtHashV1))
		return FALSE;

	if (!(buffer = (BYTE*)malloc(UserLength + DomainLength)))
		return FALSE;

	/* Concatenate(UpperCase(User), Domain) */
	CopyMemory(buffer, User, UserLength);
	CharUpperBuffW((LPWSTR)buffer, UserLength / 2);
	CopyMemory(&buffer[UserLength], Domain, DomainLength);

	/* Compute the HMAC-MD5 hash of the above value using the NTLMv1 hash as the key, the result is
	 * the NTLMv2 hash */
	if (!winpr_HMAC(WINPR_MD_MD5, NtHashV1, 16, buffer, UserLength + DomainLength, NtHash,
	                WINPR_MD4_DIGEST_LENGTH))
		goto out_fail;

	result = TRUE;
out_fail:
	free(buffer);
	return result;
}

BOOL NTOWFv2A(LPSTR Password, UINT32 PasswordLength, LPSTR User, UINT32 UserLength, LPSTR Domain,
              UINT32 DomainLength, BYTE* NtHash)
{
	LPWSTR UserW = NULL;
	LPWSTR DomainW = NULL;
	LPWSTR PasswordW = NULL;
	BOOL result = FALSE;

	if (!NtHash)
		return FALSE;

	UserW = (LPWSTR)calloc(UserLength, 2);
	DomainW = (LPWSTR)calloc(DomainLength, 2);
	PasswordW = (LPWSTR)calloc(PasswordLength, 2);

	if (!UserW || !DomainW || !PasswordW)
		goto out_fail;

	MultiByteToWideChar(CP_ACP, 0, User, UserLength, UserW, UserLength);
	MultiByteToWideChar(CP_ACP, 0, Domain, DomainLength, DomainW, DomainLength);
	MultiByteToWideChar(CP_ACP, 0, Password, PasswordLength, PasswordW, PasswordLength);

	if (!NTOWFv2W(PasswordW, PasswordLength * 2, UserW, UserLength * 2, DomainW, DomainLength * 2,
	              NtHash))
		goto out_fail;

	result = TRUE;
out_fail:
	free(UserW);
	free(DomainW);
	free(PasswordW);
	return result;
}

BOOL NTOWFv2FromHashW(BYTE* NtHashV1, LPWSTR User, UINT32 UserLength, LPWSTR Domain,
                      UINT32 DomainLength, BYTE* NtHash)
{
	BYTE* buffer;
	BYTE result = FALSE;

	if (!User || !NtHash)
		return FALSE;

	if (!(buffer = (BYTE*)malloc(UserLength + DomainLength)))
		return FALSE;

	/* Concatenate(UpperCase(User), Domain) */
	CopyMemory(buffer, User, UserLength);
	CharUpperBuffW((LPWSTR)buffer, UserLength / 2);

	if (DomainLength > 0)
	{
		CopyMemory(&buffer[UserLength], Domain, DomainLength);
	}

	/* Compute the HMAC-MD5 hash of the above value using the NTLMv1 hash as the key, the result is
	 * the NTLMv2 hash */
	if (!winpr_HMAC(WINPR_MD_MD5, NtHashV1, 16, buffer, UserLength + DomainLength, NtHash,
	                WINPR_MD4_DIGEST_LENGTH))
		goto out_fail;

	result = TRUE;
out_fail:
	free(buffer);
	return result;
}

BOOL NTOWFv2FromHashA(BYTE* NtHashV1, LPSTR User, UINT32 UserLength, LPSTR Domain,
                      UINT32 DomainLength, BYTE* NtHash)
{
	LPWSTR UserW = NULL;
	LPWSTR DomainW = NULL;
	BOOL result = FALSE;

	if (!NtHash)
		return FALSE;

	UserW = (LPWSTR)calloc(UserLength, 2);
	DomainW = (LPWSTR)calloc(DomainLength, 2);

	if (!UserW || !DomainW)
		goto out_fail;

	MultiByteToWideChar(CP_ACP, 0, User, UserLength, UserW, UserLength);
	MultiByteToWideChar(CP_ACP, 0, Domain, DomainLength, DomainW, DomainLength);

	if (!NTOWFv2FromHashW(NtHashV1, UserW, UserLength * 2, DomainW, DomainLength * 2, NtHash))
		goto out_fail;

	result = TRUE;
out_fail:
	free(UserW);
	free(DomainW);
	return result;
}

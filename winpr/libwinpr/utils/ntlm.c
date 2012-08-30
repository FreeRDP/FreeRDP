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
#include <winpr/windows.h>

#include <time.h>

#include <openssl/ssl.h>
#include <openssl/des.h>
#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/rc4.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/engine.h>

/**
 * Define NTOWFv1(Password, User, Domain) as
 * 	MD4(UNICODE(Password))
 * EndDefine
 */

BYTE* NTOWFv1W(LPWSTR Password, UINT32 PasswordLength, BYTE* NtHash)
{
	MD4_CTX md4_ctx;

	if (!Password)
		return NULL;

	if (!NtHash)
		NtHash = malloc(16);

	MD4_Init(&md4_ctx);
	MD4_Update(&md4_ctx, Password, PasswordLength);
	MD4_Final((void*) NtHash, &md4_ctx);

	return NtHash;
}

BYTE* NTOWFv1A(LPSTR Password, UINT32 PasswordLength, BYTE* NtHash)
{
	LPWSTR PasswordW = NULL;

	PasswordW = (LPWSTR) malloc(PasswordLength * 2);
	MultiByteToWideChar(CP_ACP, 0, Password, PasswordLength, PasswordW, PasswordLength);

	NtHash = NTOWFv1W(PasswordW, PasswordLength * 2, NtHash);

	free(PasswordW);

	return NtHash;
}

/**
 * Define NTOWFv2(Password, User, Domain) as
 * 	HMAC_MD5(MD4(UNICODE(Password)),
 * 		UNICODE(ConcatenationOf(UpperCase(User), Domain)))
 * EndDefine
 */

BYTE* NTOWFv2W(LPWSTR Password, UINT32 PasswordLength, LPWSTR User,
		UINT32 UserLength, LPWSTR Domain, UINT32 DomainLength, BYTE* NtHash)
{
	BYTE* buffer;
	BYTE NtHashV1[16];

	if ((!User) || (!Password))
		return NULL;

	if (!NtHash)
		NtHash = (BYTE*) malloc(16);

	NTOWFv1W(Password, PasswordLength, NtHashV1);

	buffer = (BYTE*) malloc(UserLength + DomainLength);

	/* Concatenate(UpperCase(User), Domain) */

	CopyMemory(buffer, User, UserLength);
	CharUpperBuffW((LPWSTR) buffer, UserLength / 2);
	CopyMemory(&buffer[UserLength], Domain, DomainLength);

	/* Compute the HMAC-MD5 hash of the above value using the NTLMv1 hash as the key, the result is the NTLMv2 hash */
	HMAC(EVP_md5(), (void*) NtHashV1, 16, buffer, UserLength + DomainLength, (void*) NtHash, NULL);

	free(buffer);

	return NtHash;
}

BYTE* NTOWFv2A(LPSTR Password, UINT32 PasswordLength, LPSTR User,
		UINT32 UserLength, LPSTR Domain, UINT32 DomainLength, BYTE* NtHash)
{
	LPWSTR UserW = NULL;
	LPWSTR DomainW = NULL;
	LPWSTR PasswordW = NULL;

	UserW = (LPWSTR) malloc(UserLength * 2);
	DomainW = (LPWSTR) malloc(DomainLength * 2);
	PasswordW = (LPWSTR) malloc(PasswordLength * 2);

	MultiByteToWideChar(CP_ACP, 0, User, UserLength, UserW, UserLength);
	MultiByteToWideChar(CP_ACP, 0, Domain, DomainLength, DomainW, DomainLength);
	MultiByteToWideChar(CP_ACP, 0, Password, PasswordLength, PasswordW, PasswordLength);

	NtHash = NTOWFv2W(PasswordW, PasswordLength * 2, UserW, UserLength * 2, DomainW, DomainLength * 2, NtHash);

	free(UserW);
	free(DomainW);
	free(PasswordW);

	return NtHash;
}

BYTE* NTOWFv2FromHashW(BYTE* NtHashV1, LPWSTR User, UINT32 UserLength, LPWSTR Domain, UINT32 DomainLength, BYTE* NtHash)
{
	BYTE* buffer;

	if (!User)
		return NULL;

	if (!NtHash)
		NtHash = (BYTE*) malloc(16);

	buffer = (BYTE*) malloc(UserLength + DomainLength);

	/* Concatenate(UpperCase(User), Domain) */

	CopyMemory(buffer, User, UserLength);
	CharUpperBuffW((LPWSTR) buffer, UserLength / 2);

	if (DomainLength > 0)
	{
		CopyMemory(&buffer[UserLength], Domain, DomainLength);
	}

	/* Compute the HMAC-MD5 hash of the above value using the NTLMv1 hash as the key, the result is the NTLMv2 hash */
	HMAC(EVP_md5(), (void*) NtHashV1, 16, buffer, UserLength + DomainLength, (void*) NtHash, NULL);

	free(buffer);

	return NtHash;
}

BYTE* NTOWFv2FromHashA(BYTE* NtHashV1, LPSTR User, UINT32 UserLength, LPSTR Domain, UINT32 DomainLength, BYTE* NtHash)
{
	LPWSTR UserW = NULL;
	LPWSTR DomainW = NULL;
	LPWSTR PasswordW = NULL;

	UserW = (LPWSTR) malloc(UserLength * 2);
	DomainW = (LPWSTR) malloc(DomainLength * 2);

	MultiByteToWideChar(CP_ACP, 0, User, UserLength, UserW, UserLength);
	MultiByteToWideChar(CP_ACP, 0, Domain, DomainLength, DomainW, DomainLength);

	NtHash = NTOWFv2FromHashW(NtHashV1, UserW, UserLength * 2, DomainW, DomainLength * 2, NtHash);

	free(UserW);
	free(DomainW);
	free(PasswordW);

	return NtHash;
}

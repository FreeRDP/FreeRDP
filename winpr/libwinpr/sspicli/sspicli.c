/**
 * WinPR: Windows Portable Runtime
 * Security Support Provider Interface
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

#include <winpr/sspicli.h>

/**
 * sspicli.dll:
 *
 * EnumerateSecurityPackagesA
 * EnumerateSecurityPackagesW
 * GetUserNameExW
 * ImportSecurityContextA
 * LogonUser
 * LogonUserEx
 * LogonUserExExW
 * SspiCompareAuthIdentities
 * SspiCopyAuthIdentity
 * SspiDecryptAuthIdentity
 * SspiEncodeAuthIdentityAsStrings
 * SspiEncodeStringsAsAuthIdentity
 * SspiEncryptAuthIdentity
 * SspiExcludePackage
 * SspiFreeAuthIdentity
 * SspiGetTargetHostName
 * SspiIsAuthIdentityEncrypted
 * SspiLocalFree
 * SspiMarshalAuthIdentity
 * SspiPrepareForCredRead
 * SspiPrepareForCredWrite
 * SspiUnmarshalAuthIdentity
 * SspiValidateAuthIdentity
 * SspiZeroAuthIdentity
 */

#ifndef _WIN32

#include <winpr/crt.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <pthread.h>

#include <pwd.h>
#include <grp.h>

#include "../handle/handle.h"

#include "../security/security.h"

static BOOL LogonUserCloseHandle(HANDLE handle);

static BOOL LogonUserIsHandled(HANDLE handle)
{
	WINPR_ACCESS_TOKEN* pLogonUser = (WINPR_ACCESS_TOKEN*)handle;

	if (!pLogonUser || (pLogonUser->Type != HANDLE_TYPE_ACCESS_TOKEN))
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	return TRUE;
}

static int LogonUserGetFd(HANDLE handle)
{
	WINPR_ACCESS_TOKEN* pLogonUser = (WINPR_ACCESS_TOKEN*)handle;

	if (!LogonUserIsHandled(handle))
		return -1;

	/* TODO: File fd not supported */
	(void)pLogonUser;
	return -1;
}

BOOL LogonUserCloseHandle(HANDLE handle)
{
	WINPR_ACCESS_TOKEN* token = (WINPR_ACCESS_TOKEN*)handle;

	if (!handle || !LogonUserIsHandled(handle))
		return FALSE;

	free(token->Username);
	free(token->Domain);
	free(token);
	return TRUE;
}

static HANDLE_OPS ops = { LogonUserIsHandled,
	                      LogonUserCloseHandle,
	                      LogonUserGetFd,
	                      NULL, /* CleanupHandle */
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL };

BOOL LogonUserA(LPCSTR lpszUsername, LPCSTR lpszDomain, LPCSTR lpszPassword, DWORD dwLogonType,
                DWORD dwLogonProvider, PHANDLE phToken)
{
	struct passwd* pw;
	WINPR_ACCESS_TOKEN* token;

	if (!lpszUsername)
		return FALSE;

	token = (WINPR_ACCESS_TOKEN*)calloc(1, sizeof(WINPR_ACCESS_TOKEN));

	if (!token)
		return FALSE;

	WINPR_HANDLE_SET_TYPE_AND_MODE(token, HANDLE_TYPE_ACCESS_TOKEN, WINPR_FD_READ);
	token->ops = &ops;
	token->Username = _strdup(lpszUsername);

	if (!token->Username)
	{
		free(token);
		return FALSE;
	}

	if (lpszDomain)
	{
		token->Domain = _strdup(lpszDomain);

		if (!token->Domain)
		{
			free(token->Username);
			free(token);
			return FALSE;
		}
	}

	pw = getpwnam(lpszUsername);

	if (pw)
	{
		token->UserId = (DWORD)pw->pw_uid;
		token->GroupId = (DWORD)pw->pw_gid;
	}

	*((ULONG_PTR*)phToken) = (ULONG_PTR)token;
	return TRUE;
}

BOOL LogonUserW(LPCWSTR lpszUsername, LPCWSTR lpszDomain, LPCWSTR lpszPassword, DWORD dwLogonType,
                DWORD dwLogonProvider, PHANDLE phToken)
{
	return TRUE;
}

BOOL LogonUserExA(LPCSTR lpszUsername, LPCSTR lpszDomain, LPCSTR lpszPassword, DWORD dwLogonType,
                  DWORD dwLogonProvider, PHANDLE phToken, PSID* ppLogonSid, PVOID* ppProfileBuffer,
                  LPDWORD pdwProfileLength, PQUOTA_LIMITS pQuotaLimits)
{
	return TRUE;
}

BOOL LogonUserExW(LPCWSTR lpszUsername, LPCWSTR lpszDomain, LPCWSTR lpszPassword, DWORD dwLogonType,
                  DWORD dwLogonProvider, PHANDLE phToken, PSID* ppLogonSid, PVOID* ppProfileBuffer,
                  LPDWORD pdwProfileLength, PQUOTA_LIMITS pQuotaLimits)
{
	return TRUE;
}

BOOL GetUserNameExA(EXTENDED_NAME_FORMAT NameFormat, LPSTR lpNameBuffer, PULONG nSize)
{
	size_t length;
	char* login;

	switch (NameFormat)
	{
		case NameSamCompatible:
			login = getlogin();
			length = strlen(login);

			if (*nSize >= length)
			{
				CopyMemory(lpNameBuffer, login, length + 1);
				return 1;
			}
			else
			{
				*nSize = length + 1;
			}

			break;

		case NameFullyQualifiedDN:
		case NameDisplay:
		case NameUniqueId:
		case NameCanonical:
		case NameUserPrincipal:
		case NameCanonicalEx:
		case NameServicePrincipal:
		case NameDnsDomain:
			break;

		default:
			break;
	}

	return 0;
}

BOOL GetUserNameExW(EXTENDED_NAME_FORMAT NameFormat, LPWSTR lpNameBuffer, PULONG nSize)
{
	return 0;
}

#endif

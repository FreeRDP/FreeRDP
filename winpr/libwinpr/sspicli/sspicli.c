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

#include <winpr/config.h>

#include <winpr/assert.h>
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

#ifdef WINPR_HAVE_UNISTD_H
#include <unistd.h>
#endif

#if defined(WINPR_HAVE_GETPWUID_R)
#include <sys/types.h>
#endif

#include <pthread.h>

#include <pwd.h>
#include <grp.h>

#include "../handle/handle.h"

#include "../security/security.h"

static BOOL LogonUserCloseHandle(HANDLE handle);

static BOOL LogonUserIsHandled(HANDLE handle)
{
	return WINPR_HANDLE_IS_HANDLED(handle, HANDLE_TYPE_ACCESS_TOKEN, FALSE);
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
	                      NULL,
	                      NULL };

BOOL LogonUserA(LPCSTR lpszUsername, LPCSTR lpszDomain, WINPR_ATTR_UNUSED LPCSTR lpszPassword,
                WINPR_ATTR_UNUSED DWORD dwLogonType, WINPR_ATTR_UNUSED DWORD dwLogonProvider,
                PHANDLE phToken)
{
	if (!lpszUsername)
		return FALSE;

	WINPR_ACCESS_TOKEN* token = (WINPR_ACCESS_TOKEN*)calloc(1, sizeof(WINPR_ACCESS_TOKEN));

	if (!token)
		return FALSE;

	WINPR_HANDLE_SET_TYPE_AND_MODE(token, HANDLE_TYPE_ACCESS_TOKEN, WINPR_FD_READ);
	token->common.ops = &ops;
	token->Username = _strdup(lpszUsername);

	if (!token->Username)
		goto fail;

	if (lpszDomain)
	{
		token->Domain = _strdup(lpszDomain);

		if (!token->Domain)
			goto fail;
	}

	{
		long buflen = sysconf(_SC_GETPW_R_SIZE_MAX);
		if (buflen < 0)
			buflen = 8196;

		{
			const size_t s = 1ULL + (size_t)buflen;
			char* buf = (char*)calloc(s, sizeof(char));
			if (!buf)
				goto fail;

			{
				struct passwd pwd = { 0 };
				struct passwd* pw = NULL;
				const int rc = getpwnam_r(lpszUsername, &pwd, buf,
				                          WINPR_ASSERTING_INT_CAST(size_t, buflen), &pw);
				free(buf);
				if ((rc == 0) && pw)
				{
					token->UserId = (DWORD)pw->pw_uid;
					token->GroupId = (DWORD)pw->pw_gid;
				}

				*((ULONG_PTR*)phToken) = (ULONG_PTR)token;
				return TRUE;
			}
		}
	}
fail:
	free(token->Username);
	free(token->Domain);
	free(token);
	return FALSE;
}

BOOL LogonUserW(WINPR_ATTR_UNUSED LPCWSTR lpszUsername, WINPR_ATTR_UNUSED LPCWSTR lpszDomain,
                WINPR_ATTR_UNUSED LPCWSTR lpszPassword, WINPR_ATTR_UNUSED DWORD dwLogonType,
                WINPR_ATTR_UNUSED DWORD dwLogonProvider, WINPR_ATTR_UNUSED PHANDLE phToken)
{
	WLog_ERR("TODO", "TODO: implement");
	return TRUE;
}

BOOL LogonUserExA(WINPR_ATTR_UNUSED LPCSTR lpszUsername, WINPR_ATTR_UNUSED LPCSTR lpszDomain,
                  WINPR_ATTR_UNUSED LPCSTR lpszPassword, WINPR_ATTR_UNUSED DWORD dwLogonType,
                  WINPR_ATTR_UNUSED DWORD dwLogonProvider, WINPR_ATTR_UNUSED PHANDLE phToken,
                  WINPR_ATTR_UNUSED PSID* ppLogonSid, WINPR_ATTR_UNUSED PVOID* ppProfileBuffer,
                  WINPR_ATTR_UNUSED LPDWORD pdwProfileLength,
                  WINPR_ATTR_UNUSED PQUOTA_LIMITS pQuotaLimits)
{
	WLog_ERR("TODO", "TODO: implement");
	return TRUE;
}

BOOL LogonUserExW(WINPR_ATTR_UNUSED LPCWSTR lpszUsername, WINPR_ATTR_UNUSED LPCWSTR lpszDomain,
                  WINPR_ATTR_UNUSED LPCWSTR lpszPassword, WINPR_ATTR_UNUSED DWORD dwLogonType,
                  WINPR_ATTR_UNUSED DWORD dwLogonProvider, WINPR_ATTR_UNUSED PHANDLE phToken,
                  WINPR_ATTR_UNUSED PSID* ppLogonSid, WINPR_ATTR_UNUSED PVOID* ppProfileBuffer,
                  WINPR_ATTR_UNUSED LPDWORD pdwProfileLength,
                  WINPR_ATTR_UNUSED PQUOTA_LIMITS pQuotaLimits)
{
	WLog_ERR("TODO", "TODO: implement");
	return TRUE;
}

BOOL GetUserNameExA(EXTENDED_NAME_FORMAT NameFormat, LPSTR lpNameBuffer, PULONG nSize)
{
	WINPR_ASSERT(lpNameBuffer);
	WINPR_ASSERT(nSize);

	switch (NameFormat)
	{
		case NameSamCompatible:
#if defined(WINPR_HAVE_GETPWUID_R)
		{
			int rc = 0;
			struct passwd pwd = { 0 };
			struct passwd* result = NULL;
			uid_t uid = getuid();

			rc = getpwuid_r(uid, &pwd, lpNameBuffer, *nSize, &result);
			if (rc != 0)
				return FALSE;
			if (result == NULL)
				return FALSE;
		}
#elif defined(WINPR_HAVE_GETLOGIN_R)
			if (getlogin_r(lpNameBuffer, *nSize) != 0)
				return FALSE;
#else
		{
			const char* name = getlogin();
			if (!name)
				return FALSE;
			strncpy(lpNameBuffer, name, strnlen(name, *nSize));
		}
#endif
			{
				const size_t len = strnlen(lpNameBuffer, *nSize);
				if (len > UINT32_MAX)
					return FALSE;
				*nSize = (ULONG)len;
			}
			return TRUE;

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

	return FALSE;
}

BOOL GetUserNameExW(EXTENDED_NAME_FORMAT NameFormat, LPWSTR lpNameBuffer, PULONG nSize)
{
	BOOL rc = FALSE;
	char* name = NULL;

	WINPR_ASSERT(nSize);
	WINPR_ASSERT(lpNameBuffer);

	name = calloc(1, *nSize + 1);
	if (!name)
		goto fail;

	if (!GetUserNameExA(NameFormat, name, nSize))
		goto fail;

	{
		const SSIZE_T res = ConvertUtf8ToWChar(name, lpNameBuffer, *nSize);
		if ((res < 0) || (res >= UINT32_MAX))
			goto fail;

		*nSize = (UINT32)res + 1;
	}
	rc = TRUE;
fail:
	free(name);
	return rc;
}

#endif

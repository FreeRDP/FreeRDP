/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Security Support Provider Interface (SSPI)
 *
 * Copyright 2012-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2017 Dorian Ducournau <dorian.ducournau@gmail.com>
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
#include <winpr/windows.h>

#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/ssl.h>
#include <winpr/print.h>

#include "sspi.h"

#include "sspi_winpr.h"

#include "../log.h"
#define TAG WINPR_TAG("sspi")

/* Authentication Functions: http://msdn.microsoft.com/en-us/library/windows/desktop/aa374731/ */

#include "NTLM/ntlm_export.h"
#include "CredSSP/credssp.h"
#include "Kerberos/kerberos.h"
#include "Negotiate/negotiate.h"
#include "Schannel/schannel.h"

static const SecPkgInfoA* SecPkgInfoA_LIST[] = { &NTLM_SecPkgInfoA, &KERBEROS_SecPkgInfoA,
	                                             &NEGOTIATE_SecPkgInfoA, &CREDSSP_SecPkgInfoA,
	                                             &SCHANNEL_SecPkgInfoA };

static const SecPkgInfoW* SecPkgInfoW_LIST[] = { &NTLM_SecPkgInfoW, &KERBEROS_SecPkgInfoW,
	                                             &NEGOTIATE_SecPkgInfoW, &CREDSSP_SecPkgInfoW,
	                                             &SCHANNEL_SecPkgInfoW };

static SecurityFunctionTableA winpr_SecurityFunctionTableA;
static SecurityFunctionTableW winpr_SecurityFunctionTableW;

typedef struct
{
	const SEC_CHAR* Name;
	const SecurityFunctionTableA* SecurityFunctionTable;
} SecurityFunctionTableA_NAME;

typedef struct
{
	const SEC_WCHAR* Name;
	const SecurityFunctionTableW* SecurityFunctionTable;
} SecurityFunctionTableW_NAME;

static const SecurityFunctionTableA_NAME SecurityFunctionTableA_NAME_LIST[] = {
	{ "NTLM", &NTLM_SecurityFunctionTableA },
	{ "Kerberos", &KERBEROS_SecurityFunctionTableA },
	{ "Negotiate", &NEGOTIATE_SecurityFunctionTableA },
	{ "CREDSSP", &CREDSSP_SecurityFunctionTableA },
	{ "Schannel", &SCHANNEL_SecurityFunctionTableA }
};

static const WCHAR _NTLM_NAME_W[] = { 'N', 'T', 'L', 'M', '\0' };
static const WCHAR _KERBEROS_NAME_W[] = { 'K', 'e', 'r', 'b', 'e', 'r', 'o', 's', '\0' };
static const WCHAR _NEGOTIATE_NAME_W[] = { 'N', 'e', 'g', 'o', 't', 'i', 'a', 't', 'e', '\0' };
static const WCHAR _CREDSSP_NAME_W[] = { 'C', 'r', 'e', 'd', 'S', 'S', 'P', '\0' };
static const WCHAR _SCHANNEL_NAME_W[] = { 'S', 'c', 'h', 'a', 'n', 'n', 'e', 'l', '\0' };

static const SecurityFunctionTableW_NAME SecurityFunctionTableW_NAME_LIST[] = {
	{ _NTLM_NAME_W, &NTLM_SecurityFunctionTableW },
	{ _KERBEROS_NAME_W, &KERBEROS_SecurityFunctionTableW },
	{ _NEGOTIATE_NAME_W, &NEGOTIATE_SecurityFunctionTableW },
	{ _CREDSSP_NAME_W, &CREDSSP_SecurityFunctionTableW },
	{ _SCHANNEL_NAME_W, &SCHANNEL_SecurityFunctionTableW }
};

#define SecHandle_LOWER_MAX 0xFFFFFFFF
#define SecHandle_UPPER_MAX 0xFFFFFFFE

typedef struct
{
	void* contextBuffer;
	UINT32 allocatorIndex;
} CONTEXT_BUFFER_ALLOC_ENTRY;

typedef struct
{
	UINT32 cEntries;
	UINT32 cMaxEntries;
	CONTEXT_BUFFER_ALLOC_ENTRY* entries;
} CONTEXT_BUFFER_ALLOC_TABLE;

static CONTEXT_BUFFER_ALLOC_TABLE ContextBufferAllocTable = { 0 };

static int sspi_ContextBufferAllocTableNew(void)
{
	size_t size;
	ContextBufferAllocTable.entries = NULL;
	ContextBufferAllocTable.cEntries = 0;
	ContextBufferAllocTable.cMaxEntries = 4;
	size = sizeof(CONTEXT_BUFFER_ALLOC_ENTRY) * ContextBufferAllocTable.cMaxEntries;
	ContextBufferAllocTable.entries = (CONTEXT_BUFFER_ALLOC_ENTRY*)calloc(1, size);

	if (!ContextBufferAllocTable.entries)
		return -1;

	return 1;
}

static int sspi_ContextBufferAllocTableGrow(void)
{
	size_t size;
	CONTEXT_BUFFER_ALLOC_ENTRY* entries;
	ContextBufferAllocTable.cEntries = 0;
	ContextBufferAllocTable.cMaxEntries *= 2;
	size = sizeof(CONTEXT_BUFFER_ALLOC_ENTRY) * ContextBufferAllocTable.cMaxEntries;

	if (!size)
		return -1;

	entries = (CONTEXT_BUFFER_ALLOC_ENTRY*)realloc(ContextBufferAllocTable.entries, size);

	if (!entries)
	{
		free(ContextBufferAllocTable.entries);
		return -1;
	}

	ContextBufferAllocTable.entries = entries;
	ZeroMemory((void*)&ContextBufferAllocTable.entries[ContextBufferAllocTable.cMaxEntries / 2],
	           size / 2);
	return 1;
}

static void sspi_ContextBufferAllocTableFree(void)
{
	if (ContextBufferAllocTable.cEntries != 0)
		WLog_ERR(TAG, "ContextBufferAllocTable.entries == %" PRIu32,
		         ContextBufferAllocTable.cEntries);

	ContextBufferAllocTable.cEntries = ContextBufferAllocTable.cMaxEntries = 0;
	free(ContextBufferAllocTable.entries);
	ContextBufferAllocTable.entries = NULL;
}

static void* sspi_ContextBufferAlloc(UINT32 allocatorIndex, size_t size)
{
	UINT32 index;
	void* contextBuffer;

	for (index = 0; index < ContextBufferAllocTable.cMaxEntries; index++)
	{
		if (!ContextBufferAllocTable.entries[index].contextBuffer)
		{
			contextBuffer = calloc(1, size);

			if (!contextBuffer)
				return NULL;

			ContextBufferAllocTable.cEntries++;
			ContextBufferAllocTable.entries[index].contextBuffer = contextBuffer;
			ContextBufferAllocTable.entries[index].allocatorIndex = allocatorIndex;
			return ContextBufferAllocTable.entries[index].contextBuffer;
		}
	}

	/* no available entry was found, the table needs to be grown */

	if (sspi_ContextBufferAllocTableGrow() < 0)
		return NULL;

	/* the next call to sspi_ContextBufferAlloc() should now succeed */
	return sspi_ContextBufferAlloc(allocatorIndex, size);
}

SSPI_CREDENTIALS* sspi_CredentialsNew(void)
{
	SSPI_CREDENTIALS* credentials;
	credentials = (SSPI_CREDENTIALS*)calloc(1, sizeof(SSPI_CREDENTIALS));
	return credentials;
}

void sspi_CredentialsFree(SSPI_CREDENTIALS* credentials)
{
	size_t userLength = 0;
	size_t domainLength = 0;
	size_t passwordLength = 0;

	if (!credentials)
		return;

	if (credentials->ntlmSettings.samFile)
		free(credentials->ntlmSettings.samFile);

	userLength = credentials->identity.UserLength;
	domainLength = credentials->identity.DomainLength;
	passwordLength = credentials->identity.PasswordLength;

	if (passwordLength > SSPI_CREDENTIALS_HASH_LENGTH_OFFSET) /* [pth] */
		passwordLength -= SSPI_CREDENTIALS_HASH_LENGTH_OFFSET;

	if (credentials->identity.Flags & SEC_WINNT_AUTH_IDENTITY_UNICODE)
	{
		userLength *= 2;
		domainLength *= 2;
		passwordLength *= 2;
	}

	if (credentials->identity.User)
		memset(credentials->identity.User, 0, userLength);
	if (credentials->identity.Domain)
		memset(credentials->identity.Domain, 0, domainLength);
	if (credentials->identity.Password)
		memset(credentials->identity.Password, 0, passwordLength);
	free(credentials->identity.User);
	free(credentials->identity.Domain);
	free(credentials->identity.Password);
	free(credentials);
}

void* sspi_SecBufferAlloc(PSecBuffer SecBuffer, ULONG size)
{
	if (!SecBuffer)
		return NULL;

	SecBuffer->pvBuffer = calloc(1, size);

	if (!SecBuffer->pvBuffer)
		return NULL;

	SecBuffer->cbBuffer = size;
	return SecBuffer->pvBuffer;
}

void sspi_SecBufferFree(PSecBuffer SecBuffer)
{
	if (!SecBuffer)
		return;

	if (SecBuffer->pvBuffer)
		memset(SecBuffer->pvBuffer, 0, SecBuffer->cbBuffer);

	free(SecBuffer->pvBuffer);
	SecBuffer->pvBuffer = NULL;
	SecBuffer->cbBuffer = 0;
}

SecHandle* sspi_SecureHandleAlloc(void)
{
	SecHandle* handle = (SecHandle*)calloc(1, sizeof(SecHandle));

	if (!handle)
		return NULL;

	SecInvalidateHandle(handle);
	return handle;
}

void* sspi_SecureHandleGetLowerPointer(SecHandle* handle)
{
	void* pointer;

	if (!handle || !SecIsValidHandle(handle) || !handle->dwLower)
		return NULL;

	pointer = (void*)~((size_t)handle->dwLower);
	return pointer;
}

void sspi_SecureHandleInvalidate(SecHandle* handle)
{
	if (!handle)
		return;

	handle->dwLower = 0;
	handle->dwUpper = 0;
}

void sspi_SecureHandleSetLowerPointer(SecHandle* handle, void* pointer)
{
	if (!handle)
		return;

	handle->dwLower = (ULONG_PTR)(~((size_t)pointer));
}

void* sspi_SecureHandleGetUpperPointer(SecHandle* handle)
{
	void* pointer;

	if (!handle || !SecIsValidHandle(handle) || !handle->dwUpper)
		return NULL;

	pointer = (void*)~((size_t)handle->dwUpper);
	return pointer;
}

void sspi_SecureHandleSetUpperPointer(SecHandle* handle, void* pointer)
{
	if (!handle)
		return;

	handle->dwUpper = (ULONG_PTR)(~((size_t)pointer));
}

void sspi_SecureHandleFree(SecHandle* handle)
{
	free(handle);
}

int sspi_SetAuthIdentityW(SEC_WINNT_AUTH_IDENTITY* identity, const WCHAR* user, const WCHAR* domain,
                          const WCHAR* password)
{
	return sspi_SetAuthIdentityWithLengthW(identity, user, user ? _wcslen(user) : 0, domain,
	                                       domain ? _wcslen(domain) : 0, password,
	                                       password ? _wcslen(password) : 0);
}

static BOOL copy(WCHAR** dst, UINT32* dstLen, const WCHAR* what, size_t len)
{
	WINPR_ASSERT(dst);
	WINPR_ASSERT(dstLen);
	WINPR_ASSERT(what);
	WINPR_ASSERT(len > 0);
	WINPR_ASSERT(_wcsnlen(what, len) == len);

	*dst = calloc(sizeof(WCHAR), len + 1);
	if (!*dst)
		return FALSE;
	memcpy(*dst, what, len * sizeof(WCHAR));
	*dstLen = len;
	return TRUE;
}

int sspi_SetAuthIdentityWithLengthW(SEC_WINNT_AUTH_IDENTITY* identity, const WCHAR* user,
                                    size_t userLen, const WCHAR* domain, size_t domainLen,
                                    const WCHAR* password, size_t passwordLen)
{
	WINPR_ASSERT(identity);
	sspi_FreeAuthIdentity(identity);
	identity->Flags &= ~SEC_WINNT_AUTH_IDENTITY_ANSI;
	identity->Flags |= SEC_WINNT_AUTH_IDENTITY_UNICODE;
	if (user && userLen > 0)
	{
		if (!copy(&identity->User, &identity->UserLength, user, userLen))
			return -1;
	}
	if (domain && domainLen > 0)
	{
		if (!copy(&identity->Domain, &identity->DomainLength, domain, domainLen))
			return -1;
	}
	if (password && passwordLen > 0)
	{
		if (!copy(&identity->Password, &identity->PasswordLength, password, passwordLen))
			return -1;
	}

	return 1;
}

int sspi_SetAuthIdentityA(SEC_WINNT_AUTH_IDENTITY* identity, const char* user, const char* domain,
                          const char* password)
{
	int rc;
	size_t unicodePasswordLenW = 0;
	LPWSTR unicodePassword = ConvertUtf8ToWCharAlloc(password, &unicodePasswordLenW);

	if (!unicodePassword || (unicodePasswordLenW == 0))
		return -1;

	rc = sspi_SetAuthIdentityWithUnicodePassword(identity, user, domain, unicodePassword,
	                                             (ULONG)(unicodePasswordLenW));
	free(unicodePassword);
	return rc;
}

int sspi_SetAuthIdentityWithUnicodePassword(SEC_WINNT_AUTH_IDENTITY* identity, const char* user,
                                            const char* domain, LPWSTR password,
                                            ULONG passwordLength)
{
	sspi_FreeAuthIdentity(identity);
	identity->Flags &= ~SEC_WINNT_AUTH_IDENTITY_ANSI;
	identity->Flags |= SEC_WINNT_AUTH_IDENTITY_UNICODE;

	if (user && (strlen(user) > 0))
	{
		size_t len = 0;
		identity->User = ConvertUtf8ToWCharAlloc(user, &len);
		if (!identity->User || (len == 0) || (len > ULONG_MAX))
			return -1;

		identity->UserLength = (ULONG)len;
	}

	if (domain && (strlen(domain) > 0))
	{
		size_t len = 0;
		identity->Domain = ConvertUtf8ToWCharAlloc(domain, &len);
		if (!identity->Domain || (len == 0) || (len > ULONG_MAX))
			return -1;

		identity->DomainLength = len;
	}

	identity->Password = (UINT16*)calloc(1, (passwordLength + 1) * sizeof(WCHAR));

	if (!identity->Password)
		return -1;

	CopyMemory(identity->Password, password, passwordLength * sizeof(WCHAR));
	identity->PasswordLength = passwordLength;
	return 1;
}

UINT32 sspi_GetAuthIdentityVersion(const void* identity)
{
	UINT32 version;

	if (!identity)
		return 0;

	version = *((const UINT32*)identity);

	if ((version == SEC_WINNT_AUTH_IDENTITY_VERSION) ||
	    (version == SEC_WINNT_AUTH_IDENTITY_VERSION_2))
	{
		return version;
	}

	return 0; // SEC_WINNT_AUTH_IDENTITY (no version)
}

UINT32 sspi_GetAuthIdentityFlags(const void* identity)
{
	UINT32 version;
	UINT32 flags = 0;

	if (!identity)
		return 0;

	version = sspi_GetAuthIdentityVersion(identity);

	if (version == SEC_WINNT_AUTH_IDENTITY_VERSION)
	{
		flags = ((const SEC_WINNT_AUTH_IDENTITY_EX*)identity)->Flags;
	}
	else if (version == SEC_WINNT_AUTH_IDENTITY_VERSION_2)
	{
		flags = ((const SEC_WINNT_AUTH_IDENTITY_EX2*)identity)->Flags;
	}
	else // SEC_WINNT_AUTH_IDENTITY
	{
		flags = ((const SEC_WINNT_AUTH_IDENTITY*)identity)->Flags;
	}

	return flags;
}

BOOL sspi_GetAuthIdentityUserDomainW(const void* identity, const WCHAR** pUser, UINT32* pUserLength,
                                     const WCHAR** pDomain, UINT32* pDomainLength)
{
	UINT32 version;

	if (!identity)
		return FALSE;

	version = sspi_GetAuthIdentityVersion(identity);

	if (version == SEC_WINNT_AUTH_IDENTITY_VERSION)
	{
		const SEC_WINNT_AUTH_IDENTITY_EXW* id = (const SEC_WINNT_AUTH_IDENTITY_EXW*)identity;
		*pUser = (const WCHAR*)id->User;
		*pUserLength = id->UserLength;
		*pDomain = (const WCHAR*)id->Domain;
		*pDomainLength = id->DomainLength;
	}
	else if (version == SEC_WINNT_AUTH_IDENTITY_VERSION_2)
	{
		const SEC_WINNT_AUTH_IDENTITY_EX2* id = (const SEC_WINNT_AUTH_IDENTITY_EX2*)identity;
		UINT32 UserOffset = id->UserOffset;
		UINT32 DomainOffset = id->DomainOffset;
		*pUser = (const WCHAR*)&((const uint8_t*)identity)[UserOffset];
		*pUserLength = id->UserLength / 2;
		*pDomain = (const WCHAR*)&((const uint8_t*)identity)[DomainOffset];
		*pDomainLength = id->DomainLength / 2;
	}
	else // SEC_WINNT_AUTH_IDENTITY
	{
		const SEC_WINNT_AUTH_IDENTITY_W* id = (const SEC_WINNT_AUTH_IDENTITY_W*)identity;
		*pUser = (const WCHAR*)id->User;
		*pUserLength = id->UserLength;
		*pDomain = (const WCHAR*)id->Domain;
		*pDomainLength = id->DomainLength;
	}

	return TRUE;
}

BOOL sspi_GetAuthIdentityUserDomainA(const void* identity, const char** pUser, UINT32* pUserLength,
                                     const char** pDomain, UINT32* pDomainLength)
{
	UINT32 version;

	if (!identity)
		return FALSE;

	version = sspi_GetAuthIdentityVersion(identity);

	if (version == SEC_WINNT_AUTH_IDENTITY_VERSION)
	{
		const SEC_WINNT_AUTH_IDENTITY_EXA* id = (const SEC_WINNT_AUTH_IDENTITY_EXA*)identity;
		*pUser = (const char*)id->User;
		*pUserLength = id->UserLength;
		*pDomain = (const char*)id->Domain;
		*pDomainLength = id->DomainLength;
	}
	else if (version == SEC_WINNT_AUTH_IDENTITY_VERSION_2)
	{
		const SEC_WINNT_AUTH_IDENTITY_EX2* id = (const SEC_WINNT_AUTH_IDENTITY_EX2*)identity;
		UINT32 UserOffset = id->UserOffset;
		UINT32 DomainOffset = id->DomainOffset;
		*pUser = (const char*)&((const uint8_t*)identity)[UserOffset];
		*pUserLength = id->UserLength;
		*pDomain = (const char*)&((const uint8_t*)identity)[DomainOffset];
		*pDomainLength = id->DomainLength;
	}
	else // SEC_WINNT_AUTH_IDENTITY
	{
		const SEC_WINNT_AUTH_IDENTITY_A* id = (const SEC_WINNT_AUTH_IDENTITY_A*)identity;
		*pUser = (const char*)id->User;
		*pUserLength = id->UserLength;
		*pDomain = (const char*)id->Domain;
		*pDomainLength = id->DomainLength;
	}

	return TRUE;
}

BOOL sspi_GetAuthIdentityPasswordW(const void* identity, const WCHAR** pPassword,
                                   UINT32* pPasswordLength)
{
	UINT32 version;

	if (!identity)
		return FALSE;

	version = sspi_GetAuthIdentityVersion(identity);

	if (version == SEC_WINNT_AUTH_IDENTITY_VERSION)
	{
		const SEC_WINNT_AUTH_IDENTITY_EXW* id = (const SEC_WINNT_AUTH_IDENTITY_EXW*)identity;
		*pPassword = (const WCHAR*)id->Password;
		*pPasswordLength = id->PasswordLength;
	}
	else if (version == SEC_WINNT_AUTH_IDENTITY_VERSION_2)
	{
		return FALSE; // TODO: packed credentials
	}
	else // SEC_WINNT_AUTH_IDENTITY
	{
		const SEC_WINNT_AUTH_IDENTITY_W* id = (const SEC_WINNT_AUTH_IDENTITY_W*)identity;
		*pPassword = (const WCHAR*)id->Password;
		*pPasswordLength = id->PasswordLength;
	}

	return TRUE;
}

BOOL sspi_GetAuthIdentityPasswordA(const void* identity, const char** pPassword,
                                   UINT32* pPasswordLength)
{
	UINT32 version;

	if (!identity)
		return FALSE;

	version = sspi_GetAuthIdentityVersion(identity);

	if (version == SEC_WINNT_AUTH_IDENTITY_VERSION)
	{
		const SEC_WINNT_AUTH_IDENTITY_EXA* id = (const SEC_WINNT_AUTH_IDENTITY_EXA*)identity;
		*pPassword = (const char*)id->Password;
		*pPasswordLength = id->PasswordLength;
	}
	else if (version == SEC_WINNT_AUTH_IDENTITY_VERSION_2)
	{
		return FALSE; // TODO: packed credentials
	}
	else // SEC_WINNT_AUTH_IDENTITY
	{
		const SEC_WINNT_AUTH_IDENTITY_A* id = (const SEC_WINNT_AUTH_IDENTITY_A*)identity;
		*pPassword = (const char*)id->Password;
		*pPasswordLength = id->PasswordLength;
	}

	return TRUE;
}

BOOL sspi_CopyAuthIdentityFieldsA(const SEC_WINNT_AUTH_IDENTITY_INFO* identity, char** pUser,
                                  char** pDomain, char** pPassword)
{
	BOOL success = FALSE;
	const char* UserA = NULL;
	const char* DomainA = NULL;
	const char* PasswordA = NULL;
	const WCHAR* UserW = NULL;
	const WCHAR* DomainW = NULL;
	const WCHAR* PasswordW = NULL;
	UINT32 UserLength = 0;
	UINT32 DomainLength = 0;
	UINT32 PasswordLength = 0;

	if (!identity || !pUser || !pDomain || !pPassword)
		return FALSE;

	*pUser = *pDomain = *pPassword = NULL;

	UINT32 identityFlags = sspi_GetAuthIdentityFlags(identity);

	if (identityFlags & SEC_WINNT_AUTH_IDENTITY_ANSI)
	{
		if (!sspi_GetAuthIdentityUserDomainA(identity, &UserA, &UserLength, &DomainA,
		                                     &DomainLength))
			goto cleanup;

		if (!sspi_GetAuthIdentityPasswordA(identity, &PasswordA, &PasswordLength))
			goto cleanup;

		if (UserA && UserLength)
		{
			*pUser = _strdup(UserA);

			if (!(*pUser))
				goto cleanup;
		}

		if (DomainA && DomainLength)
		{
			*pDomain = _strdup(DomainA);

			if (!(*pDomain))
				goto cleanup;
		}

		if (PasswordA && PasswordLength)
		{
			*pPassword = _strdup(PasswordA);

			if (!(*pPassword))
				goto cleanup;
		}

		success = TRUE;
	}
	else
	{
		if (!sspi_GetAuthIdentityUserDomainW(identity, &UserW, &UserLength, &DomainW,
		                                     &DomainLength))
			goto cleanup;

		if (!sspi_GetAuthIdentityPasswordW(identity, &PasswordW, &PasswordLength))
			goto cleanup;

		if (UserW && (UserLength > 0))
		{
			*pUser = ConvertWCharNToUtf8Alloc(UserW, UserLength, NULL);
			if (!(*pUser))
				goto cleanup;
		}

		if (DomainW && (DomainLength > 0))
		{
			*pDomain = ConvertWCharNToUtf8Alloc(DomainW, DomainLength, NULL);
			if (!(*pDomain))
				goto cleanup;
		}

		if (PasswordW && (PasswordLength > 0))
		{
			*pPassword = ConvertWCharNToUtf8Alloc(PasswordW, PasswordLength, NULL);
			if (!(*pPassword))
				goto cleanup;
		}

		success = TRUE;
	}

cleanup:
	return success;
}

BOOL sspi_CopyAuthIdentityFieldsW(const SEC_WINNT_AUTH_IDENTITY_INFO* identity, WCHAR** pUser,
                                  WCHAR** pDomain, WCHAR** pPassword)
{
	BOOL success = FALSE;
	const char* UserA = NULL;
	const char* DomainA = NULL;
	const char* PasswordA = NULL;
	const WCHAR* UserW = NULL;
	const WCHAR* DomainW = NULL;
	const WCHAR* PasswordW = NULL;
	UINT32 UserLength = 0;
	UINT32 DomainLength = 0;
	UINT32 PasswordLength = 0;

	if (!identity || !pUser || !pDomain || !pPassword)
		return FALSE;

	*pUser = *pDomain = *pPassword = NULL;

	UINT32 identityFlags = sspi_GetAuthIdentityFlags(identity);

	if (identityFlags & SEC_WINNT_AUTH_IDENTITY_ANSI)
	{
		if (!sspi_GetAuthIdentityUserDomainA(identity, &UserA, &UserLength, &DomainA,
		                                     &DomainLength))
			goto cleanup;

		if (!sspi_GetAuthIdentityPasswordA(identity, &PasswordA, &PasswordLength))
			goto cleanup;

		if (UserA && (UserLength > 0))
		{
			WCHAR* ptr = ConvertUtf8NToWCharAlloc(UserA, UserLength, NULL);
			*pUser = ptr;

			if (!ptr)
				goto cleanup;
		}

		if (DomainA && (DomainLength > 0))
		{
			WCHAR* ptr = ConvertUtf8NToWCharAlloc(DomainA, DomainLength, NULL);
			*pDomain = ptr;
			if (!ptr)
				goto cleanup;
		}

		if (PasswordA && (PasswordLength > 0))
		{
			WCHAR* ptr = ConvertUtf8NToWCharAlloc(PasswordA, PasswordLength, NULL);

			*pPassword = ptr;
			if (!ptr)
				goto cleanup;
		}

		success = TRUE;
	}
	else
	{
		if (!sspi_GetAuthIdentityUserDomainW(identity, &UserW, &UserLength, &DomainW,
		                                     &DomainLength))
			goto cleanup;

		if (!sspi_GetAuthIdentityPasswordW(identity, &PasswordW, &PasswordLength))
			goto cleanup;

		if (UserW && UserLength)
		{
			*pUser = _wcsdup(UserW);

			if (!(*pUser))
				goto cleanup;
		}

		if (DomainW && DomainLength)
		{
			*pDomain = _wcsdup(DomainW);

			if (!(*pDomain))
				goto cleanup;
		}

		if (PasswordW && PasswordLength)
		{
			*pPassword = _wcsdup(PasswordW);

			if (!(*pPassword))
				goto cleanup;
		}

		success = TRUE;
	}

cleanup:
	return success;
}

BOOL sspi_CopyAuthPackageListA(const SEC_WINNT_AUTH_IDENTITY_INFO* identity, char** pPackageList)
{
	UINT32 version;
	UINT32 identityFlags;
	char* PackageList = NULL;
	const char* PackageListA = NULL;
	const WCHAR* PackageListW = NULL;
	UINT32 PackageListLength = 0;
	UINT32 PackageListOffset = 0;
	const void* pAuthData = (const void*)identity;

	if (!pAuthData)
		return FALSE;

	version = sspi_GetAuthIdentityVersion(pAuthData);
	identityFlags = sspi_GetAuthIdentityFlags(pAuthData);

	if (identityFlags & SEC_WINNT_AUTH_IDENTITY_ANSI)
	{
		if (version == SEC_WINNT_AUTH_IDENTITY_VERSION)
		{
			const SEC_WINNT_AUTH_IDENTITY_EXA* ad = (const SEC_WINNT_AUTH_IDENTITY_EXA*)pAuthData;
			PackageListA = (const char*)ad->PackageList;
			PackageListLength = ad->PackageListLength;
		}

		if (PackageListA && PackageListLength)
		{
			PackageList = _strdup(PackageListA);
		}
	}
	else
	{
		if (version == SEC_WINNT_AUTH_IDENTITY_VERSION)
		{
			const SEC_WINNT_AUTH_IDENTITY_EXW* ad = (const SEC_WINNT_AUTH_IDENTITY_EXW*)pAuthData;
			PackageListW = (const WCHAR*)ad->PackageList;
			PackageListLength = ad->PackageListLength;
		}
		else if (version == SEC_WINNT_AUTH_IDENTITY_VERSION_2)
		{
			const SEC_WINNT_AUTH_IDENTITY_EX2* ad = (const SEC_WINNT_AUTH_IDENTITY_EX2*)pAuthData;
			PackageListOffset = ad->PackageListOffset;
			PackageListW = (const WCHAR*)&((const uint8_t*)pAuthData)[PackageListOffset];
			PackageListLength = ad->PackageListLength / 2;
		}

		if (PackageListW && (PackageListLength > 0))
			PackageList = ConvertWCharNToUtf8Alloc(PackageListW, PackageListLength, NULL);
	}

	if (PackageList)
	{
		*pPackageList = PackageList;
		return TRUE;
	}

	return FALSE;
}

int sspi_CopyAuthIdentity(SEC_WINNT_AUTH_IDENTITY* identity,
                          const SEC_WINNT_AUTH_IDENTITY_INFO* srcIdentity)
{
	int status;
	UINT32 identityFlags;
	const char* UserA = NULL;
	const char* DomainA = NULL;
	const char* PasswordA = NULL;
	const WCHAR* UserW = NULL;
	const WCHAR* DomainW = NULL;
	const WCHAR* PasswordW = NULL;
	UINT32 UserLength = 0;
	UINT32 DomainLength = 0;
	UINT32 PasswordLength = 0;

	sspi_FreeAuthIdentity(identity);

	identityFlags = sspi_GetAuthIdentityFlags(srcIdentity);

	identity->Flags = identityFlags;

	if (identityFlags & SEC_WINNT_AUTH_IDENTITY_ANSI)
	{
		if (!sspi_GetAuthIdentityUserDomainA(srcIdentity, &UserA, &UserLength, &DomainA,
		                                     &DomainLength))
		{
			return -1;
		}

		if (!sspi_GetAuthIdentityPasswordA(srcIdentity, &PasswordA, &PasswordLength))
		{
			return -1;
		}

		status = sspi_SetAuthIdentity(identity, UserA, DomainA, PasswordA);

		if (status <= 0)
			return -1;

		identity->Flags &= ~SEC_WINNT_AUTH_IDENTITY_ANSI;
		identity->Flags |= SEC_WINNT_AUTH_IDENTITY_UNICODE;
		return 1;
	}

	identity->Flags |= SEC_WINNT_AUTH_IDENTITY_UNICODE;

	if (!sspi_GetAuthIdentityUserDomainW(srcIdentity, &UserW, &UserLength, &DomainW, &DomainLength))
	{
		return -1;
	}

	if (!sspi_GetAuthIdentityPasswordW(srcIdentity, &PasswordW, &PasswordLength))
	{
		return -1;
	}

	/* login/password authentication */
	identity->UserLength = UserLength;

	if (identity->UserLength > 0)
	{
		identity->User = (UINT16*)calloc((identity->UserLength + 1), sizeof(WCHAR));

		if (!identity->User)
			return -1;

		CopyMemory(identity->User, UserW, identity->UserLength * sizeof(WCHAR));
		identity->User[identity->UserLength] = 0;
	}

	identity->DomainLength = DomainLength;

	if (identity->DomainLength > 0)
	{
		identity->Domain = (UINT16*)calloc((identity->DomainLength + 1), sizeof(WCHAR));

		if (!identity->Domain)
			return -1;

		CopyMemory(identity->Domain, DomainW, identity->DomainLength * sizeof(WCHAR));
		identity->Domain[identity->DomainLength] = 0;
	}

	identity->PasswordLength = PasswordLength;

	if (identity->PasswordLength > SSPI_CREDENTIALS_HASH_LENGTH_OFFSET)
		identity->PasswordLength -= SSPI_CREDENTIALS_HASH_LENGTH_OFFSET;

	if (PasswordW)
	{
		identity->Password = (UINT16*)calloc((identity->PasswordLength + 1), sizeof(WCHAR));

		if (!identity->Password)
			return -1;

		CopyMemory(identity->Password, PasswordW, identity->PasswordLength * sizeof(WCHAR));
		identity->Password[identity->PasswordLength] = 0;
	}

	identity->PasswordLength = PasswordLength;
	/* End of login/password authentication */
	return 1;
}

PSecBuffer sspi_FindSecBuffer(PSecBufferDesc pMessage, ULONG BufferType)
{
	ULONG index;
	PSecBuffer pSecBuffer = NULL;

	for (index = 0; index < pMessage->cBuffers; index++)
	{
		if (pMessage->pBuffers[index].BufferType == BufferType)
		{
			pSecBuffer = &pMessage->pBuffers[index];
			break;
		}
	}

	return pSecBuffer;
}

static BOOL CALLBACK sspi_init(PINIT_ONCE InitOnce, PVOID Parameter, PVOID* Context)
{
	winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);
	sspi_ContextBufferAllocTableNew();
	return TRUE;
}

void sspi_GlobalInit(void)
{
	static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
	DWORD flags = 0;
	InitOnceExecuteOnce(&once, sspi_init, &flags, NULL);
}

void sspi_GlobalFinish(void)
{
	sspi_ContextBufferAllocTableFree();
}

static const SecurityFunctionTableA* sspi_GetSecurityFunctionTableAByNameA(const SEC_CHAR* Name)
{
	int index;
	UINT32 cPackages;
	cPackages = sizeof(SecPkgInfoA_LIST) / sizeof(*(SecPkgInfoA_LIST));

	for (index = 0; index < (int)cPackages; index++)
	{
		if (strcmp(Name, SecurityFunctionTableA_NAME_LIST[index].Name) == 0)
		{
			return (const SecurityFunctionTableA*)SecurityFunctionTableA_NAME_LIST[index]
			    .SecurityFunctionTable;
		}
	}

	return NULL;
}

static const SecurityFunctionTableW* sspi_GetSecurityFunctionTableWByNameW(const SEC_WCHAR* Name)
{
	int index;
	UINT32 cPackages;
	cPackages = sizeof(SecPkgInfoW_LIST) / sizeof(*(SecPkgInfoW_LIST));

	for (index = 0; index < (int)cPackages; index++)
	{
		if (lstrcmpW(Name, SecurityFunctionTableW_NAME_LIST[index].Name) == 0)
		{
			return (const SecurityFunctionTableW*)SecurityFunctionTableW_NAME_LIST[index]
			    .SecurityFunctionTable;
		}
	}

	return NULL;
}

static const SecurityFunctionTableW* sspi_GetSecurityFunctionTableWByNameA(const SEC_CHAR* Name)
{
	SEC_WCHAR* NameW = NULL;
	const SecurityFunctionTableW* table = NULL;

	if (!Name)
		return NULL;

	NameW = ConvertUtf8ToWCharAlloc(Name, NULL);

	if (!NameW)
		return NULL;

	table = sspi_GetSecurityFunctionTableWByNameW(NameW);
	free(NameW);
	return table;
}

static void FreeContextBuffer_EnumerateSecurityPackages(void* contextBuffer);
static void FreeContextBuffer_QuerySecurityPackageInfo(void* contextBuffer);

static void sspi_ContextBufferFree(void* contextBuffer)
{
	UINT32 index;
	UINT32 allocatorIndex;

	for (index = 0; index < ContextBufferAllocTable.cMaxEntries; index++)
	{
		if (contextBuffer == ContextBufferAllocTable.entries[index].contextBuffer)
		{
			contextBuffer = ContextBufferAllocTable.entries[index].contextBuffer;
			allocatorIndex = ContextBufferAllocTable.entries[index].allocatorIndex;
			ContextBufferAllocTable.cEntries--;
			ContextBufferAllocTable.entries[index].allocatorIndex = 0;
			ContextBufferAllocTable.entries[index].contextBuffer = NULL;

			switch (allocatorIndex)
			{
				case EnumerateSecurityPackagesIndex:
					FreeContextBuffer_EnumerateSecurityPackages(contextBuffer);
					break;

				case QuerySecurityPackageInfoIndex:
					FreeContextBuffer_QuerySecurityPackageInfo(contextBuffer);
					break;
			}
		}
	}
}

/**
 * Standard SSPI API
 */

/* Package Management */

static SECURITY_STATUS SEC_ENTRY winpr_EnumerateSecurityPackagesW(ULONG* pcPackages,
                                                                  PSecPkgInfoW* ppPackageInfo)
{
	int index;
	size_t size;
	UINT32 cPackages;
	SecPkgInfoW* pPackageInfo;
	cPackages = sizeof(SecPkgInfoW_LIST) / sizeof(*(SecPkgInfoW_LIST));
	size = sizeof(SecPkgInfoW) * cPackages;
	pPackageInfo = (SecPkgInfoW*)sspi_ContextBufferAlloc(EnumerateSecurityPackagesIndex, size);

	if (!pPackageInfo)
		return SEC_E_INSUFFICIENT_MEMORY;

	for (index = 0; index < (int)cPackages; index++)
	{
		pPackageInfo[index].fCapabilities = SecPkgInfoW_LIST[index]->fCapabilities;
		pPackageInfo[index].wVersion = SecPkgInfoW_LIST[index]->wVersion;
		pPackageInfo[index].wRPCID = SecPkgInfoW_LIST[index]->wRPCID;
		pPackageInfo[index].cbMaxToken = SecPkgInfoW_LIST[index]->cbMaxToken;
		pPackageInfo[index].Name = _wcsdup(SecPkgInfoW_LIST[index]->Name);
		pPackageInfo[index].Comment = _wcsdup(SecPkgInfoW_LIST[index]->Comment);
	}

	*(pcPackages) = cPackages;
	*(ppPackageInfo) = pPackageInfo;
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY winpr_EnumerateSecurityPackagesA(ULONG* pcPackages,
                                                                  PSecPkgInfoA* ppPackageInfo)
{
	int index;
	size_t size;
	UINT32 cPackages;
	SecPkgInfoA* pPackageInfo;
	cPackages = sizeof(SecPkgInfoA_LIST) / sizeof(*(SecPkgInfoA_LIST));
	size = sizeof(SecPkgInfoA) * cPackages;
	pPackageInfo = (SecPkgInfoA*)sspi_ContextBufferAlloc(EnumerateSecurityPackagesIndex, size);

	if (!pPackageInfo)
		return SEC_E_INSUFFICIENT_MEMORY;

	for (index = 0; index < (int)cPackages; index++)
	{
		pPackageInfo[index].fCapabilities = SecPkgInfoA_LIST[index]->fCapabilities;
		pPackageInfo[index].wVersion = SecPkgInfoA_LIST[index]->wVersion;
		pPackageInfo[index].wRPCID = SecPkgInfoA_LIST[index]->wRPCID;
		pPackageInfo[index].cbMaxToken = SecPkgInfoA_LIST[index]->cbMaxToken;
		pPackageInfo[index].Name = _strdup(SecPkgInfoA_LIST[index]->Name);
		pPackageInfo[index].Comment = _strdup(SecPkgInfoA_LIST[index]->Comment);

		if (!pPackageInfo[index].Name || !pPackageInfo[index].Comment)
		{
			sspi_ContextBufferFree(pPackageInfo);
			return SEC_E_INSUFFICIENT_MEMORY;
		}
	}

	*(pcPackages) = cPackages;
	*(ppPackageInfo) = pPackageInfo;
	return SEC_E_OK;
}

static void FreeContextBuffer_EnumerateSecurityPackages(void* contextBuffer)
{
	int index;
	UINT32 cPackages;
	SecPkgInfoA* pPackageInfo = (SecPkgInfoA*)contextBuffer;
	cPackages = sizeof(SecPkgInfoA_LIST) / sizeof(*(SecPkgInfoA_LIST));

	if (!pPackageInfo)
		return;

	for (index = 0; index < (int)cPackages; index++)
	{
		free(pPackageInfo[index].Name);
		free(pPackageInfo[index].Comment);
	}

	free(pPackageInfo);
}

SecurityFunctionTableW* SEC_ENTRY winpr_InitSecurityInterfaceW(void)
{
	return &winpr_SecurityFunctionTableW;
}

SecurityFunctionTableA* SEC_ENTRY winpr_InitSecurityInterfaceA(void)
{
	return &winpr_SecurityFunctionTableA;
}

static SECURITY_STATUS SEC_ENTRY winpr_QuerySecurityPackageInfoW(SEC_WCHAR* pszPackageName,
                                                                 PSecPkgInfoW* ppPackageInfo)
{
	int index;
	size_t size;
	UINT32 cPackages;
	SecPkgInfoW* pPackageInfo;
	cPackages = sizeof(SecPkgInfoW_LIST) / sizeof(*(SecPkgInfoW_LIST));

	for (index = 0; index < (int)cPackages; index++)
	{
		if (lstrcmpW(pszPackageName, SecPkgInfoW_LIST[index]->Name) == 0)
		{
			size = sizeof(SecPkgInfoW);
			pPackageInfo =
			    (SecPkgInfoW*)sspi_ContextBufferAlloc(QuerySecurityPackageInfoIndex, size);

			if (!pPackageInfo)
				return SEC_E_INSUFFICIENT_MEMORY;

			pPackageInfo->fCapabilities = SecPkgInfoW_LIST[index]->fCapabilities;
			pPackageInfo->wVersion = SecPkgInfoW_LIST[index]->wVersion;
			pPackageInfo->wRPCID = SecPkgInfoW_LIST[index]->wRPCID;
			pPackageInfo->cbMaxToken = SecPkgInfoW_LIST[index]->cbMaxToken;
			pPackageInfo->Name = _wcsdup(SecPkgInfoW_LIST[index]->Name);
			pPackageInfo->Comment = _wcsdup(SecPkgInfoW_LIST[index]->Comment);
			*(ppPackageInfo) = pPackageInfo;
			return SEC_E_OK;
		}
	}

	*(ppPackageInfo) = NULL;
	return SEC_E_SECPKG_NOT_FOUND;
}

static SECURITY_STATUS SEC_ENTRY winpr_QuerySecurityPackageInfoA(SEC_CHAR* pszPackageName,
                                                                 PSecPkgInfoA* ppPackageInfo)
{
	int index;
	size_t size;
	UINT32 cPackages;
	SecPkgInfoA* pPackageInfo;
	cPackages = sizeof(SecPkgInfoA_LIST) / sizeof(*(SecPkgInfoA_LIST));

	for (index = 0; index < (int)cPackages; index++)
	{
		if (strcmp(pszPackageName, SecPkgInfoA_LIST[index]->Name) == 0)
		{
			size = sizeof(SecPkgInfoA);
			pPackageInfo =
			    (SecPkgInfoA*)sspi_ContextBufferAlloc(QuerySecurityPackageInfoIndex, size);

			if (!pPackageInfo)
				return SEC_E_INSUFFICIENT_MEMORY;

			pPackageInfo->fCapabilities = SecPkgInfoA_LIST[index]->fCapabilities;
			pPackageInfo->wVersion = SecPkgInfoA_LIST[index]->wVersion;
			pPackageInfo->wRPCID = SecPkgInfoA_LIST[index]->wRPCID;
			pPackageInfo->cbMaxToken = SecPkgInfoA_LIST[index]->cbMaxToken;
			pPackageInfo->Name = _strdup(SecPkgInfoA_LIST[index]->Name);
			pPackageInfo->Comment = _strdup(SecPkgInfoA_LIST[index]->Comment);

			if (!pPackageInfo->Name || !pPackageInfo->Comment)
			{
				sspi_ContextBufferFree(pPackageInfo);
				return SEC_E_INSUFFICIENT_MEMORY;
			}

			*(ppPackageInfo) = pPackageInfo;
			return SEC_E_OK;
		}
	}

	*(ppPackageInfo) = NULL;
	return SEC_E_SECPKG_NOT_FOUND;
}

void FreeContextBuffer_QuerySecurityPackageInfo(void* contextBuffer)
{
	SecPkgInfo* pPackageInfo = (SecPkgInfo*)contextBuffer;

	if (!pPackageInfo)
		return;

	free(pPackageInfo->Name);
	free(pPackageInfo->Comment);
	free(pPackageInfo);
}

/* Credential Management */

static SECURITY_STATUS SEC_ENTRY winpr_AcquireCredentialsHandleW(
    SEC_WCHAR* pszPrincipal, SEC_WCHAR* pszPackage, ULONG fCredentialUse, void* pvLogonID,
    void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
    PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	const SecurityFunctionTableW* table = sspi_GetSecurityFunctionTableWByNameW(pszPackage);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->AcquireCredentialsHandleW)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->AcquireCredentialsHandleW(pszPrincipal, pszPackage, fCredentialUse, pvLogonID,
	                                          pAuthData, pGetKeyFn, pvGetKeyArgument, phCredential,
	                                          ptsExpiry);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "AcquireCredentialsHandleW status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_AcquireCredentialsHandleA(
    SEC_CHAR* pszPrincipal, SEC_CHAR* pszPackage, ULONG fCredentialUse, void* pvLogonID,
    void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
    PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	const SecurityFunctionTableA* table = sspi_GetSecurityFunctionTableAByNameA(pszPackage);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->AcquireCredentialsHandleA)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->AcquireCredentialsHandleA(pszPrincipal, pszPackage, fCredentialUse, pvLogonID,
	                                          pAuthData, pGetKeyFn, pvGetKeyArgument, phCredential,
	                                          ptsExpiry);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "AcquireCredentialsHandleA status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_ExportSecurityContext(PCtxtHandle phContext, ULONG fFlags,
                                                             PSecBuffer pPackedContext,
                                                             HANDLE* pToken)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	const SecurityFunctionTableW* table;
	Name = (SEC_CHAR*)sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableWByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->ExportSecurityContext)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->ExportSecurityContext(phContext, fFlags, pPackedContext, pToken);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "ExportSecurityContext status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_FreeCredentialsHandle(PCredHandle phCredential)
{
	char* Name;
	SECURITY_STATUS status;
	const SecurityFunctionTableA* table;
	Name = (char*)sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->FreeCredentialsHandle)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->FreeCredentialsHandle(phCredential);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "FreeCredentialsHandle status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_ImportSecurityContextW(SEC_WCHAR* pszPackage,
                                                              PSecBuffer pPackedContext,
                                                              HANDLE pToken, PCtxtHandle phContext)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	const SecurityFunctionTableW* table;
	Name = (SEC_CHAR*)sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableWByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->ImportSecurityContextW)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->ImportSecurityContextW(pszPackage, pPackedContext, pToken, phContext);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "ImportSecurityContextW status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_ImportSecurityContextA(SEC_CHAR* pszPackage,
                                                              PSecBuffer pPackedContext,
                                                              HANDLE pToken, PCtxtHandle phContext)
{
	char* Name = NULL;
	SECURITY_STATUS status;
	const SecurityFunctionTableA* table;
	Name = (char*)sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->ImportSecurityContextA)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->ImportSecurityContextA(pszPackage, pPackedContext, pToken, phContext);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "ImportSecurityContextA status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_QueryCredentialsAttributesW(PCredHandle phCredential,
                                                                   ULONG ulAttribute, void* pBuffer)
{
	SEC_WCHAR* Name;
	SECURITY_STATUS status;
	const SecurityFunctionTableW* table;
	Name = (SEC_WCHAR*)sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableWByNameW(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->QueryCredentialsAttributesW)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->QueryCredentialsAttributesW(phCredential, ulAttribute, pBuffer);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "QueryCredentialsAttributesW status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_QueryCredentialsAttributesA(PCredHandle phCredential,
                                                                   ULONG ulAttribute, void* pBuffer)
{
	char* Name;
	SECURITY_STATUS status;
	const SecurityFunctionTableA* table;
	Name = (char*)sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->QueryCredentialsAttributesA)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->QueryCredentialsAttributesA(phCredential, ulAttribute, pBuffer);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "QueryCredentialsAttributesA status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_SetCredentialsAttributesW(PCredHandle phCredential,
                                                                 ULONG ulAttribute, void* pBuffer,
                                                                 ULONG cbBuffer)
{
	SEC_WCHAR* Name;
	SECURITY_STATUS status;
	const SecurityFunctionTableW* table;
	Name = (SEC_WCHAR*)sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableWByNameW(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->SetCredentialsAttributesW)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->SetCredentialsAttributesW(phCredential, ulAttribute, pBuffer, cbBuffer);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "SetCredentialsAttributesW status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_SetCredentialsAttributesA(PCredHandle phCredential,
                                                                 ULONG ulAttribute, void* pBuffer,
                                                                 ULONG cbBuffer)
{
	char* Name;
	SECURITY_STATUS status;
	const SecurityFunctionTableA* table;
	Name = (char*)sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->SetCredentialsAttributesA)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->SetCredentialsAttributesA(phCredential, ulAttribute, pBuffer, cbBuffer);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "SetCredentialsAttributesA status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

/* Context Management */

static SECURITY_STATUS SEC_ENTRY
winpr_AcceptSecurityContext(PCredHandle phCredential, PCtxtHandle phContext, PSecBufferDesc pInput,
                            ULONG fContextReq, ULONG TargetDataRep, PCtxtHandle phNewContext,
                            PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsTimeStamp)
{
	char* Name;
	SECURITY_STATUS status;
	const SecurityFunctionTableA* table;
	Name = (char*)sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->AcceptSecurityContext)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status =
	    table->AcceptSecurityContext(phCredential, phContext, pInput, fContextReq, TargetDataRep,
	                                 phNewContext, pOutput, pfContextAttr, ptsTimeStamp);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "AcceptSecurityContext status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_ApplyControlToken(PCtxtHandle phContext,
                                                         PSecBufferDesc pInput)
{
	char* Name = NULL;
	SECURITY_STATUS status;
	const SecurityFunctionTableA* table;
	Name = (char*)sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->ApplyControlToken)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->ApplyControlToken(phContext, pInput);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "ApplyControlToken status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_CompleteAuthToken(PCtxtHandle phContext,
                                                         PSecBufferDesc pToken)
{
	char* Name = NULL;
	SECURITY_STATUS status;
	const SecurityFunctionTableA* table;
	Name = (char*)sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->CompleteAuthToken)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->CompleteAuthToken(phContext, pToken);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "CompleteAuthToken status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_DeleteSecurityContext(PCtxtHandle phContext)
{
	char* Name = NULL;
	SECURITY_STATUS status;
	const SecurityFunctionTableA* table;
	Name = (char*)sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->DeleteSecurityContext)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->DeleteSecurityContext(phContext);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "DeleteSecurityContext status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_FreeContextBuffer(void* pvContextBuffer)
{
	if (!pvContextBuffer)
		return SEC_E_INVALID_HANDLE;

	sspi_ContextBufferFree(pvContextBuffer);
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY winpr_ImpersonateSecurityContext(PCtxtHandle phContext)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	const SecurityFunctionTableW* table;
	Name = (SEC_CHAR*)sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableWByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->ImpersonateSecurityContext)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->ImpersonateSecurityContext(phContext);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "ImpersonateSecurityContext status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_InitializeSecurityContextW(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_WCHAR* pszTargetName, ULONG fContextReq,
    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	const SecurityFunctionTableW* table;
	Name = (SEC_CHAR*)sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableWByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->InitializeSecurityContextW)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->InitializeSecurityContextW(phCredential, phContext, pszTargetName, fContextReq,
	                                           Reserved1, TargetDataRep, pInput, Reserved2,
	                                           phNewContext, pOutput, pfContextAttr, ptsExpiry);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "InitializeSecurityContextW status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_InitializeSecurityContextA(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_CHAR* pszTargetName, ULONG fContextReq,
    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	const SecurityFunctionTableA* table;
	Name = (SEC_CHAR*)sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->InitializeSecurityContextA)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->InitializeSecurityContextA(phCredential, phContext, pszTargetName, fContextReq,
	                                           Reserved1, TargetDataRep, pInput, Reserved2,
	                                           phNewContext, pOutput, pfContextAttr, ptsExpiry);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "InitializeSecurityContextA status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_QueryContextAttributesW(PCtxtHandle phContext,
                                                               ULONG ulAttribute, void* pBuffer)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	const SecurityFunctionTableW* table;
	Name = (SEC_CHAR*)sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableWByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->QueryContextAttributesW)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->QueryContextAttributesW(phContext, ulAttribute, pBuffer);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "QueryContextAttributesW status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_QueryContextAttributesA(PCtxtHandle phContext,
                                                               ULONG ulAttribute, void* pBuffer)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	const SecurityFunctionTableA* table;
	Name = (SEC_CHAR*)sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->QueryContextAttributesA)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->QueryContextAttributesA(phContext, ulAttribute, pBuffer);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "QueryContextAttributesA status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_QuerySecurityContextToken(PCtxtHandle phContext,
                                                                 HANDLE* phToken)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	const SecurityFunctionTableW* table;
	Name = (SEC_CHAR*)sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableWByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->QuerySecurityContextToken)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->QuerySecurityContextToken(phContext, phToken);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "QuerySecurityContextToken status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_SetContextAttributesW(PCtxtHandle phContext,
                                                             ULONG ulAttribute, void* pBuffer,
                                                             ULONG cbBuffer)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	const SecurityFunctionTableW* table;
	Name = (SEC_CHAR*)sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableWByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->SetContextAttributesW)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->SetContextAttributesW(phContext, ulAttribute, pBuffer, cbBuffer);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "SetContextAttributesW status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_SetContextAttributesA(PCtxtHandle phContext,
                                                             ULONG ulAttribute, void* pBuffer,
                                                             ULONG cbBuffer)
{
	char* Name;
	SECURITY_STATUS status;
	const SecurityFunctionTableA* table;
	Name = (char*)sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->SetContextAttributesA)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->SetContextAttributesA(phContext, ulAttribute, pBuffer, cbBuffer);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "SetContextAttributesA status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_RevertSecurityContext(PCtxtHandle phContext)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	const SecurityFunctionTableW* table;
	Name = (SEC_CHAR*)sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableWByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->RevertSecurityContext)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->RevertSecurityContext(phContext);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "RevertSecurityContext status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

/* Message Support */

static SECURITY_STATUS SEC_ENTRY winpr_DecryptMessage(PCtxtHandle phContext,
                                                      PSecBufferDesc pMessage, ULONG MessageSeqNo,
                                                      PULONG pfQOP)
{
	char* Name;
	SECURITY_STATUS status;
	const SecurityFunctionTableA* table;
	Name = (char*)sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->DecryptMessage)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->DecryptMessage(phContext, pMessage, MessageSeqNo, pfQOP);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "DecryptMessage status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_EncryptMessage(PCtxtHandle phContext, ULONG fQOP,
                                                      PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
	char* Name;
	SECURITY_STATUS status;
	const SecurityFunctionTableA* table;
	Name = (char*)sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->EncryptMessage)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->EncryptMessage(phContext, fQOP, pMessage, MessageSeqNo);

	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "EncryptMessage status %s [0x%08" PRIX32 "]", GetSecurityStatusString(status),
		         status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_MakeSignature(PCtxtHandle phContext, ULONG fQOP,
                                                     PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
	char* Name;
	SECURITY_STATUS status;
	const SecurityFunctionTableA* table;
	Name = (char*)sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->MakeSignature)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->MakeSignature(phContext, fQOP, pMessage, MessageSeqNo);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "MakeSignature status %s [0x%08" PRIX32 "]", GetSecurityStatusString(status),
		          status);
	}

	return status;
}

static SECURITY_STATUS SEC_ENTRY winpr_VerifySignature(PCtxtHandle phContext,
                                                       PSecBufferDesc pMessage, ULONG MessageSeqNo,
                                                       PULONG pfQOP)
{
	char* Name;
	SECURITY_STATUS status;
	const SecurityFunctionTableA* table;
	Name = (char*)sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->VerifySignature)
	{
		WLog_WARN(TAG, "[%s]: Security module does not provide an implementation", __FUNCTION__);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}

	status = table->VerifySignature(phContext, pMessage, MessageSeqNo, pfQOP);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "VerifySignature status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
	}

	return status;
}

static SecurityFunctionTableA winpr_SecurityFunctionTableA = {
	3,                                 /* dwVersion */
	winpr_EnumerateSecurityPackagesA,  /* EnumerateSecurityPackages */
	winpr_QueryCredentialsAttributesA, /* QueryCredentialsAttributes */
	winpr_AcquireCredentialsHandleA,   /* AcquireCredentialsHandle */
	winpr_FreeCredentialsHandle,       /* FreeCredentialsHandle */
	NULL,                              /* Reserved2 */
	winpr_InitializeSecurityContextA,  /* InitializeSecurityContext */
	winpr_AcceptSecurityContext,       /* AcceptSecurityContext */
	winpr_CompleteAuthToken,           /* CompleteAuthToken */
	winpr_DeleteSecurityContext,       /* DeleteSecurityContext */
	winpr_ApplyControlToken,           /* ApplyControlToken */
	winpr_QueryContextAttributesA,     /* QueryContextAttributes */
	winpr_ImpersonateSecurityContext,  /* ImpersonateSecurityContext */
	winpr_RevertSecurityContext,       /* RevertSecurityContext */
	winpr_MakeSignature,               /* MakeSignature */
	winpr_VerifySignature,             /* VerifySignature */
	winpr_FreeContextBuffer,           /* FreeContextBuffer */
	winpr_QuerySecurityPackageInfoA,   /* QuerySecurityPackageInfo */
	NULL,                              /* Reserved3 */
	NULL,                              /* Reserved4 */
	winpr_ExportSecurityContext,       /* ExportSecurityContext */
	winpr_ImportSecurityContextA,      /* ImportSecurityContext */
	NULL,                              /* AddCredentials */
	NULL,                              /* Reserved8 */
	winpr_QuerySecurityContextToken,   /* QuerySecurityContextToken */
	winpr_EncryptMessage,              /* EncryptMessage */
	winpr_DecryptMessage,              /* DecryptMessage */
	winpr_SetContextAttributesA,       /* SetContextAttributes */
	winpr_SetCredentialsAttributesA,   /* SetCredentialsAttributes */
};

static SecurityFunctionTableW winpr_SecurityFunctionTableW = {
	3,                                 /* dwVersion */
	winpr_EnumerateSecurityPackagesW,  /* EnumerateSecurityPackages */
	winpr_QueryCredentialsAttributesW, /* QueryCredentialsAttributes */
	winpr_AcquireCredentialsHandleW,   /* AcquireCredentialsHandle */
	winpr_FreeCredentialsHandle,       /* FreeCredentialsHandle */
	NULL,                              /* Reserved2 */
	winpr_InitializeSecurityContextW,  /* InitializeSecurityContext */
	winpr_AcceptSecurityContext,       /* AcceptSecurityContext */
	winpr_CompleteAuthToken,           /* CompleteAuthToken */
	winpr_DeleteSecurityContext,       /* DeleteSecurityContext */
	winpr_ApplyControlToken,           /* ApplyControlToken */
	winpr_QueryContextAttributesW,     /* QueryContextAttributes */
	winpr_ImpersonateSecurityContext,  /* ImpersonateSecurityContext */
	winpr_RevertSecurityContext,       /* RevertSecurityContext */
	winpr_MakeSignature,               /* MakeSignature */
	winpr_VerifySignature,             /* VerifySignature */
	winpr_FreeContextBuffer,           /* FreeContextBuffer */
	winpr_QuerySecurityPackageInfoW,   /* QuerySecurityPackageInfo */
	NULL,                              /* Reserved3 */
	NULL,                              /* Reserved4 */
	winpr_ExportSecurityContext,       /* ExportSecurityContext */
	winpr_ImportSecurityContextW,      /* ImportSecurityContext */
	NULL,                              /* AddCredentials */
	NULL,                              /* Reserved8 */
	winpr_QuerySecurityContextToken,   /* QuerySecurityContextToken */
	winpr_EncryptMessage,              /* EncryptMessage */
	winpr_DecryptMessage,              /* DecryptMessage */
	winpr_SetContextAttributesW,       /* SetContextAttributes */
	winpr_SetCredentialsAttributesW,   /* SetCredentialsAttributes */
};

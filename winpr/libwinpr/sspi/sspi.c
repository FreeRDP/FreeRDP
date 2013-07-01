/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Security Support Provider Interface (SSPI)
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

#include <stdlib.h>

#include <winpr/windows.h>

#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/print.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "sspi.h"

/* Authentication Functions: http://msdn.microsoft.com/en-us/library/windows/desktop/aa374731/ */

#ifdef WINPR_SSPI

extern const SecPkgInfoA NTLM_SecPkgInfoA;
extern const SecPkgInfoW NTLM_SecPkgInfoW;
extern const SecurityFunctionTableA NTLM_SecurityFunctionTableA;
extern const SecurityFunctionTableW NTLM_SecurityFunctionTableW;

extern const SecPkgInfoA CREDSSP_SecPkgInfoA;
extern const SecPkgInfoW CREDSSP_SecPkgInfoW;
extern const SecurityFunctionTableA CREDSSP_SecurityFunctionTableA;
extern const SecurityFunctionTableW CREDSSP_SecurityFunctionTableW;

extern const SecPkgInfoA SCHANNEL_SecPkgInfoA;
extern const SecPkgInfoW SCHANNEL_SecPkgInfoW;
extern const SecurityFunctionTableA SCHANNEL_SecurityFunctionTableA;
extern const SecurityFunctionTableW SCHANNEL_SecurityFunctionTableW;

const SecPkgInfoA* SecPkgInfoA_LIST[] =
{
	&NTLM_SecPkgInfoA,
	&CREDSSP_SecPkgInfoA,
	&SCHANNEL_SecPkgInfoA
};

const SecPkgInfoW* SecPkgInfoW_LIST[] =
{
	&NTLM_SecPkgInfoW,
	&CREDSSP_SecPkgInfoW,
	&SCHANNEL_SecPkgInfoW
};

SecurityFunctionTableA SSPI_SecurityFunctionTableA;
SecurityFunctionTableW SSPI_SecurityFunctionTableW;

struct _SecurityFunctionTableA_NAME
{
	SEC_CHAR* Name;
	const SecurityFunctionTableA* SecurityFunctionTable;
};
typedef struct _SecurityFunctionTableA_NAME SecurityFunctionTableA_NAME;

struct _SecurityFunctionTableW_NAME
{
	SEC_WCHAR* Name;
	const SecurityFunctionTableW* SecurityFunctionTable;
};
typedef struct _SecurityFunctionTableW_NAME SecurityFunctionTableW_NAME;

const SecurityFunctionTableA_NAME SecurityFunctionTableA_NAME_LIST[] =
{
	{ "NTLM", &NTLM_SecurityFunctionTableA },
	{ "CREDSSP", &CREDSSP_SecurityFunctionTableA },
	{ "Schannel", &SCHANNEL_SecurityFunctionTableA }
};

WCHAR NTLM_NAME_W[] = { 'N','T','L','M','\0' };
WCHAR CREDSSP_NAME_W[] = { 'C','r','e','d','S','S','P','\0' };
WCHAR SCHANNEL_NAME_W[] = { 'S','c','h','a','n','n','e','l','\0' };

const SecurityFunctionTableW_NAME SecurityFunctionTableW_NAME_LIST[] =
{
	{ NTLM_NAME_W, &NTLM_SecurityFunctionTableW },
	{ CREDSSP_NAME_W, &CREDSSP_SecurityFunctionTableW },
	{ SCHANNEL_NAME_W, &SCHANNEL_SecurityFunctionTableW }
};

#endif

#define SecHandle_LOWER_MAX	0xFFFFFFFF
#define SecHandle_UPPER_MAX	0xFFFFFFFE

struct _CONTEXT_BUFFER_ALLOC_ENTRY
{
	void* contextBuffer;
	UINT32 allocatorIndex;
};
typedef struct _CONTEXT_BUFFER_ALLOC_ENTRY CONTEXT_BUFFER_ALLOC_ENTRY;

struct _CONTEXT_BUFFER_ALLOC_TABLE
{
	UINT32 cEntries;
	UINT32 cMaxEntries;
	CONTEXT_BUFFER_ALLOC_ENTRY* entries;
};
typedef struct _CONTEXT_BUFFER_ALLOC_TABLE CONTEXT_BUFFER_ALLOC_TABLE;

CONTEXT_BUFFER_ALLOC_TABLE ContextBufferAllocTable;

void sspi_ContextBufferAllocTableNew()
{
	size_t size;

	ContextBufferAllocTable.cEntries = 0;
	ContextBufferAllocTable.cMaxEntries = 4;

	size = sizeof(CONTEXT_BUFFER_ALLOC_ENTRY) * ContextBufferAllocTable.cMaxEntries;

	ContextBufferAllocTable.entries = malloc(size);
	ZeroMemory(ContextBufferAllocTable.entries, size);
}

void sspi_ContextBufferAllocTableGrow()
{
	size_t size;
	ContextBufferAllocTable.cEntries = 0;
	ContextBufferAllocTable.cMaxEntries *= 2;

	size = sizeof(CONTEXT_BUFFER_ALLOC_ENTRY) * ContextBufferAllocTable.cMaxEntries;

	ContextBufferAllocTable.entries = realloc(ContextBufferAllocTable.entries, size);
	ZeroMemory((void*) &ContextBufferAllocTable.entries[ContextBufferAllocTable.cMaxEntries / 2], size / 2);
}

void sspi_ContextBufferAllocTableFree()
{
	ContextBufferAllocTable.cEntries = ContextBufferAllocTable.cMaxEntries = 0;
	free(ContextBufferAllocTable.entries);
}

void* sspi_ContextBufferAlloc(UINT32 allocatorIndex, size_t size)
{
	int index;
	void* contextBuffer;

	for (index = 0; index < (int) ContextBufferAllocTable.cMaxEntries; index++)
	{
		if (ContextBufferAllocTable.entries[index].contextBuffer == NULL)
		{
			contextBuffer = malloc(size);
			ZeroMemory(contextBuffer, size);
			ContextBufferAllocTable.cEntries++;

			ContextBufferAllocTable.entries[index].contextBuffer = contextBuffer;
			ContextBufferAllocTable.entries[index].allocatorIndex = allocatorIndex;

			return ContextBufferAllocTable.entries[index].contextBuffer;
		}
	}

	/* no available entry was found, the table needs to be grown */

	sspi_ContextBufferAllocTableGrow();

	/* the next call to sspi_ContextBufferAlloc() should now succeed */

	return sspi_ContextBufferAlloc(allocatorIndex, size);
}

CREDENTIALS* sspi_CredentialsNew()
{
	CREDENTIALS* credentials;

	credentials = (CREDENTIALS*) malloc(sizeof(CREDENTIALS));
	ZeroMemory(credentials, sizeof(CREDENTIALS));

	if (credentials != NULL)
	{

	}

	return credentials;
}

void sspi_CredentialsFree(CREDENTIALS* credentials)
{
	if (!credentials)
		return;

	free(credentials);
}

void sspi_SecBufferAlloc(PSecBuffer SecBuffer, size_t size)
{
	SecBuffer->cbBuffer = size;
	SecBuffer->pvBuffer = malloc(size);
	ZeroMemory(SecBuffer->pvBuffer, SecBuffer->cbBuffer);
}

void sspi_SecBufferFree(PSecBuffer SecBuffer)
{
	free(SecBuffer->pvBuffer);
	SecBuffer->pvBuffer = NULL;
	SecBuffer->cbBuffer = 0;
}

SecHandle* sspi_SecureHandleAlloc()
{
	SecHandle* handle = (SecHandle*) malloc(sizeof(SecHandle));
	sspi_SecureHandleInit(handle);
	return handle;
}

void sspi_SecureHandleInit(SecHandle* handle)
{
	if (!handle)
		return;

	memset(handle, 0xFF, sizeof(SecHandle));
}

void sspi_SecureHandleInvalidate(SecHandle* handle)
{
	if (!handle)
		return;

	sspi_SecureHandleInit(handle);
}

void* sspi_SecureHandleGetLowerPointer(SecHandle* handle)
{
	void* pointer;

	if (!handle || !SecIsValidHandle(handle))
		return NULL;

	pointer = (void*) ~((size_t) handle->dwLower);

	return pointer;
}

void sspi_SecureHandleSetLowerPointer(SecHandle* handle, void* pointer)
{
	if (!handle)
		return;

	handle->dwLower = (ULONG_PTR) (~((size_t) pointer));
}

void* sspi_SecureHandleGetUpperPointer(SecHandle* handle)
{
	void* pointer;

	if (!handle || !SecIsValidHandle(handle))
		return NULL;

	pointer = (void*) ~((size_t) handle->dwUpper);

	return pointer;
}

void sspi_SecureHandleSetUpperPointer(SecHandle* handle, void* pointer)
{
	if (!handle)
		return;

	handle->dwUpper = (ULONG_PTR) (~((size_t) pointer));
}

void sspi_SecureHandleFree(SecHandle* handle)
{
	if (!handle)
		return;

	free(handle);
}

void sspi_SetAuthIdentity(SEC_WINNT_AUTH_IDENTITY* identity, char* user, char* domain, char* password)
{
	identity->Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;

	if (user)
	{
		identity->UserLength = ConvertToUnicode(CP_UTF8, 0, user, -1, &identity->User, 0) - 1;
	}
	else
	{
		identity->User = (UINT16*) NULL;
		identity->UserLength = 0;
	}

	if (domain)
	{
		identity->DomainLength = ConvertToUnicode(CP_UTF8, 0, domain, -1, &identity->Domain, 0) - 1;
	}
	else
	{
		identity->Domain = (UINT16*) NULL;
		identity->DomainLength = 0;
	}

	if (password != NULL)
	{
		identity->PasswordLength = ConvertToUnicode(CP_UTF8, 0, password, -1, &identity->Password, 0) - 1;
	}
	else
	{
		identity->Password = NULL;
		identity->PasswordLength = 0;
	}
}

void sspi_CopyAuthIdentity(SEC_WINNT_AUTH_IDENTITY* identity, SEC_WINNT_AUTH_IDENTITY* srcIdentity)
{
	if (identity->Flags == SEC_WINNT_AUTH_IDENTITY_ANSI)
	{
		sspi_SetAuthIdentity(identity, (char*) srcIdentity->User,
				(char*) srcIdentity->Domain, (char*) srcIdentity->Password);

		identity->Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;

		return;
	}

	identity->Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;

	identity->User = identity->Domain = identity->Password = NULL;

	identity->UserLength = srcIdentity->UserLength;

	if (identity->UserLength > 0)
	{
		identity->User = (UINT16*) malloc((identity->UserLength + 1) * sizeof(WCHAR));
		CopyMemory(identity->User, srcIdentity->User, identity->UserLength * sizeof(WCHAR));
		identity->User[identity->UserLength] = 0;
	}

	identity->DomainLength = srcIdentity->DomainLength;

	if (identity->DomainLength > 0)
	{
		identity->Domain = (UINT16*) malloc((identity->DomainLength + 1) * sizeof(WCHAR));
		CopyMemory(identity->Domain, srcIdentity->Domain, identity->DomainLength * sizeof(WCHAR));
		identity->Domain[identity->DomainLength] = 0;
	}

	identity->PasswordLength = srcIdentity->PasswordLength;

	if (identity->PasswordLength > 0)
	{
		identity->Password = (UINT16*) malloc((identity->PasswordLength + 1) * sizeof(WCHAR));
		CopyMemory(identity->Password, srcIdentity->Password, identity->PasswordLength * sizeof(WCHAR));
		identity->Password[identity->PasswordLength] = 0;
	}
}

PSecBuffer sspi_FindSecBuffer(PSecBufferDesc pMessage, ULONG BufferType)
{
	int index;
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

static BOOL sspi_initialized = FALSE;

void sspi_GlobalInit()
{
	if (!sspi_initialized)
	{
		SSL_load_error_strings();
		SSL_library_init();

		sspi_ContextBufferAllocTableNew();
		sspi_initialized = TRUE;
	}
}

void sspi_GlobalFinish()
{
	if (sspi_initialized)
	{
		sspi_ContextBufferAllocTableFree();
	}

	sspi_initialized = FALSE;
}

#ifndef WITH_NATIVE_SSPI

SecurityFunctionTableA* sspi_GetSecurityFunctionTableAByNameA(const SEC_CHAR* Name)
{
	int index;
	UINT32 cPackages;

	cPackages = sizeof(SecPkgInfoA_LIST) / sizeof(*(SecPkgInfoA_LIST));

	for (index = 0; index < (int) cPackages; index++)
	{
		if (strcmp(Name, SecurityFunctionTableA_NAME_LIST[index].Name) == 0)
		{
			return (SecurityFunctionTableA*) SecurityFunctionTableA_NAME_LIST[index].SecurityFunctionTable;
		}
	}

	return NULL;
}

SecurityFunctionTableA* sspi_GetSecurityFunctionTableAByNameW(const SEC_WCHAR* Name)
{
	return NULL;
}

SecurityFunctionTableW* sspi_GetSecurityFunctionTableWByNameW(const SEC_WCHAR* Name)
{
	int index;
	UINT32 cPackages;

	cPackages = sizeof(SecPkgInfoW_LIST) / sizeof(*(SecPkgInfoW_LIST));

	for (index = 0; index < (int) cPackages; index++)
	{
		if (lstrcmpW(Name, SecurityFunctionTableW_NAME_LIST[index].Name) == 0)
		{
			return (SecurityFunctionTableW*) SecurityFunctionTableW_NAME_LIST[index].SecurityFunctionTable;
		}
	}

	return NULL;
}

SecurityFunctionTableW* sspi_GetSecurityFunctionTableWByNameA(const SEC_CHAR* Name)
{
	SEC_WCHAR* NameW = NULL;
	SecurityFunctionTableW* table;

	ConvertToUnicode(CP_UTF8, 0, Name, -1, &NameW, 0);

	table = sspi_GetSecurityFunctionTableWByNameW(NameW);
	free(NameW);

	return table;
}

void FreeContextBuffer_EnumerateSecurityPackages(void* contextBuffer);
void FreeContextBuffer_QuerySecurityPackageInfo(void* contextBuffer);

void sspi_ContextBufferFree(void* contextBuffer)
{
	int index;
	UINT32 allocatorIndex;

	for (index = 0; index < (int) ContextBufferAllocTable.cMaxEntries; index++)
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

/* Package Management */

SECURITY_STATUS SEC_ENTRY EnumerateSecurityPackagesW(ULONG* pcPackages, PSecPkgInfoW* ppPackageInfo)
{
	int index;
	size_t size;
	UINT32 cPackages;
	SecPkgInfoW* pPackageInfo;

	cPackages = sizeof(SecPkgInfoW_LIST) / sizeof(*(SecPkgInfoW_LIST));
	size = sizeof(SecPkgInfoW) * cPackages;

	pPackageInfo = (SecPkgInfoW*) sspi_ContextBufferAlloc(EnumerateSecurityPackagesIndex, size);

	for (index = 0; index < (int) cPackages; index++)
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

SECURITY_STATUS SEC_ENTRY EnumerateSecurityPackagesA(ULONG* pcPackages, PSecPkgInfoA* ppPackageInfo)
{
	int index;
	size_t size;
	UINT32 cPackages;
	SecPkgInfoA* pPackageInfo;

	cPackages = sizeof(SecPkgInfoA_LIST) / sizeof(*(SecPkgInfoA_LIST));
	size = sizeof(SecPkgInfoA) * cPackages;

	pPackageInfo = (SecPkgInfoA*) sspi_ContextBufferAlloc(EnumerateSecurityPackagesIndex, size);

	for (index = 0; index < (int) cPackages; index++)
	{
		pPackageInfo[index].fCapabilities = SecPkgInfoA_LIST[index]->fCapabilities;
		pPackageInfo[index].wVersion = SecPkgInfoA_LIST[index]->wVersion;
		pPackageInfo[index].wRPCID = SecPkgInfoA_LIST[index]->wRPCID;
		pPackageInfo[index].cbMaxToken = SecPkgInfoA_LIST[index]->cbMaxToken;
		pPackageInfo[index].Name = _strdup(SecPkgInfoA_LIST[index]->Name);
		pPackageInfo[index].Comment = _strdup(SecPkgInfoA_LIST[index]->Comment);
	}

	*(pcPackages) = cPackages;
	*(ppPackageInfo) = pPackageInfo;

	return SEC_E_OK;
}

void FreeContextBuffer_EnumerateSecurityPackages(void* contextBuffer)
{
	int index;
	UINT32 cPackages;
	SecPkgInfoA* pPackageInfo = (SecPkgInfoA*) contextBuffer;

	cPackages = sizeof(SecPkgInfoA_LIST) / sizeof(*(SecPkgInfoA_LIST));

	for (index = 0; index < (int) cPackages; index++)
	{
		if (pPackageInfo[index].Name)
			free(pPackageInfo[index].Name);

		if (pPackageInfo[index].Comment)
			free(pPackageInfo[index].Comment);
	}

	free(pPackageInfo);
}

SecurityFunctionTableW* SEC_ENTRY InitSecurityInterfaceW(void)
{
	return &SSPI_SecurityFunctionTableW;
}

SecurityFunctionTableA* SEC_ENTRY InitSecurityInterfaceA(void)
{
	return &SSPI_SecurityFunctionTableA;
}

SECURITY_STATUS SEC_ENTRY QuerySecurityPackageInfoW(SEC_WCHAR* pszPackageName, PSecPkgInfoW* ppPackageInfo)
{
	int index;
	size_t size;
	UINT32 cPackages;
	SecPkgInfoW* pPackageInfo;

	cPackages = sizeof(SecPkgInfoW_LIST) / sizeof(*(SecPkgInfoW_LIST));

	for (index = 0; index < (int) cPackages; index++)
	{
		if (lstrcmpW(pszPackageName, SecPkgInfoW_LIST[index]->Name) == 0)
		{
			size = sizeof(SecPkgInfoW);
			pPackageInfo = (SecPkgInfoW*) sspi_ContextBufferAlloc(QuerySecurityPackageInfoIndex, size);

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

SECURITY_STATUS SEC_ENTRY QuerySecurityPackageInfoA(SEC_CHAR* pszPackageName, PSecPkgInfoA* ppPackageInfo)
{
	int index;
	size_t size;
	UINT32 cPackages;
	SecPkgInfoA* pPackageInfo;

	cPackages = sizeof(SecPkgInfoA_LIST) / sizeof(*(SecPkgInfoA_LIST));

	for (index = 0; index < (int) cPackages; index++)
	{
		if (strcmp(pszPackageName, SecPkgInfoA_LIST[index]->Name) == 0)
		{
			size = sizeof(SecPkgInfoA);
			pPackageInfo = (SecPkgInfoA*) sspi_ContextBufferAlloc(QuerySecurityPackageInfoIndex, size);

			pPackageInfo->fCapabilities = SecPkgInfoA_LIST[index]->fCapabilities;
			pPackageInfo->wVersion = SecPkgInfoA_LIST[index]->wVersion;
			pPackageInfo->wRPCID = SecPkgInfoA_LIST[index]->wRPCID;
			pPackageInfo->cbMaxToken = SecPkgInfoA_LIST[index]->cbMaxToken;
			pPackageInfo->Name = _strdup(SecPkgInfoA_LIST[index]->Name);
			pPackageInfo->Comment = _strdup(SecPkgInfoA_LIST[index]->Comment);

			*(ppPackageInfo) = pPackageInfo;

			return SEC_E_OK;
		}
	}

	*(ppPackageInfo) = NULL;

	return SEC_E_SECPKG_NOT_FOUND;
}

void FreeContextBuffer_QuerySecurityPackageInfo(void* contextBuffer)
{
	SecPkgInfo* pPackageInfo = (SecPkgInfo*) contextBuffer;

	if (pPackageInfo->Name)
		free(pPackageInfo->Name);

	if (pPackageInfo->Comment)
		free(pPackageInfo->Comment);

	free(pPackageInfo);
}

/* Credential Management */

SECURITY_STATUS SEC_ENTRY AcquireCredentialsHandleW(SEC_WCHAR* pszPrincipal, SEC_WCHAR* pszPackage,
		ULONG fCredentialUse, void* pvLogonID, void* pAuthData, SEC_GET_KEY_FN pGetKeyFn,
		void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	SecurityFunctionTableW* table = sspi_GetSecurityFunctionTableWByNameW(pszPackage);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->AcquireCredentialsHandleW == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->AcquireCredentialsHandleW(pszPrincipal, pszPackage, fCredentialUse,
			pvLogonID, pAuthData, pGetKeyFn, pvGetKeyArgument, phCredential, ptsExpiry);

	return status;
}

SECURITY_STATUS SEC_ENTRY AcquireCredentialsHandleA(SEC_CHAR* pszPrincipal, SEC_CHAR* pszPackage,
		ULONG fCredentialUse, void* pvLogonID, void* pAuthData, SEC_GET_KEY_FN pGetKeyFn,
		void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	SecurityFunctionTableA* table = sspi_GetSecurityFunctionTableAByNameA(pszPackage);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->AcquireCredentialsHandleA == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->AcquireCredentialsHandleA(pszPrincipal, pszPackage, fCredentialUse,
			pvLogonID, pAuthData, pGetKeyFn, pvGetKeyArgument, phCredential, ptsExpiry);

	return status;
}

SECURITY_STATUS SEC_ENTRY ExportSecurityContext(PCtxtHandle phContext, ULONG fFlags, PSecBuffer pPackedContext, HANDLE* pToken)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY FreeCredentialsHandle(PCredHandle phCredential)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->FreeCredentialsHandle == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->FreeCredentialsHandle(phCredential);

	return status;
}

SECURITY_STATUS SEC_ENTRY ImportSecurityContextW(SEC_WCHAR* pszPackage, PSecBuffer pPackedContext, HANDLE pToken, PCtxtHandle phContext)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY ImportSecurityContextA(SEC_CHAR* pszPackage, PSecBuffer pPackedContext, HANDLE pToken, PCtxtHandle phContext)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY QueryCredentialsAttributesW(PCredHandle phCredential, ULONG ulAttribute, void* pBuffer)
{
	SEC_WCHAR* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableW* table;

	Name = (SEC_WCHAR*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableWByNameW(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->QueryCredentialsAttributesW == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->QueryCredentialsAttributesW(phCredential, ulAttribute, pBuffer);

	return status;
}

SECURITY_STATUS SEC_ENTRY QueryCredentialsAttributesA(PCredHandle phCredential, ULONG ulAttribute, void* pBuffer)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->QueryCredentialsAttributesA == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->QueryCredentialsAttributesA(phCredential, ulAttribute, pBuffer);

	return status;
}

/* Context Management */

SECURITY_STATUS SEC_ENTRY AcceptSecurityContext(PCredHandle phCredential, PCtxtHandle phContext,
		PSecBufferDesc pInput, ULONG fContextReq, ULONG TargetDataRep, PCtxtHandle phNewContext,
		PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsTimeStamp)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->AcceptSecurityContext == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->AcceptSecurityContext(phCredential, phContext, pInput, fContextReq,
			TargetDataRep, phNewContext, pOutput, pfContextAttr, ptsTimeStamp);

	return status;
}

SECURITY_STATUS SEC_ENTRY ApplyControlToken(PCtxtHandle phContext, PSecBufferDesc pInput)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY CompleteAuthToken(PCtxtHandle phContext, PSecBufferDesc pToken)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY DeleteSecurityContext(PCtxtHandle phContext)
{
	char* Name = NULL;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->DeleteSecurityContext == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->DeleteSecurityContext(phContext);

	return status;
}

SECURITY_STATUS SEC_ENTRY FreeContextBuffer(void* pvContextBuffer)
{
	if (!pvContextBuffer)
		return SEC_E_INVALID_HANDLE;

	sspi_ContextBufferFree(pvContextBuffer);

	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY ImpersonateSecurityContext(PCtxtHandle phContext)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY InitializeSecurityContextW(PCredHandle phCredential, PCtxtHandle phContext,
		SEC_WCHAR* pszTargetName, ULONG fContextReq, ULONG Reserved1, ULONG TargetDataRep,
		PSecBufferDesc pInput, ULONG Reserved2, PCtxtHandle phNewContext,
		PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableW* table;

	Name = (SEC_CHAR*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableWByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->InitializeSecurityContextW == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->InitializeSecurityContextW(phCredential, phContext,
			pszTargetName, fContextReq, Reserved1, TargetDataRep,
			pInput, Reserved2, phNewContext, pOutput, pfContextAttr, ptsExpiry);

	return status;
}

SECURITY_STATUS SEC_ENTRY InitializeSecurityContextA(PCredHandle phCredential, PCtxtHandle phContext,
		SEC_CHAR* pszTargetName, ULONG fContextReq, ULONG Reserved1, ULONG TargetDataRep,
		PSecBufferDesc pInput, ULONG Reserved2, PCtxtHandle phNewContext,
		PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (SEC_CHAR*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->InitializeSecurityContextA == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->InitializeSecurityContextA(phCredential, phContext,
			pszTargetName, fContextReq, Reserved1, TargetDataRep,
			pInput, Reserved2, phNewContext, pOutput, pfContextAttr, ptsExpiry);

	return status;
}

SECURITY_STATUS SEC_ENTRY QueryContextAttributesW(PCtxtHandle phContext, ULONG ulAttribute, void* pBuffer)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableW* table;

	Name = (SEC_CHAR*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableWByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->QueryContextAttributesW == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->QueryContextAttributesW(phContext, ulAttribute, pBuffer);

	return status;
}

SECURITY_STATUS SEC_ENTRY QueryContextAttributesA(PCtxtHandle phContext, ULONG ulAttribute, void* pBuffer)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (SEC_CHAR*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->QueryContextAttributesA == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->QueryContextAttributesA(phContext, ulAttribute, pBuffer);

	return status;
}

SECURITY_STATUS SEC_ENTRY QuerySecurityContextToken(PCtxtHandle phContext, HANDLE* phToken)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY SetContextAttributes(PCtxtHandle phContext, ULONG ulAttribute, void* pBuffer, ULONG cbBuffer)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY RevertSecurityContext(PCtxtHandle phContext)
{
	return SEC_E_OK;
}

/* Message Support */

SECURITY_STATUS SEC_ENTRY DecryptMessage(PCtxtHandle phContext, PSecBufferDesc pMessage, ULONG MessageSeqNo, PULONG pfQOP)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->DecryptMessage == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->DecryptMessage(phContext, pMessage, MessageSeqNo, pfQOP);

	return status;
}

SECURITY_STATUS SEC_ENTRY EncryptMessage(PCtxtHandle phContext, ULONG fQOP, PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->EncryptMessage == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->EncryptMessage(phContext, fQOP, pMessage, MessageSeqNo);

	return status;
}

SECURITY_STATUS SEC_ENTRY MakeSignature(PCtxtHandle phContext, ULONG fQOP, PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->MakeSignature == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->MakeSignature(phContext, fQOP, pMessage, MessageSeqNo);

	return status;
}

SECURITY_STATUS SEC_ENTRY VerifySignature(PCtxtHandle phContext, PSecBufferDesc pMessage, ULONG MessageSeqNo, PULONG pfQOP)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->VerifySignature == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->VerifySignature(phContext, pMessage, MessageSeqNo, pfQOP);

	return status;
}

SecurityFunctionTableA SSPI_SecurityFunctionTableA =
{
	1, /* dwVersion */
	EnumerateSecurityPackagesA, /* EnumerateSecurityPackages */
	QueryCredentialsAttributesA, /* QueryCredentialsAttributes */
	AcquireCredentialsHandleA, /* AcquireCredentialsHandle */
	FreeCredentialsHandle, /* FreeCredentialsHandle */
	NULL, /* Reserved2 */
	InitializeSecurityContextA, /* InitializeSecurityContext */
	AcceptSecurityContext, /* AcceptSecurityContext */
	CompleteAuthToken, /* CompleteAuthToken */
	DeleteSecurityContext, /* DeleteSecurityContext */
	ApplyControlToken, /* ApplyControlToken */
	QueryContextAttributesA, /* QueryContextAttributes */
	ImpersonateSecurityContext, /* ImpersonateSecurityContext */
	RevertSecurityContext, /* RevertSecurityContext */
	MakeSignature, /* MakeSignature */
	VerifySignature, /* VerifySignature */
	FreeContextBuffer, /* FreeContextBuffer */
	QuerySecurityPackageInfoA, /* QuerySecurityPackageInfo */
	NULL, /* Reserved3 */
	NULL, /* Reserved4 */
	ExportSecurityContext, /* ExportSecurityContext */
	ImportSecurityContextA, /* ImportSecurityContext */
	NULL, /* AddCredentials */
	NULL, /* Reserved8 */
	QuerySecurityContextToken, /* QuerySecurityContextToken */
	EncryptMessage, /* EncryptMessage */
	DecryptMessage, /* DecryptMessage */
	SetContextAttributes, /* SetContextAttributes */
};

SecurityFunctionTableW SSPI_SecurityFunctionTableW =
{
	1, /* dwVersion */
	EnumerateSecurityPackagesW, /* EnumerateSecurityPackages */
	QueryCredentialsAttributesW, /* QueryCredentialsAttributes */
	AcquireCredentialsHandleW, /* AcquireCredentialsHandle */
	FreeCredentialsHandle, /* FreeCredentialsHandle */
	NULL, /* Reserved2 */
	InitializeSecurityContextW, /* InitializeSecurityContext */
	AcceptSecurityContext, /* AcceptSecurityContext */
	CompleteAuthToken, /* CompleteAuthToken */
	DeleteSecurityContext, /* DeleteSecurityContext */
	ApplyControlToken, /* ApplyControlToken */
	QueryContextAttributesW, /* QueryContextAttributes */
	ImpersonateSecurityContext, /* ImpersonateSecurityContext */
	RevertSecurityContext, /* RevertSecurityContext */
	MakeSignature, /* MakeSignature */
	VerifySignature, /* VerifySignature */
	FreeContextBuffer, /* FreeContextBuffer */
	QuerySecurityPackageInfoW, /* QuerySecurityPackageInfo */
	NULL, /* Reserved3 */
	NULL, /* Reserved4 */
	ExportSecurityContext, /* ExportSecurityContext */
	ImportSecurityContextW, /* ImportSecurityContext */
	NULL, /* AddCredentials */
	NULL, /* Reserved8 */
	QuerySecurityContextToken, /* QuerySecurityContextToken */
	EncryptMessage, /* EncryptMessage */
	DecryptMessage, /* DecryptMessage */
	SetContextAttributes, /* SetContextAttributes */
};

#endif

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

#include <freerdp/utils/memory.h>

#include <winpr/sspi.h>

#include "sspi.h"

/* Authentication Functions: http://msdn.microsoft.com/en-us/library/windows/desktop/aa374731/ */

#ifdef FREERDP_SSPI

extern const SecPkgInfoA NTLM_SecPkgInfoA;
extern const SecPkgInfoW NTLM_SecPkgInfoW;
extern const SecPkgInfoA CREDSSP_SecPkgInfoA;
extern const SecPkgInfoW CREDSSP_SecPkgInfoW;

extern const SecurityFunctionTableA NTLM_SecurityFunctionTableA;
extern const SecurityFunctionTableW NTLM_SecurityFunctionTableW;
extern const SecurityFunctionTableA CREDSSP_SecurityFunctionTableA;
extern const SecurityFunctionTableW CREDSSP_SecurityFunctionTableW;

const SecPkgInfoA* SecPkgInfoA_LIST[] =
{
	&NTLM_SecPkgInfoA,
	&CREDSSP_SecPkgInfoA
};

const SecPkgInfoW* SecPkgInfoW_LIST[] =
{
	&NTLM_SecPkgInfoW,
	&CREDSSP_SecPkgInfoW
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
	{ "CREDSSP", &CREDSSP_SecurityFunctionTableA }
};

const SecurityFunctionTableW_NAME SecurityFunctionTableW_NAME_LIST[] =
{
	{ L"NTLM", &NTLM_SecurityFunctionTableW },
	{ L"CREDSSP", &CREDSSP_SecurityFunctionTableW }
};

#endif

#define SecHandle_LOWER_MAX	0xFFFFFFFF
#define SecHandle_UPPER_MAX	0xFFFFFFFE

struct _CONTEXT_BUFFER_ALLOC_ENTRY
{
	void* contextBuffer;
	uint32 allocatorIndex;
};
typedef struct _CONTEXT_BUFFER_ALLOC_ENTRY CONTEXT_BUFFER_ALLOC_ENTRY;

struct _CONTEXT_BUFFER_ALLOC_TABLE
{
	uint32 cEntries;
	uint32 cMaxEntries;
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

	ContextBufferAllocTable.entries = xzalloc(size);
}

void sspi_ContextBufferAllocTableGrow()
{
	size_t size;
	ContextBufferAllocTable.cEntries = 0;
	ContextBufferAllocTable.cMaxEntries *= 2;

	size = sizeof(CONTEXT_BUFFER_ALLOC_ENTRY) * ContextBufferAllocTable.cMaxEntries;

	ContextBufferAllocTable.entries = xrealloc(ContextBufferAllocTable.entries, size);
	memset((void*) &ContextBufferAllocTable.entries[ContextBufferAllocTable.cMaxEntries / 2], 0, size / 2);
}

void sspi_ContextBufferAllocTableFree()
{
	ContextBufferAllocTable.cEntries = ContextBufferAllocTable.cMaxEntries = 0;
	xfree(ContextBufferAllocTable.entries);
}

void* sspi_ContextBufferAlloc(uint32 allocatorIndex, size_t size)
{
	int index;
	void* contextBuffer;

	for (index = 0; index < (int) ContextBufferAllocTable.cMaxEntries; index++)
	{
		if (ContextBufferAllocTable.entries[index].contextBuffer == NULL)
		{
			contextBuffer = xzalloc(size);
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

	credentials = xnew(CREDENTIALS);

	if (credentials != NULL)
	{

	}

	return credentials;
}

void sspi_CredentialsFree(CREDENTIALS* credentials)
{
	if (!credentials)
		return;

	xfree(credentials);
}

void sspi_SecBufferAlloc(PSecBuffer SecBuffer, size_t size)
{
	SecBuffer->cbBuffer = size;
	SecBuffer->pvBuffer = xzalloc(size);
}

void sspi_SecBufferFree(PSecBuffer SecBuffer)
{
	SecBuffer->cbBuffer = 0;
	xfree(SecBuffer->pvBuffer);
	SecBuffer->pvBuffer = NULL;
}

SecHandle* sspi_SecureHandleAlloc()
{
	SecHandle* handle = xmalloc(sizeof(SecHandle));
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

	if (!handle)
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

	if (!handle)
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

	xfree(handle);
}

void sspi_GlobalInit()
{
	sspi_ContextBufferAllocTableNew();
}

void sspi_GlobalFinish()
{
	sspi_ContextBufferAllocTableFree();
}

#ifndef NATIVE_SSPI

SecurityFunctionTableA* sspi_GetSecurityFunctionTableByNameA(const SEC_CHAR* Name)
{
	int index;
	uint32 cPackages;

	cPackages = ARRAY_SIZE(SecPkgInfoA_LIST);

	for (index = 0; index < (int) cPackages; index++)
	{
		if (strcmp(Name, SecurityFunctionTableA_NAME_LIST[index].Name) == 0)
		{
			return (SecurityFunctionTableA*) SecurityFunctionTableA_NAME_LIST[index].SecurityFunctionTable;
		}
	}

	return NULL;
}

SecurityFunctionTableW* sspi_GetSecurityFunctionTableByNameW(const SEC_WCHAR* Name)
{
	int index;
	uint32 cPackages;

	cPackages = ARRAY_SIZE(SecPkgInfoW_LIST);

	for (index = 0; index < (int) cPackages; index++)
	{
		if (wcscmp(Name, SecurityFunctionTableW_NAME_LIST[index].Name) == 0)
		{
			return (SecurityFunctionTableW*) SecurityFunctionTableW_NAME_LIST[index].SecurityFunctionTable;
		}
	}

	return NULL;
}

void FreeContextBuffer_EnumerateSecurityPackages(void* contextBuffer);
void FreeContextBuffer_QuerySecurityPackageInfo(void* contextBuffer);

void sspi_ContextBufferFree(void* contextBuffer)
{
	int index;
	uint32 allocatorIndex;

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

SECURITY_STATUS SEC_ENTRY EnumerateSecurityPackagesW(uint32* pcPackages, PSecPkgInfoW* ppPackageInfo)
{
	int index;
	size_t size;
	uint32 cPackages;
	SecPkgInfoW* pPackageInfo;

	cPackages = ARRAY_SIZE(SecPkgInfoW_LIST);
	size = sizeof(SecPkgInfoW) * cPackages;

	pPackageInfo = (SecPkgInfoW*) sspi_ContextBufferAlloc(EnumerateSecurityPackagesIndex, size);

	for (index = 0; index < (int) cPackages; index++)
	{
		pPackageInfo[index].fCapabilities = SecPkgInfoW_LIST[index]->fCapabilities;
		pPackageInfo[index].wVersion = SecPkgInfoW_LIST[index]->wVersion;
		pPackageInfo[index].wRPCID = SecPkgInfoW_LIST[index]->wRPCID;
		pPackageInfo[index].cbMaxToken = SecPkgInfoW_LIST[index]->cbMaxToken;
		pPackageInfo[index].Name = xwcsdup(SecPkgInfoW_LIST[index]->Name);
		pPackageInfo[index].Comment = xwcsdup(SecPkgInfoW_LIST[index]->Comment);
	}

	*(pcPackages) = cPackages;
	*(ppPackageInfo) = pPackageInfo;

	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY EnumerateSecurityPackagesA(uint32* pcPackages, PSecPkgInfoA* ppPackageInfo)
{
	int index;
	size_t size;
	uint32 cPackages;
	SecPkgInfoA* pPackageInfo;

	cPackages = ARRAY_SIZE(SecPkgInfoA_LIST);
	size = sizeof(SecPkgInfoA) * cPackages;

	pPackageInfo = (SecPkgInfoA*) sspi_ContextBufferAlloc(EnumerateSecurityPackagesIndex, size);

	for (index = 0; index < (int) cPackages; index++)
	{
		pPackageInfo[index].fCapabilities = SecPkgInfoA_LIST[index]->fCapabilities;
		pPackageInfo[index].wVersion = SecPkgInfoA_LIST[index]->wVersion;
		pPackageInfo[index].wRPCID = SecPkgInfoA_LIST[index]->wRPCID;
		pPackageInfo[index].cbMaxToken = SecPkgInfoA_LIST[index]->cbMaxToken;
		pPackageInfo[index].Name = xstrdup(SecPkgInfoA_LIST[index]->Name);
		pPackageInfo[index].Comment = xstrdup(SecPkgInfoA_LIST[index]->Comment);
	}

	*(pcPackages) = cPackages;
	*(ppPackageInfo) = pPackageInfo;

	return SEC_E_OK;
}

void FreeContextBuffer_EnumerateSecurityPackages(void* contextBuffer)
{
	int index;
	uint32 cPackages;
	SecPkgInfoA* pPackageInfo = (SecPkgInfoA*) contextBuffer;

	cPackages = ARRAY_SIZE(SecPkgInfoA_LIST);

	for (index = 0; index < (int) cPackages; index++)
	{
		if (pPackageInfo[index].Name)
			xfree(pPackageInfo[index].Name);

		if (pPackageInfo[index].Comment)
			xfree(pPackageInfo[index].Comment);
	}

	xfree(pPackageInfo);
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
	uint32 cPackages;
	SecPkgInfoW* pPackageInfo;

	cPackages = ARRAY_SIZE(SecPkgInfoW_LIST);

	for (index = 0; index < (int) cPackages; index++)
	{
		if (wcscmp(pszPackageName, SecPkgInfoW_LIST[index]->Name) == 0)
		{
			size = sizeof(SecPkgInfoW);
			pPackageInfo = (SecPkgInfoW*) sspi_ContextBufferAlloc(QuerySecurityPackageInfoIndex, size);

			pPackageInfo->fCapabilities = SecPkgInfoW_LIST[index]->fCapabilities;
			pPackageInfo->wVersion = SecPkgInfoW_LIST[index]->wVersion;
			pPackageInfo->wRPCID = SecPkgInfoW_LIST[index]->wRPCID;
			pPackageInfo->cbMaxToken = SecPkgInfoW_LIST[index]->cbMaxToken;
			pPackageInfo->Name = xwcsdup(SecPkgInfoW_LIST[index]->Name);
			pPackageInfo->Comment = xwcsdup(SecPkgInfoW_LIST[index]->Comment);

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
	uint32 cPackages;
	SecPkgInfoA* pPackageInfo;

	cPackages = ARRAY_SIZE(SecPkgInfoA_LIST);

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
			pPackageInfo->Name = xstrdup(SecPkgInfoA_LIST[index]->Name);
			pPackageInfo->Comment = xstrdup(SecPkgInfoA_LIST[index]->Comment);

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
		xfree(pPackageInfo->Name);

	if (pPackageInfo->Comment)
		xfree(pPackageInfo->Comment);

	xfree(pPackageInfo);
}

/* Credential Management */

SECURITY_STATUS SEC_ENTRY AcquireCredentialsHandleW(SEC_WCHAR* pszPrincipal, SEC_WCHAR* pszPackage,
		uint32 fCredentialUse, void* pvLogonID, void* pAuthData, void* pGetKeyFn,
		void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	SecurityFunctionTableW* table = sspi_GetSecurityFunctionTableByNameW(pszPackage);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->AcquireCredentialsHandleW == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->AcquireCredentialsHandleW(pszPrincipal, pszPackage, fCredentialUse,
			pvLogonID, pAuthData, pGetKeyFn, pvGetKeyArgument, phCredential, ptsExpiry);

	return status;
}

SECURITY_STATUS SEC_ENTRY AcquireCredentialsHandleA(SEC_CHAR* pszPrincipal, SEC_CHAR* pszPackage,
		uint32 fCredentialUse, void* pvLogonID, void* pAuthData, void* pGetKeyFn,
		void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	SecurityFunctionTableA* table = sspi_GetSecurityFunctionTableByNameA(pszPackage);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->AcquireCredentialsHandleA == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->AcquireCredentialsHandleA(pszPrincipal, pszPackage, fCredentialUse,
			pvLogonID, pAuthData, pGetKeyFn, pvGetKeyArgument, phCredential, ptsExpiry);

	return status;
}

SECURITY_STATUS SEC_ENTRY ExportSecurityContext(PCtxtHandle phContext, uint32 fFlags, PSecBuffer pPackedContext, void* pToken)
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

	table = sspi_GetSecurityFunctionTableByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->FreeCredentialsHandle == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->FreeCredentialsHandle(phCredential);

	return status;
}

SECURITY_STATUS SEC_ENTRY ImportSecurityContextW(SEC_WCHAR* pszPackage, PSecBuffer pPackedContext, void* pToken, PCtxtHandle phContext)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY ImportSecurityContextA(SEC_CHAR* pszPackage, PSecBuffer pPackedContext, void* pToken, PCtxtHandle phContext)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY QueryCredentialsAttributesW(PCredHandle phCredential, uint32 ulAttribute, void* pBuffer)
{
	SEC_WCHAR* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableW* table;

	Name = (SEC_WCHAR*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableByNameW(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->QueryCredentialsAttributesW == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->QueryCredentialsAttributesW(phCredential, ulAttribute, pBuffer);

	return status;
}

SECURITY_STATUS SEC_ENTRY QueryCredentialsAttributesA(PCredHandle phCredential, uint32 ulAttribute, void* pBuffer)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->QueryCredentialsAttributesA == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->QueryCredentialsAttributesA(phCredential, ulAttribute, pBuffer);

	return status;
}

/* Context Management */

SECURITY_STATUS SEC_ENTRY AcceptSecurityContext(PCredHandle phCredential, PCtxtHandle phContext,
		PSecBufferDesc pInput, uint32 fContextReq, uint32 TargetDataRep, PCtxtHandle phNewContext,
		PSecBufferDesc pOutput, uint32* pfContextAttr, PTimeStamp ptsTimeStamp)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableByNameA(Name);

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
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableByNameA(Name);

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
		SEC_WCHAR* pszTargetName, uint32 fContextReq, uint32 Reserved1, uint32 TargetDataRep,
		PSecBufferDesc pInput, uint32 Reserved2, PCtxtHandle phNewContext,
		PSecBufferDesc pOutput, uint32* pfContextAttr, PTimeStamp ptsExpiry)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableW* table;

	Name = (SEC_CHAR*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = (SecurityFunctionTableW*) sspi_GetSecurityFunctionTableByNameA(Name);

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
		SEC_CHAR* pszTargetName, uint32 fContextReq, uint32 Reserved1, uint32 TargetDataRep,
		PSecBufferDesc pInput, uint32 Reserved2, PCtxtHandle phNewContext,
		PSecBufferDesc pOutput, uint32* pfContextAttr, PTimeStamp ptsExpiry)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (SEC_CHAR*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->InitializeSecurityContextA == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->InitializeSecurityContextA(phCredential, phContext,
			pszTargetName, fContextReq, Reserved1, TargetDataRep,
			pInput, Reserved2, phNewContext, pOutput, pfContextAttr, ptsExpiry);

	return status;
}

SECURITY_STATUS SEC_ENTRY QueryContextAttributesW(PCtxtHandle phContext, uint32 ulAttribute, void* pBuffer)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableW* table;

	Name = (SEC_CHAR*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = (SecurityFunctionTableW*) sspi_GetSecurityFunctionTableByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->QueryContextAttributesW == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->QueryContextAttributesW(phContext, ulAttribute, pBuffer);

	return status;
}

SECURITY_STATUS SEC_ENTRY QueryContextAttributesA(PCtxtHandle phContext, uint32 ulAttribute, void* pBuffer)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (SEC_CHAR*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->QueryContextAttributesA == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->QueryContextAttributesA(phContext, ulAttribute, pBuffer);

	return status;
}

SECURITY_STATUS SEC_ENTRY QuerySecurityContextToken(PCtxtHandle phContext, void* phToken)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY SetContextAttributes(PCtxtHandle phContext, uint32 ulAttribute, void* pBuffer, uint32 cbBuffer)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY RevertSecurityContext(PCtxtHandle phContext)
{
	return SEC_E_OK;
}

/* Message Support */

SECURITY_STATUS SEC_ENTRY DecryptMessage(PCtxtHandle phContext, PSecBufferDesc pMessage, uint32 MessageSeqNo, uint32* pfQOP)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->DecryptMessage == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->DecryptMessage(phContext, pMessage, MessageSeqNo, pfQOP);

	return status;
}

SECURITY_STATUS SEC_ENTRY EncryptMessage(PCtxtHandle phContext, uint32 fQOP, PSecBufferDesc pMessage, uint32 MessageSeqNo)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->EncryptMessage == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->EncryptMessage(phContext, fQOP, pMessage, MessageSeqNo);

	return status;
}

SECURITY_STATUS SEC_ENTRY MakeSignature(PCtxtHandle phContext, uint32 fQOP, PSecBufferDesc pMessage, uint32 MessageSeqNo)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->MakeSignature == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->MakeSignature(phContext, fQOP, pMessage, MessageSeqNo);

	return status;
}

SECURITY_STATUS SEC_ENTRY VerifySignature(PCtxtHandle phContext, PSecBufferDesc pMessage, uint32 MessageSeqNo, uint32* pfQOP)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableByNameA(Name);

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
	QueryContextAttributes, /* QueryContextAttributes */
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
	QueryContextAttributes, /* QueryContextAttributes */
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

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

#include <freerdp/sspi/sspi.h>

#include "sspi.h"

/* Authentication Functions: http://msdn.microsoft.com/en-us/library/windows/desktop/aa374731/ */

extern const SecPkgInfo NTLM_SecPkgInfo;
extern const SecPkgInfo CREDSSP_SecPkgInfo;

const SecurityFunctionTable SSPI_SecurityFunctionTable;
extern const SecurityFunctionTable NTLM_SecurityFunctionTable;
extern const SecurityFunctionTable CREDSSP_SecurityFunctionTable;

const SecPkgInfo* SecPkgInfo_LIST[] =
{
	&NTLM_SecPkgInfo,
	&CREDSSP_SecPkgInfo
};

struct _SecurityFunctionTable_NAME
{
	char* Name;
	const SecurityFunctionTable* SecurityFunctionTable;
};
typedef struct _SecurityFunctionTable_NAME SecurityFunctionTable_NAME;

const SecurityFunctionTable_NAME SecurityFunctionTable_NAME_LIST[] =
{
	{ "NTLM", &NTLM_SecurityFunctionTable },
	{ "CREDSSP", &CREDSSP_SecurityFunctionTable }
};

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

void sspi_SecBufferAlloc(SecBuffer* SecBuffer, size_t size)
{
	SecBuffer->cbBuffer = size;
	SecBuffer->pvBuffer = xzalloc(size);
}

void sspi_SecBufferFree(SecBuffer* SecBuffer)
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

	handle->dwLower = (uint32*) (~((size_t) pointer));
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

	handle->dwUpper = (uint32*) (~((size_t) pointer));
}

void sspi_SecureHandleFree(SecHandle* handle)
{
	if (!handle)
		return;

	xfree(handle);
}

SecurityFunctionTable* sspi_GetSecurityFunctionTableByName(const char* Name)
{
	int index;
	uint32 cPackages;

	cPackages = ARRAY_SIZE(SecPkgInfo_LIST);

	for (index = 0; index < (int) cPackages; index++)
	{
		if (strcmp(Name, SecurityFunctionTable_NAME_LIST[index].Name) == 0)
		{
			return (SecurityFunctionTable*) SecurityFunctionTable_NAME_LIST[index].SecurityFunctionTable;
		}
	}

	return NULL;
}

void sspi_GlobalInit()
{
	sspi_ContextBufferAllocTableNew();
}

void sspi_GlobalFinish()
{
	sspi_ContextBufferAllocTableFree();
}

/* Package Management */

SECURITY_STATUS EnumerateSecurityPackages(uint32* pcPackages, SecPkgInfo** ppPackageInfo)
{
	int index;
	size_t size;
	uint32 cPackages;
	SecPkgInfo* pPackageInfo;

	cPackages = ARRAY_SIZE(SecPkgInfo_LIST);
	size = sizeof(SecPkgInfo) * cPackages;

	pPackageInfo = (SecPkgInfo*) sspi_ContextBufferAlloc(EnumerateSecurityPackagesIndex, size);

	for (index = 0; index < (int) cPackages; index++)
	{
		pPackageInfo[index].fCapabilities = SecPkgInfo_LIST[index]->fCapabilities;
		pPackageInfo[index].wVersion = SecPkgInfo_LIST[index]->wVersion;
		pPackageInfo[index].wRPCID = SecPkgInfo_LIST[index]->wRPCID;
		pPackageInfo[index].cbMaxToken = SecPkgInfo_LIST[index]->cbMaxToken;
		pPackageInfo[index].Name = xstrdup(SecPkgInfo_LIST[index]->Name);
		pPackageInfo[index].Comment = xstrdup(SecPkgInfo_LIST[index]->Comment);
	}

	*(pcPackages) = cPackages;
	*(ppPackageInfo) = pPackageInfo;

	return SEC_E_OK;
}

void FreeContextBuffer_EnumerateSecurityPackages(void* contextBuffer)
{
	int index;
	uint32 cPackages;
	SecPkgInfo* pPackageInfo = (SecPkgInfo*) contextBuffer;

	cPackages = ARRAY_SIZE(SecPkgInfo_LIST);

	for (index = 0; index < (int) cPackages; index++)
	{
		if (pPackageInfo[index].Name)
			xfree(pPackageInfo[index].Name);

		if (pPackageInfo[index].Comment)
			xfree(pPackageInfo[index].Comment);
	}

	xfree(pPackageInfo);
}

SecurityFunctionTable* InitSecurityInterface(void)
{
	SecurityFunctionTable* table;
	table = xnew(SecurityFunctionTable);
	memcpy((void*) table, (void*) &SSPI_SecurityFunctionTable, sizeof(SecurityFunctionTable));
	return table;
}

SECURITY_STATUS QuerySecurityPackageInfo(char* pszPackageName, SecPkgInfo** ppPackageInfo)
{
	int index;
	size_t size;
	uint32 cPackages;
	SecPkgInfo* pPackageInfo;

	cPackages = ARRAY_SIZE(SecPkgInfo_LIST);

	for (index = 0; index < (int) cPackages; index++)
	{
		if (strcmp(pszPackageName, SecPkgInfo_LIST[index]->Name) == 0)
		{
			size = sizeof(SecPkgInfo);
			pPackageInfo = (SecPkgInfo*) sspi_ContextBufferAlloc(QuerySecurityPackageInfoIndex, size);

			pPackageInfo->fCapabilities = SecPkgInfo_LIST[index]->fCapabilities;
			pPackageInfo->wVersion = SecPkgInfo_LIST[index]->wVersion;
			pPackageInfo->wRPCID = SecPkgInfo_LIST[index]->wRPCID;
			pPackageInfo->cbMaxToken = SecPkgInfo_LIST[index]->cbMaxToken;
			pPackageInfo->Name = xstrdup(SecPkgInfo_LIST[index]->Name);
			pPackageInfo->Comment = xstrdup(SecPkgInfo_LIST[index]->Comment);

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

SECURITY_STATUS AcquireCredentialsHandle(char* pszPrincipal, char* pszPackage,
		uint32 fCredentialUse, void* pvLogonID, void* pAuthData, void* pGetKeyFn,
		void* pvGetKeyArgument, CredHandle* phCredential, SEC_TIMESTAMP* ptsExpiry)
{
	SECURITY_STATUS status;
	SecurityFunctionTable* table = sspi_GetSecurityFunctionTableByName(pszPackage);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->AcquireCredentialsHandle == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->AcquireCredentialsHandle(pszPrincipal, pszPackage, fCredentialUse,
			pvLogonID, pAuthData, pGetKeyFn, pvGetKeyArgument, phCredential, ptsExpiry);

	return status;
}

SECURITY_STATUS ExportSecurityContext(CtxtHandle* phContext, uint32 fFlags, SecBuffer* pPackedContext, void* pToken)
{
	return SEC_E_OK;
}

SECURITY_STATUS FreeCredentialsHandle(CredHandle* phCredential)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTable* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableByName(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->FreeCredentialsHandle == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->FreeCredentialsHandle(phCredential);

	return status;
}

SECURITY_STATUS ImportSecurityContext(char* pszPackage, SecBuffer* pPackedContext, void* pToken, CtxtHandle* phContext)
{
	return SEC_E_OK;
}

SECURITY_STATUS QueryCredentialsAttributes(CredHandle* phCredential, uint32 ulAttribute, void* pBuffer)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTable* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableByName(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->QueryCredentialsAttributes == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->QueryCredentialsAttributes(phCredential, ulAttribute, pBuffer);

	return status;
}

/* Context Management */

SECURITY_STATUS AcceptSecurityContext(CredHandle* phCredential, CtxtHandle* phContext,
		SecBufferDesc* pInput, uint32 fContextReq, uint32 TargetDataRep, CtxtHandle* phNewContext,
		SecBufferDesc* pOutput, uint32* pfContextAttr, SEC_TIMESTAMP* ptsTimeStamp)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTable* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableByName(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->AcceptSecurityContext == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->AcceptSecurityContext(phCredential, phContext, pInput, fContextReq,
			TargetDataRep, phNewContext, pOutput, pfContextAttr, ptsTimeStamp);

	return status;
}

SECURITY_STATUS ApplyControlToken(CtxtHandle* phContext, SecBufferDesc* pInput)
{
	return SEC_E_OK;
}

SECURITY_STATUS CompleteAuthToken(CtxtHandle* phContext, SecBufferDesc* pToken)
{
	return SEC_E_OK;
}

SECURITY_STATUS DeleteSecurityContext(CtxtHandle* phContext)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTable* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableByName(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->DeleteSecurityContext == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->DeleteSecurityContext(phContext);

	return status;
}

SECURITY_STATUS FreeContextBuffer(void* pvContextBuffer)
{
	if (!pvContextBuffer)
		return SEC_E_INVALID_HANDLE;

	sspi_ContextBufferFree(pvContextBuffer);

	return SEC_E_OK;
}

SECURITY_STATUS ImpersonateSecurityContext(CtxtHandle* phContext)
{
	return SEC_E_OK;
}

SECURITY_STATUS InitializeSecurityContext(CredHandle* phCredential, CtxtHandle* phContext,
		char* pszTargetName, uint32 fContextReq, uint32 Reserved1, uint32 TargetDataRep,
		SecBufferDesc* pInput, uint32 Reserved2, CtxtHandle* phNewContext,
		SecBufferDesc* pOutput, uint32* pfContextAttr, SEC_TIMESTAMP* ptsExpiry)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTable* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableByName(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->InitializeSecurityContext == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->InitializeSecurityContext(phCredential, phContext,
			pszTargetName, fContextReq, Reserved1, TargetDataRep,
			pInput, Reserved2, phNewContext, pOutput, pfContextAttr, ptsExpiry);

	return status;
}

SECURITY_STATUS QueryContextAttributes(CtxtHandle* phContext, uint32 ulAttribute, void* pBuffer)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTable* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableByName(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->QueryContextAttributes == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->QueryContextAttributes(phContext, ulAttribute, pBuffer);

	return status;
}

SECURITY_STATUS QuerySecurityContextToken(CtxtHandle* phContext, void* phToken)
{
	return SEC_E_OK;
}

SECURITY_STATUS SetContextAttributes(CtxtHandle* phContext, uint32 ulAttribute, void* pBuffer, uint32 cbBuffer)
{
	return SEC_E_OK;
}

SECURITY_STATUS RevertSecurityContext(CtxtHandle* phContext)
{
	return SEC_E_OK;
}

/* Message Support */

SECURITY_STATUS DecryptMessage(CtxtHandle* phContext, SecBufferDesc* pMessage, uint32 MessageSeqNo, uint32* pfQOP)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTable* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableByName(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->DecryptMessage == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->DecryptMessage(phContext, pMessage, MessageSeqNo, pfQOP);

	return status;
}

SECURITY_STATUS EncryptMessage(CtxtHandle* phContext, uint32 fQOP, SecBufferDesc* pMessage, uint32 MessageSeqNo)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTable* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableByName(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->EncryptMessage == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->EncryptMessage(phContext, fQOP, pMessage, MessageSeqNo);

	return status;
}

SECURITY_STATUS MakeSignature(CtxtHandle* phContext, uint32 fQOP, SecBufferDesc* pMessage, uint32 MessageSeqNo)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTable* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableByName(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->MakeSignature == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->MakeSignature(phContext, fQOP, pMessage, MessageSeqNo);

	return status;
}

SECURITY_STATUS VerifySignature(CtxtHandle* phContext, SecBufferDesc* pMessage, uint32 MessageSeqNo, uint32* pfQOP)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTable* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableByName(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (table->VerifySignature == NULL)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->VerifySignature(phContext, pMessage, MessageSeqNo, pfQOP);

	return status;
}

const SecurityFunctionTable SSPI_SecurityFunctionTable =
{
	1, /* dwVersion */
	EnumerateSecurityPackages, /* EnumerateSecurityPackages */
	NULL, /* Reserved1 */
	QueryCredentialsAttributes, /* QueryCredentialsAttributes */
	AcquireCredentialsHandle, /* AcquireCredentialsHandle */
	FreeCredentialsHandle, /* FreeCredentialsHandle */
	NULL, /* Reserved2 */
	InitializeSecurityContext, /* InitializeSecurityContext */
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
	QuerySecurityPackageInfo, /* QuerySecurityPackageInfo */
	NULL, /* Reserved3 */
	NULL, /* Reserved4 */
	ExportSecurityContext, /* ExportSecurityContext */
	ImportSecurityContext, /* ImportSecurityContext */
	NULL, /* AddCredentials */
	NULL, /* Reserved8 */
	QuerySecurityContextToken, /* QuerySecurityContextToken */
	EncryptMessage, /* EncryptMessage */
	DecryptMessage, /* DecryptMessage */
	SetContextAttributes, /* SetContextAttributes */
};

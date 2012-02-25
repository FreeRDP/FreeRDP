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

#include <freerdp/auth/sspi.h>

#include "sspi.h"

/* Authentication Functions: http://msdn.microsoft.com/en-us/library/windows/desktop/aa374731/ */

extern const SEC_PKG_INFO NTLM_SEC_PKG_INFO;
extern const SEC_PKG_INFO CREDSSP_SEC_PKG_INFO;

const SECURITY_FUNCTION_TABLE SSPI_SECURITY_FUNCTION_TABLE;
extern const SECURITY_FUNCTION_TABLE NTLM_SECURITY_FUNCTION_TABLE;
extern const SECURITY_FUNCTION_TABLE CREDSSP_SECURITY_FUNCTION_TABLE;

const SEC_PKG_INFO* SEC_PKG_INFO_LIST[] =
{
	&NTLM_SEC_PKG_INFO,
	&CREDSSP_SEC_PKG_INFO
};

struct _SECURITY_FUNCTION_TABLE_NAME
{
	char* Name;
	const SECURITY_FUNCTION_TABLE* security_function_table;
};
typedef struct _SECURITY_FUNCTION_TABLE_NAME SECURITY_FUNCTION_TABLE_NAME;

const SECURITY_FUNCTION_TABLE_NAME SECURITY_FUNCTION_TABLE_NAME_LIST[] =
{
	{ "NTLM", &NTLM_SECURITY_FUNCTION_TABLE },
	{ "CREDSSP", &CREDSSP_SECURITY_FUNCTION_TABLE }
};

#define SEC_HANDLE_LOWER_MAX	0xFFFFFFFF
#define SEC_HANDLE_UPPER_MAX	0xFFFFFFFE

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

	for (index = 0; index < ContextBufferAllocTable.cMaxEntries; index++)
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

void sspi_SecBufferAlloc(SEC_BUFFER* sec_buffer, size_t size)
{
	sec_buffer->cbBuffer = size;
	sec_buffer->pvBuffer = xzalloc(size);
}

void sspi_SecBufferFree(SEC_BUFFER* sec_buffer)
{
	sec_buffer->cbBuffer = 0;
	xfree(sec_buffer->pvBuffer);
}

SEC_HANDLE* sspi_SecureHandleAlloc()
{
	SEC_HANDLE* handle = xmalloc(sizeof(SEC_HANDLE));
	sspi_SecureHandleInit(handle);
	return handle;
}

void sspi_SecureHandleInit(SEC_HANDLE* handle)
{
	if (!handle)
		return;

	memset(handle, 0xFF, sizeof(SEC_HANDLE));
}

void sspi_SecureHandleInvalidate(SEC_HANDLE* handle)
{
	if (!handle)
		return;

	sspi_SecureHandleInit(handle);
}

void* sspi_SecureHandleGetLowerPointer(SEC_HANDLE* handle)
{
	void* pointer;

	if (!handle)
		return NULL;

	pointer = (void*) ~((size_t) handle->dwLower);

	return pointer;
}

void sspi_SecureHandleSetLowerPointer(SEC_HANDLE* handle, void* pointer)
{
	if (!handle)
		return;

	handle->dwLower = (uint32*) (~((size_t) pointer));
}

void* sspi_SecureHandleGetUpperPointer(SEC_HANDLE* handle)
{
	void* pointer;

	if (!handle)
		return NULL;

	pointer = (void*) ~((size_t) handle->dwUpper);

	return pointer;
}

void sspi_SecureHandleSetUpperPointer(SEC_HANDLE* handle, void* pointer)
{
	if (!handle)
		return;

	handle->dwUpper = (uint32*) (~((size_t) pointer));
}

void sspi_SecureHandleFree(SEC_HANDLE* handle)
{
	if (!handle)
		return;

	xfree(handle);
}

SECURITY_FUNCTION_TABLE* sspi_GetSecurityFunctionTableByName(const char* Name)
{
	int index;
	uint32 cPackages;

	cPackages = sizeof(SEC_PKG_INFO_LIST) / sizeof(SEC_PKG_INFO*);

	for (index = 0; index < cPackages; index++)
	{
		if (strcmp(Name, SECURITY_FUNCTION_TABLE_NAME_LIST[index].Name) == 0)
		{
			return (SECURITY_FUNCTION_TABLE*) SECURITY_FUNCTION_TABLE_NAME_LIST[index].security_function_table;
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

SECURITY_STATUS EnumerateSecurityPackages(uint32* pcPackages, SEC_PKG_INFO** ppPackageInfo)
{
	int index;
	size_t size;
	uint32 cPackages;
	SEC_PKG_INFO* pPackageInfo;

	cPackages = sizeof(SEC_PKG_INFO_LIST) / sizeof(SEC_PKG_INFO*);
	size = sizeof(SEC_PKG_INFO) * cPackages;

	pPackageInfo = (SEC_PKG_INFO*) sspi_ContextBufferAlloc(EnumerateSecurityPackagesIndex, size);

	for (index = 0; index < cPackages; index++)
	{
		pPackageInfo[index].fCapabilities = SEC_PKG_INFO_LIST[index]->fCapabilities;
		pPackageInfo[index].wVersion = SEC_PKG_INFO_LIST[index]->wVersion;
		pPackageInfo[index].wRPCID = SEC_PKG_INFO_LIST[index]->wRPCID;
		pPackageInfo[index].cbMaxToken = SEC_PKG_INFO_LIST[index]->cbMaxToken;
		pPackageInfo[index].Name = xstrdup(SEC_PKG_INFO_LIST[index]->Name);
		pPackageInfo[index].Comment = xstrdup(SEC_PKG_INFO_LIST[index]->Comment);
	}

	*(pcPackages) = cPackages;
	*(ppPackageInfo) = pPackageInfo;

	return SEC_E_OK;
}

void FreeContextBuffer_EnumerateSecurityPackages(void* contextBuffer)
{
	int index;
	uint32 cPackages;
	SEC_PKG_INFO* pPackageInfo = (SEC_PKG_INFO*) contextBuffer;

	cPackages = sizeof(SEC_PKG_INFO_LIST) / sizeof(SEC_PKG_INFO*);

	for (index = 0; index < cPackages; index++)
	{
		if (pPackageInfo[index].Name)
			xfree(pPackageInfo[index].Name);

		if (pPackageInfo[index].Comment)
			xfree(pPackageInfo[index].Comment);
	}

	xfree(pPackageInfo);
}

SECURITY_FUNCTION_TABLE* InitSecurityInterface(void)
{
	SECURITY_FUNCTION_TABLE* security_function_table;
	security_function_table = xnew(SECURITY_FUNCTION_TABLE);
	memcpy((void*) security_function_table, (void*) &SSPI_SECURITY_FUNCTION_TABLE, sizeof(SECURITY_FUNCTION_TABLE));
	return security_function_table;
}

SECURITY_STATUS QuerySecurityPackageInfo(char* pszPackageName, SEC_PKG_INFO** ppPackageInfo)
{
	int index;
	size_t size;
	uint32 cPackages;
	SEC_PKG_INFO* pPackageInfo;

	cPackages = sizeof(SEC_PKG_INFO_LIST) / sizeof(SEC_PKG_INFO*);

	for (index = 0; index < cPackages; index++)
	{
		if (strcmp(pszPackageName, SEC_PKG_INFO_LIST[index]->Name) == 0)
		{
			size = sizeof(SEC_PKG_INFO);
			pPackageInfo = (SEC_PKG_INFO*) sspi_ContextBufferAlloc(QuerySecurityPackageInfoIndex, size);

			pPackageInfo->fCapabilities = SEC_PKG_INFO_LIST[index]->fCapabilities;
			pPackageInfo->wVersion = SEC_PKG_INFO_LIST[index]->wVersion;
			pPackageInfo->wRPCID = SEC_PKG_INFO_LIST[index]->wRPCID;
			pPackageInfo->cbMaxToken = SEC_PKG_INFO_LIST[index]->cbMaxToken;
			pPackageInfo->Name = xstrdup(SEC_PKG_INFO_LIST[index]->Name);
			pPackageInfo->Comment = xstrdup(SEC_PKG_INFO_LIST[index]->Comment);

			*(ppPackageInfo) = pPackageInfo;

			return SEC_E_OK;
		}
	}

	*(ppPackageInfo) = NULL;

	return SEC_E_SECPKG_NOT_FOUND;
}

void FreeContextBuffer_QuerySecurityPackageInfo(void* contextBuffer)
{
	SEC_PKG_INFO* pPackageInfo = (SEC_PKG_INFO*) contextBuffer;

	if (pPackageInfo->Name)
		xfree(pPackageInfo->Name);

	if (pPackageInfo->Comment)
		xfree(pPackageInfo->Comment);

	xfree(pPackageInfo);
}

/* Credential Management */

SECURITY_STATUS AcquireCredentialsHandle(char* pszPrincipal, char* pszPackage,
		uint32 fCredentialUse, void* pvLogonID, void* pAuthData, void* pGetKeyFn,
		void* pvGetKeyArgument, CRED_HANDLE* phCredential, SEC_TIMESTAMP* ptsExpiry)
{
	SECURITY_STATUS status;
	SECURITY_FUNCTION_TABLE* table = sspi_GetSecurityFunctionTableByName(pszPackage);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!(table->AcquireCredentialsHandle))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->AcquireCredentialsHandle(pszPrincipal, pszPackage, fCredentialUse,
			pvLogonID, pAuthData, pGetKeyFn, pvGetKeyArgument, phCredential, ptsExpiry);

	return status;
}

SECURITY_STATUS ExportSecurityContext(CTXT_HANDLE* phContext, uint32 fFlags, SEC_BUFFER* pPackedContext, void* pToken)
{
	return SEC_E_OK;
}

SECURITY_STATUS FreeCredentialsHandle(CRED_HANDLE* phCredential)
{
	char* Name;
	SECURITY_STATUS status;
	SECURITY_FUNCTION_TABLE* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableByName(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!(table->FreeCredentialsHandle))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->FreeCredentialsHandle(phCredential);

	return status;
}

SECURITY_STATUS ImportSecurityContext(char* pszPackage, SEC_BUFFER* pPackedContext, void* pToken, CTXT_HANDLE* phContext)
{
	return SEC_E_OK;
}

SECURITY_STATUS QueryCredentialsAttributes(CRED_HANDLE* phCredential, uint32 ulAttribute, void* pBuffer)
{
	char* Name;
	SECURITY_STATUS status;
	SECURITY_FUNCTION_TABLE* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableByName(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	status = table->QueryCredentialsAttributes(phCredential, ulAttribute, pBuffer);

	return status;
}

/* Context Management */

SECURITY_STATUS AcceptSecurityContext(CRED_HANDLE* phCredential, CTXT_HANDLE* phContext,
		SEC_BUFFER_DESC* pInput, uint32 fContextReq, uint32 TargetDataRep, CTXT_HANDLE* phNewContext,
		SEC_BUFFER_DESC* pOutput, uint32* pfContextAttr, SEC_TIMESTAMP* ptsTimeStamp)
{
	return SEC_E_OK;
}

SECURITY_STATUS ApplyControlToken(CTXT_HANDLE* phContext, SEC_BUFFER_DESC* pInput)
{
	return SEC_E_OK;
}

SECURITY_STATUS CompleteAuthToken(CTXT_HANDLE* phContext, SEC_BUFFER_DESC* pToken)
{
	return SEC_E_OK;
}

SECURITY_STATUS DeleteSecurityContext(CTXT_HANDLE* phContext)
{
	return SEC_E_OK;
}

SECURITY_STATUS FreeContextBuffer(void* pvContextBuffer)
{
	if (!pvContextBuffer)
		return SEC_E_INVALID_HANDLE;

	sspi_ContextBufferFree(pvContextBuffer);

	return SEC_E_OK;
}

SECURITY_STATUS ImpersonateSecurityContext(CTXT_HANDLE* phContext)
{
	return SEC_E_OK;
}

SECURITY_STATUS InitializeSecurityContext(CRED_HANDLE* phCredential, CTXT_HANDLE* phContext,
		char* pszTargetName, uint32 fContextReq, uint32 Reserved1, uint32 TargetDataRep,
		SEC_BUFFER_DESC* pInput, uint32 Reserved2, CTXT_HANDLE* phNewContext,
		SEC_BUFFER_DESC* pOutput, uint32* pfContextAttr, SEC_TIMESTAMP* ptsExpiry)
{
	char* Name;
	SECURITY_STATUS status;
	SECURITY_FUNCTION_TABLE* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableByName(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	status = table->InitializeSecurityContext(phCredential, phContext,
			pszTargetName, fContextReq, Reserved1, TargetDataRep,
			pInput, Reserved2, phNewContext, pOutput, pfContextAttr, ptsExpiry);

	return status;
}

SECURITY_STATUS QueryContextAttributes(CTXT_HANDLE* phContext, uint32 ulAttribute, void* pBuffer)
{
	return SEC_E_OK;
}

SECURITY_STATUS QuerySecurityContextToken(CTXT_HANDLE* phContext, void* phToken)
{
	return SEC_E_OK;
}

SECURITY_STATUS SetContextAttributes(CTXT_HANDLE* phContext, uint32 ulAttribute, void* pBuffer, uint32 cbBuffer)
{
	return SEC_E_OK;
}

SECURITY_STATUS RevertSecurityContext(CTXT_HANDLE* phContext)
{
	return SEC_E_OK;
}

/* Message Support */

SECURITY_STATUS DecryptMessage(CTXT_HANDLE* phContext, SEC_BUFFER_DESC* pMessage, uint32 MessageSeqNo, uint32* pfQOP)
{
	return SEC_E_OK;
}

SECURITY_STATUS EncryptMessage(CTXT_HANDLE* phContext, uint32 fQOP, SEC_BUFFER_DESC* pMessage, uint32 MessageSeqNo)
{
	return SEC_E_OK;
}

SECURITY_STATUS MakeSignature(CTXT_HANDLE* phContext, uint32 fQOP, SEC_BUFFER_DESC* pMessage, uint32 MessageSeqNo)
{
	return SEC_E_OK;
}

SECURITY_STATUS VerifySignature(CTXT_HANDLE* phContext, SEC_BUFFER_DESC* pMessage, uint32 MessageSeqNo, uint32* pfQOP)
{
	return SEC_E_OK;
}

const SECURITY_FUNCTION_TABLE SSPI_SECURITY_FUNCTION_TABLE =
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

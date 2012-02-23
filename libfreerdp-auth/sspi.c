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

/* Package Management */

SECURITY_STATUS EnumerateSecurityPackages(uint32* pcPackages, SEC_PKG_INFO** ppPackageInfo)
{
	/*
	 * Name: Negotiate
	 * Comment: Microsoft Package Negotiator
	 * fCapabilities: 0x00083BB3
	 * wVersion: 1
	 * wRPCID: 0x0009
	 * cbMaxToken: 0x00002FE0
	 */

	/*
	 * Name: NegoExtender
	 * Comment: NegoExtender Security Package
	 * fCapabilities: 0x00113913
	 * wVersion: 1
	 * wRPCID: 0x001E
	 * cbMaxToken: 0x00002EE0
	 */

	/*
	 * Name: Kerberos
	 * Comment: Microsoft Kerberos V1.0
	 * fCapabilities: 0x000F3BBF
	 * wVersion: 1
	 * wRPCID: 0x0010
	 * cbMaxToken: 0x00002EE0
	 */

	/*
	 * Name: NTLM
	 * Comment: NTLM Security Package
	 * fCapabilities: 0x00082B37
	 * wVersion: 1
	 * wRPCID: 0x000A
	 * cbMaxToken: 0x00000B48
	 */

	/*
	 * Name: Schannel
	 * Comment: Schannel Security Package
	 * fCapabilities: 0x000107B3
	 * wVersion: 1
	 * wRPCID: 0x000E
	 * cbMaxToken: 0x00006000
	 */

	/*
	 * Name: TSSSP
	 * Comment: TS Service Security Package
	 * fCapabilities: 0x00010230
	 * wVersion: 1
	 * wRPCID: 0x0016
	 * cbMaxToken: 0x000032C8
	 */

	/*
	 * Name: CREDSSP
	 * Comment: Microsoft CredSSP Security Provider
	 * fCapabilities: 0x000110733
	 * wVersion: 1
	 * wRPCID: 0xFFFF
	 * cbMaxToken: 0x000090A8
	 */

	int index;
	uint32 cPackages;
	SEC_PKG_INFO* pPackageInfo;

	cPackages = sizeof(SEC_PKG_INFO_LIST) / sizeof(SEC_PKG_INFO*);
	pPackageInfo = (SEC_PKG_INFO*) xmalloc(sizeof(SEC_PKG_INFO) * cPackages);

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
	uint32 cPackages;
	SEC_PKG_INFO* pPackageInfo;

	cPackages = sizeof(SEC_PKG_INFO_LIST) / sizeof(SEC_PKG_INFO*);

	for (index = 0; index < cPackages; index++)
	{
		if (strcmp(pszPackageName, SEC_PKG_INFO_LIST[index]->Name) == 0)
		{
			pPackageInfo = (SEC_PKG_INFO*) xmalloc(sizeof(SEC_PKG_INFO));

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
	return SEC_E_OK;
}

SECURITY_STATUS ImportSecurityContext(char* pszPackage, SEC_BUFFER* pPackedContext, void* pToken, CTXT_HANDLE* phContext)
{
	return SEC_E_OK;
}

SECURITY_STATUS QueryCredentialsAttributes(CRED_HANDLE* phCredential, uint32 ulAttribute, void* pBuffer)
{
	return SEC_E_OK;
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
	if (pvContextBuffer != NULL)
		xfree(pvContextBuffer);

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
	return SEC_E_OK;
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

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

	return SEC_E_OK;
}

SECURITY_FUNCTION_TABLE* InitSecurityInterface(void)
{
	SECURITY_FUNCTION_TABLE* security_function_table;
	security_function_table = xnew(SECURITY_FUNCTION_TABLE);
	return security_function_table;
}

SECURITY_STATUS QuerySecurityPackageInfo(char* pszPackageName, SEC_PKG_INFO** ppPackageInfo)
{
	return SEC_E_OK;
}

/* Credential Management */

SECURITY_STATUS AcquireCredentialsHandle(char* pszPrincipal, char* pszPackage,
		uint32 fCredentialUse, void* pvLogonID, void* pAuthData, void* pGetKeyFn,
		void* pvGetKeyArgument, CRED_HANDLE* phCredential, SEC_TIMESTAMP* ptsExpiry)
{
	return SEC_E_OK;
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

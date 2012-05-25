/**
 * WinPR: Windows Portable Runtime
 * Schannel Security Package
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

#include <winpr/crt.h>
#include <winpr/sspi.h>

#include "schannel.h"

#include "../sspi.h"

char* SCHANNEL_PACKAGE_NAME = "Schannel";

SECURITY_STATUS SEC_ENTRY schannel_InitializeSecurityContextW(PCredHandle phCredential, PCtxtHandle phContext,
		SEC_WCHAR* pszTargetName, ULONG fContextReq, ULONG Reserved1, ULONG TargetDataRep,
		PSecBufferDesc pInput, ULONG Reserved2, PCtxtHandle phNewContext,
		PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY schannel_InitializeSecurityContextA(PCredHandle phCredential, PCtxtHandle phContext,
		SEC_CHAR* pszTargetName, ULONG fContextReq, ULONG Reserved1, ULONG TargetDataRep,
		PSecBufferDesc pInput, ULONG Reserved2, PCtxtHandle phNewContext,
		PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	SCHANNEL_CONTEXT* context;
	CREDENTIALS* credentials;

	context = sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
	{
		context = schannel_ContextNew();

		credentials = (CREDENTIALS*) sspi_SecureHandleGetLowerPointer(phCredential);

		sspi_SecureHandleSetLowerPointer(phNewContext, context);
		sspi_SecureHandleSetUpperPointer(phNewContext, (void*) SCHANNEL_PACKAGE_NAME);
	}

	return SEC_E_OK;

}

SCHANNEL_CONTEXT* schannel_ContextNew()
{
	SCHANNEL_CONTEXT* context;

	context = (SCHANNEL_CONTEXT*) calloc(1, sizeof(SCHANNEL_CONTEXT));

	if (context != NULL)
	{

	}

	return context;
}

void schannel_ContextFree(SCHANNEL_CONTEXT* context)
{
	if (!context)
		return;

	free(context);
}

SECURITY_STATUS SEC_ENTRY schannel_QueryContextAttributes(PCtxtHandle phContext, ULONG ulAttribute, void* pBuffer)
{
	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	return SEC_E_UNSUPPORTED_FUNCTION;
}

SECURITY_STATUS SEC_ENTRY schannel_AcquireCredentialsHandleW(SEC_WCHAR* pszPrincipal, SEC_WCHAR* pszPackage,
		ULONG fCredentialUse, PLUID pvLogonID, void* pAuthData, void* pGetKeyFn,
		void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY schannel_AcquireCredentialsHandleA(SEC_CHAR* pszPrincipal, SEC_CHAR* pszPackage,
		ULONG fCredentialUse, PLUID pvLogonID, void* pAuthData, void* pGetKeyFn,
		void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry)
{
	CREDENTIALS* credentials;
	SEC_WINNT_AUTH_IDENTITY* identity;

	if (fCredentialUse == SECPKG_CRED_OUTBOUND)
	{
		credentials = sspi_CredentialsNew();
		identity = (SEC_WINNT_AUTH_IDENTITY*) pAuthData;

		//CopyMemory(&(credentials->identity), identity, sizeof(SEC_WINNT_AUTH_IDENTITY));

		sspi_SecureHandleSetLowerPointer(phCredential, (void*) credentials);
		sspi_SecureHandleSetUpperPointer(phCredential, (void*) SCHANNEL_PACKAGE_NAME);

		return SEC_E_OK;
	}

	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY schannel_QueryCredentialsAttributesW(PCredHandle phCredential, ULONG ulAttribute, void* pBuffer)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY schannel_QueryCredentialsAttributesA(PCredHandle phCredential, ULONG ulAttribute, void* pBuffer)
{
	if (ulAttribute == SECPKG_CRED_ATTR_NAMES)
	{
		CREDENTIALS* credentials;

		credentials = (CREDENTIALS*) sspi_SecureHandleGetLowerPointer(phCredential);

		return SEC_E_OK;
	}

	return SEC_E_UNSUPPORTED_FUNCTION;
}

SECURITY_STATUS SEC_ENTRY schannel_FreeCredentialsHandle(PCredHandle phCredential)
{
	CREDENTIALS* credentials;

	if (!phCredential)
		return SEC_E_INVALID_HANDLE;

	credentials = (CREDENTIALS*) sspi_SecureHandleGetLowerPointer(phCredential);

	if (!credentials)
		return SEC_E_INVALID_HANDLE;

	sspi_CredentialsFree(credentials);

	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY schannel_EncryptMessage(PCtxtHandle phContext, ULONG fQOP, PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY schannel_DecryptMessage(PCtxtHandle phContext, PSecBufferDesc pMessage, ULONG MessageSeqNo, ULONG* pfQOP)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY schannel_MakeSignature(PCtxtHandle phContext, ULONG fQOP, PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY schannel_VerifySignature(PCtxtHandle phContext, PSecBufferDesc pMessage, ULONG MessageSeqNo, ULONG* pfQOP)
{
	return SEC_E_OK;
}

const SecurityFunctionTableA SCHANNEL_SecurityFunctionTableA =
{
	1, /* dwVersion */
	NULL, /* EnumerateSecurityPackages */
	schannel_QueryCredentialsAttributesA, /* QueryCredentialsAttributes */
	schannel_AcquireCredentialsHandleA, /* AcquireCredentialsHandle */
	schannel_FreeCredentialsHandle, /* FreeCredentialsHandle */
	NULL, /* Reserved2 */
	schannel_InitializeSecurityContextA, /* InitializeSecurityContext */
	NULL, /* AcceptSecurityContext */
	NULL, /* CompleteAuthToken */
	NULL, /* DeleteSecurityContext */
	NULL, /* ApplyControlToken */
	schannel_QueryContextAttributes, /* QueryContextAttributes */
	NULL, /* ImpersonateSecurityContext */
	NULL, /* RevertSecurityContext */
	schannel_MakeSignature, /* MakeSignature */
	schannel_VerifySignature, /* VerifySignature */
	NULL, /* FreeContextBuffer */
	NULL, /* QuerySecurityPackageInfo */
	NULL, /* Reserved3 */
	NULL, /* Reserved4 */
	NULL, /* ExportSecurityContext */
	NULL, /* ImportSecurityContext */
	NULL, /* AddCredentials */
	NULL, /* Reserved8 */
	NULL, /* QuerySecurityContextToken */
	schannel_EncryptMessage, /* EncryptMessage */
	schannel_DecryptMessage, /* DecryptMessage */
	NULL, /* SetContextAttributes */
};

const SecurityFunctionTableW SCHANNEL_SecurityFunctionTableW =
{
	1, /* dwVersion */
	NULL, /* EnumerateSecurityPackages */
	schannel_QueryCredentialsAttributesW, /* QueryCredentialsAttributes */
	schannel_AcquireCredentialsHandleW, /* AcquireCredentialsHandle */
	schannel_FreeCredentialsHandle, /* FreeCredentialsHandle */
	NULL, /* Reserved2 */
	schannel_InitializeSecurityContextW, /* InitializeSecurityContext */
	NULL, /* AcceptSecurityContext */
	NULL, /* CompleteAuthToken */
	NULL, /* DeleteSecurityContext */
	NULL, /* ApplyControlToken */
	schannel_QueryContextAttributes, /* QueryContextAttributes */
	NULL, /* ImpersonateSecurityContext */
	NULL, /* RevertSecurityContext */
	schannel_MakeSignature, /* MakeSignature */
	schannel_VerifySignature, /* VerifySignature */
	NULL, /* FreeContextBuffer */
	NULL, /* QuerySecurityPackageInfo */
	NULL, /* Reserved3 */
	NULL, /* Reserved4 */
	NULL, /* ExportSecurityContext */
	NULL, /* ImportSecurityContext */
	NULL, /* AddCredentials */
	NULL, /* Reserved8 */
	NULL, /* QuerySecurityContextToken */
	schannel_EncryptMessage, /* EncryptMessage */
	schannel_DecryptMessage, /* DecryptMessage */
	NULL, /* SetContextAttributes */
};

const SecPkgInfoA SCHANNEL_SecPkgInfoA =
{
	0x000107B3, /* fCapabilities */
	1, /* wVersion */
	0x000E, /* wRPCID */
	0x00006000, /* cbMaxToken */
	"Schannel", /* Name */
	"Schannel Security Package" /* Comment */
};

const SecPkgInfoW SCHANNEL_SecPkgInfoW =
{
	0x000107B3, /* fCapabilities */
	1, /* wVersion */
	0x000E, /* wRPCID */
	0x00006000, /* cbMaxToken */
	L"Schannel", /* Name */
	L"Schannel Security Package" /* Comment */
};


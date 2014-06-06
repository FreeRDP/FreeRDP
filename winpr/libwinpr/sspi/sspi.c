/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Security Support Provider Interface (SSPI)
 *
 * Copyright 2012-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>
#include <winpr/library.h>

#include "sspi.h"

static BOOL g_Initialized = FALSE;
static HMODULE g_SspiModule = NULL;

static SecurityFunctionTableW* g_SspiW = NULL;
static SecurityFunctionTableA* g_SspiA = NULL;

SecurityFunctionTableA sspi_SecurityFunctionTableA;
SecurityFunctionTableW sspi_SecurityFunctionTableW;

BOOL InitializeSspiModule_Native(void)
{
#ifdef _WIN32
	INIT_SECURITY_INTERFACE_W pInitSecurityInterfaceW;
	INIT_SECURITY_INTERFACE_A pInitSecurityInterfaceA;

	g_SspiModule = LoadLibraryA("secur32.dll");

	if (!g_SspiModule)
		g_SspiModule = LoadLibraryA("security.dll");

	if (!g_SspiModule)
		return FALSE;

	pInitSecurityInterfaceW = (INIT_SECURITY_INTERFACE_W) GetProcAddress(g_SspiModule, "InitSecurityInterfaceW");
	pInitSecurityInterfaceA = (INIT_SECURITY_INTERFACE_A) GetProcAddress(g_SspiModule, "InitSecurityInterfaceA");

	if (pInitSecurityInterfaceW)
		g_SspiW = pInitSecurityInterfaceW();

	if (pInitSecurityInterfaceA)
		g_SspiA = pInitSecurityInterfaceA();

	return TRUE;
#else
	return FALSE;
#endif
}

void InitializeSspiModule(void)
{
	if (g_Initialized)
		return;

	g_Initialized = TRUE;

	if (!InitializeSspiModule_Native())
	{
		printf("WINPR SSPI!\n");

		g_SspiW = winpr_InitSecurityInterfaceW();
		g_SspiA = winpr_InitSecurityInterfaceA();
	}
	else
	{
		printf("NATIVE SSPI!\n");
	}
}

/**
 * Standard SSPI API
 */

/* Package Management */

SECURITY_STATUS SEC_ENTRY EnumerateSecurityPackagesW(ULONG* pcPackages, PSecPkgInfoW* ppPackageInfo)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiW && g_SspiW->EnumerateSecurityPackagesW))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->EnumerateSecurityPackagesW(pcPackages, ppPackageInfo);

	return status;
}

SECURITY_STATUS SEC_ENTRY EnumerateSecurityPackagesA(ULONG* pcPackages, PSecPkgInfoA* ppPackageInfo)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiA && g_SspiA->EnumerateSecurityPackagesA))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiA->EnumerateSecurityPackagesA(pcPackages, ppPackageInfo);

	return status;
}

SecurityFunctionTableW* SEC_ENTRY InitSecurityInterfaceW(void)
{
	if (!g_Initialized)
		InitializeSspiModule();

	return &sspi_SecurityFunctionTableW;
}

SecurityFunctionTableA* SEC_ENTRY InitSecurityInterfaceA(void)
{
	if (!g_Initialized)
		InitializeSspiModule();

	return &sspi_SecurityFunctionTableA;
}

SECURITY_STATUS SEC_ENTRY QuerySecurityPackageInfoW(SEC_WCHAR* pszPackageName, PSecPkgInfoW* ppPackageInfo)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiW && g_SspiW->QuerySecurityPackageInfoW))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->QuerySecurityPackageInfoW(pszPackageName, ppPackageInfo);

	return status;
}

SECURITY_STATUS SEC_ENTRY QuerySecurityPackageInfoA(SEC_CHAR* pszPackageName, PSecPkgInfoA* ppPackageInfo)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiA && g_SspiA->QuerySecurityPackageInfoA))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiA->QuerySecurityPackageInfoA(pszPackageName, ppPackageInfo);

	return status;
}

/* Credential Management */

SECURITY_STATUS SEC_ENTRY AcquireCredentialsHandleW(SEC_WCHAR* pszPrincipal, SEC_WCHAR* pszPackage,
		ULONG fCredentialUse, void* pvLogonID, void* pAuthData, SEC_GET_KEY_FN pGetKeyFn,
		void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiW && g_SspiW->AcquireCredentialsHandleW))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->AcquireCredentialsHandleW(pszPrincipal, pszPackage, fCredentialUse,
		pvLogonID, pAuthData, pGetKeyFn, pvGetKeyArgument, phCredential, ptsExpiry);

	return status;
}

SECURITY_STATUS SEC_ENTRY AcquireCredentialsHandleA(SEC_CHAR* pszPrincipal, SEC_CHAR* pszPackage,
		ULONG fCredentialUse, void* pvLogonID, void* pAuthData, SEC_GET_KEY_FN pGetKeyFn,
		void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiA && g_SspiA->AcquireCredentialsHandleA))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiA->AcquireCredentialsHandleA(pszPrincipal, pszPackage, fCredentialUse,
		pvLogonID, pAuthData, pGetKeyFn, pvGetKeyArgument, phCredential, ptsExpiry);

	return status;
}

SECURITY_STATUS SEC_ENTRY ExportSecurityContext(PCtxtHandle phContext, ULONG fFlags, PSecBuffer pPackedContext, HANDLE* pToken)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiW && g_SspiW->ExportSecurityContext))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->ExportSecurityContext(phContext, fFlags, pPackedContext, pToken);

	return status;
}

SECURITY_STATUS SEC_ENTRY FreeCredentialsHandle(PCredHandle phCredential)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiW && g_SspiW->FreeCredentialsHandle))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->FreeCredentialsHandle(phCredential);

	return status;
}

SECURITY_STATUS SEC_ENTRY ImportSecurityContextW(SEC_WCHAR* pszPackage, PSecBuffer pPackedContext, HANDLE pToken, PCtxtHandle phContext)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiW && g_SspiW->ImportSecurityContextW))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->ImportSecurityContextW(pszPackage, pPackedContext, pToken, phContext);

	return status;
}

SECURITY_STATUS SEC_ENTRY ImportSecurityContextA(SEC_CHAR* pszPackage, PSecBuffer pPackedContext, HANDLE pToken, PCtxtHandle phContext)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiA && g_SspiA->ImportSecurityContextA))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiA->ImportSecurityContextA(pszPackage, pPackedContext, pToken, phContext);

	return status;
}

SECURITY_STATUS SEC_ENTRY QueryCredentialsAttributesW(PCredHandle phCredential, ULONG ulAttribute, void* pBuffer)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiW && g_SspiW->QueryCredentialsAttributesW))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->QueryCredentialsAttributesW(phCredential, ulAttribute, pBuffer);

	return status;
}

SECURITY_STATUS SEC_ENTRY QueryCredentialsAttributesA(PCredHandle phCredential, ULONG ulAttribute, void* pBuffer)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiA && g_SspiA->QueryCredentialsAttributesA))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiA->QueryCredentialsAttributesA(phCredential, ulAttribute, pBuffer);

	return status;
}

/* Context Management */

SECURITY_STATUS SEC_ENTRY AcceptSecurityContext(PCredHandle phCredential, PCtxtHandle phContext,
		PSecBufferDesc pInput, ULONG fContextReq, ULONG TargetDataRep, PCtxtHandle phNewContext,
		PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsTimeStamp)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiW && g_SspiW->AcceptSecurityContext))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->AcceptSecurityContext(phCredential, phContext, pInput, fContextReq,
		TargetDataRep, phNewContext, pOutput, pfContextAttr, ptsTimeStamp);

	return status;
}

SECURITY_STATUS SEC_ENTRY ApplyControlToken(PCtxtHandle phContext, PSecBufferDesc pInput)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiW && g_SspiW->ApplyControlToken))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->ApplyControlToken(phContext, pInput);

	return status;
}

SECURITY_STATUS SEC_ENTRY CompleteAuthToken(PCtxtHandle phContext, PSecBufferDesc pToken)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiW && g_SspiW->CompleteAuthToken))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->CompleteAuthToken(phContext, pToken);

	return status;
}

SECURITY_STATUS SEC_ENTRY DeleteSecurityContext(PCtxtHandle phContext)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiW && g_SspiW->DeleteSecurityContext))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->DeleteSecurityContext(phContext);

	return status;
}

SECURITY_STATUS SEC_ENTRY FreeContextBuffer(void* pvContextBuffer)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiW && g_SspiW->FreeContextBuffer))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->FreeContextBuffer(pvContextBuffer);

	return status;
}

SECURITY_STATUS SEC_ENTRY ImpersonateSecurityContext(PCtxtHandle phContext)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiW && g_SspiW->ImpersonateSecurityContext))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->ImpersonateSecurityContext(phContext);

	return status;
}

SECURITY_STATUS SEC_ENTRY InitializeSecurityContextW(PCredHandle phCredential, PCtxtHandle phContext,
		SEC_WCHAR* pszTargetName, ULONG fContextReq, ULONG Reserved1, ULONG TargetDataRep,
		PSecBufferDesc pInput, ULONG Reserved2, PCtxtHandle phNewContext,
		PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiW && g_SspiW->InitializeSecurityContextW))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->InitializeSecurityContextW(phCredential, phContext,
		pszTargetName, fContextReq, Reserved1, TargetDataRep, pInput,
		Reserved2, phNewContext, pOutput, pfContextAttr, ptsExpiry);

	return status;
}

SECURITY_STATUS SEC_ENTRY InitializeSecurityContextA(PCredHandle phCredential, PCtxtHandle phContext,
		SEC_CHAR* pszTargetName, ULONG fContextReq, ULONG Reserved1, ULONG TargetDataRep,
		PSecBufferDesc pInput, ULONG Reserved2, PCtxtHandle phNewContext,
		PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiA && g_SspiA->InitializeSecurityContextA))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiA->InitializeSecurityContextA(phCredential, phContext,
		pszTargetName, fContextReq, Reserved1, TargetDataRep, pInput,
		Reserved2, phNewContext, pOutput, pfContextAttr, ptsExpiry);

	return status;
}

SECURITY_STATUS SEC_ENTRY QueryContextAttributesW(PCtxtHandle phContext, ULONG ulAttribute, void* pBuffer)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiW && g_SspiW->QueryContextAttributesW))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->QueryContextAttributesW(phContext, ulAttribute, pBuffer);

	return status;
}

SECURITY_STATUS SEC_ENTRY QueryContextAttributesA(PCtxtHandle phContext, ULONG ulAttribute, void* pBuffer)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiA && g_SspiA->QueryContextAttributesA))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiA->QueryContextAttributesA(phContext, ulAttribute, pBuffer);

	return status;
}

SECURITY_STATUS SEC_ENTRY QuerySecurityContextToken(PCtxtHandle phContext, HANDLE* phToken)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiW && g_SspiW->QuerySecurityContextToken))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->QuerySecurityContextToken(phContext, phToken);

	return status;
}

SECURITY_STATUS SEC_ENTRY SetContextAttributesW(PCtxtHandle phContext, ULONG ulAttribute, void* pBuffer, ULONG cbBuffer)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiW && g_SspiW->SetContextAttributesW))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->SetContextAttributesW(phContext, ulAttribute, pBuffer, cbBuffer);

	return status;
}

SECURITY_STATUS SEC_ENTRY SetContextAttributesA(PCtxtHandle phContext, ULONG ulAttribute, void* pBuffer, ULONG cbBuffer)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiA && g_SspiA->SetContextAttributesA))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiA->SetContextAttributesA(phContext, ulAttribute, pBuffer, cbBuffer);

	return status;
}

SECURITY_STATUS SEC_ENTRY RevertSecurityContext(PCtxtHandle phContext)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiW && g_SspiW->RevertSecurityContext))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->RevertSecurityContext(phContext);

	return status;
}

/* Message Support */

SECURITY_STATUS SEC_ENTRY DecryptMessage(PCtxtHandle phContext, PSecBufferDesc pMessage, ULONG MessageSeqNo, PULONG pfQOP)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiW && g_SspiW->DecryptMessage))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->DecryptMessage(phContext, pMessage, MessageSeqNo, pfQOP);

	return status;
}

SECURITY_STATUS SEC_ENTRY EncryptMessage(PCtxtHandle phContext, ULONG fQOP, PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiW && g_SspiW->EncryptMessage))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->EncryptMessage(phContext, fQOP, pMessage, MessageSeqNo);

	return status;
}

SECURITY_STATUS SEC_ENTRY MakeSignature(PCtxtHandle phContext, ULONG fQOP, PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiW && g_SspiW->MakeSignature))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->MakeSignature(phContext, fQOP, pMessage, MessageSeqNo);

	return status;
}

SECURITY_STATUS SEC_ENTRY VerifySignature(PCtxtHandle phContext, PSecBufferDesc pMessage, ULONG MessageSeqNo, PULONG pfQOP)
{
	SECURITY_STATUS status;

	if (!g_Initialized)
		InitializeSspiModule();

	if (!(g_SspiW && g_SspiW->VerifySignature))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->VerifySignature(phContext, pMessage, MessageSeqNo, pfQOP);

	return status;
}

SecurityFunctionTableA sspi_SecurityFunctionTableA =
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
	SetContextAttributesA, /* SetContextAttributes */
};

SecurityFunctionTableW sspi_SecurityFunctionTableW =
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
	SetContextAttributesW, /* SetContextAttributes */
};

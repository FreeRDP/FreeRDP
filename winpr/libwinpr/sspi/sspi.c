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

#define _NO_KSECDD_IMPORT_	1

#include <winpr/sspi.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/wlog.h>
#include <winpr/library.h>
#include <winpr/environment.h>

#include "sspi.h"

static wLog* g_Log = NULL;

static INIT_ONCE g_Initialized = INIT_ONCE_STATIC_INIT;
#if defined(WITH_NATIVE_SSPI)
static HMODULE g_SspiModule = NULL;
#endif

static SecurityFunctionTableW* g_SspiW = NULL;
static SecurityFunctionTableA* g_SspiA = NULL;

#if defined(WITH_NATIVE_SSPI)
static BOOL ShouldUseNativeSspi(void);
static BOOL InitializeSspiModule_Native(void);
#endif

#if defined(WITH_NATIVE_SSPI)
BOOL ShouldUseNativeSspi(void)
{
	BOOL status = FALSE;
#ifdef _WIN32
	LPCSTR sspi = "WINPR_NATIVE_SSPI";
	DWORD nSize;
	char* env = NULL;
	nSize = GetEnvironmentVariableA(sspi, NULL, 0);

	if (!nSize)
		return TRUE;

	env = (LPSTR) malloc(nSize);

	if (!env)
		return TRUE;

	if (GetEnvironmentVariableA(sspi, env, nSize) != nSize - 1)
	{
		free(env);
		return TRUE;
	}

	if (strcmp(env, "0") == 0)
		status = FALSE;
	else
		status = TRUE;

	free(env);
#endif
	return status;
}
#endif

#if defined(WITH_NATIVE_SSPI)
BOOL InitializeSspiModule_Native(void)
{
	INIT_SECURITY_INTERFACE_W pInitSecurityInterfaceW;
	INIT_SECURITY_INTERFACE_A pInitSecurityInterfaceA;
	g_SspiModule = LoadLibraryA("secur32.dll");

	if (!g_SspiModule)
		g_SspiModule = LoadLibraryA("security.dll");

	if (!g_SspiModule)
		return FALSE;

	pInitSecurityInterfaceW = (INIT_SECURITY_INTERFACE_W) GetProcAddress(g_SspiModule,
	                          "InitSecurityInterfaceW");
	pInitSecurityInterfaceA = (INIT_SECURITY_INTERFACE_A) GetProcAddress(g_SspiModule,
	                          "InitSecurityInterfaceA");

	if (pInitSecurityInterfaceW)
		g_SspiW = pInitSecurityInterfaceW();

	if (pInitSecurityInterfaceA)
		g_SspiA = pInitSecurityInterfaceA();

	return TRUE;
}
#endif

static BOOL CALLBACK InitializeSspiModuleInt(PINIT_ONCE once, PVOID param, PVOID* context)
{
	BOOL status = FALSE;
#if defined(WITH_NATIVE_SSPI)
	DWORD flags = 0;

	if (param)
		flags = *(DWORD*)param;

#endif
	sspi_GlobalInit();
	g_Log = WLog_Get("com.winpr.sspi");
#if defined(WITH_NATIVE_SSPI)

	if (flags && (flags & SSPI_INTERFACE_NATIVE))
	{
		status = InitializeSspiModule_Native();
	}
	else if (flags && (flags & SSPI_INTERFACE_WINPR))
	{
		g_SspiW = winpr_InitSecurityInterfaceW();
		g_SspiA = winpr_InitSecurityInterfaceA();
		status = TRUE;
	}

	if (!status && ShouldUseNativeSspi())
	{
		status = InitializeSspiModule_Native();
	}

#endif

	if (!status)
	{
		g_SspiW = winpr_InitSecurityInterfaceW();
		g_SspiA = winpr_InitSecurityInterfaceA();
	}

	return TRUE;
}

const char* GetSecurityStatusString(SECURITY_STATUS status)
{
	switch (status)
	{
		case SEC_E_OK:
			return "SEC_E_OK";

		case SEC_E_INSUFFICIENT_MEMORY:
			return "SEC_E_INSUFFICIENT_MEMORY";

		case SEC_E_INVALID_HANDLE:
			return "SEC_E_INVALID_HANDLE";

		case SEC_E_UNSUPPORTED_FUNCTION:
			return "SEC_E_UNSUPPORTED_FUNCTION";

		case SEC_E_TARGET_UNKNOWN:
			return "SEC_E_TARGET_UNKNOWN";

		case SEC_E_INTERNAL_ERROR:
			return "SEC_E_INTERNAL_ERROR";

		case SEC_E_SECPKG_NOT_FOUND:
			return "SEC_E_SECPKG_NOT_FOUND";

		case SEC_E_NOT_OWNER:
			return "SEC_E_NOT_OWNER";

		case SEC_E_CANNOT_INSTALL:
			return "SEC_E_CANNOT_INSTALL";

		case SEC_E_INVALID_TOKEN:
			return "SEC_E_INVALID_TOKEN";

		case SEC_E_CANNOT_PACK:
			return "SEC_E_CANNOT_PACK";

		case SEC_E_QOP_NOT_SUPPORTED:
			return "SEC_E_QOP_NOT_SUPPORTED";

		case SEC_E_NO_IMPERSONATION:
			return "SEC_E_NO_IMPERSONATION";

		case SEC_E_LOGON_DENIED:
			return "SEC_E_LOGON_DENIED";

		case SEC_E_UNKNOWN_CREDENTIALS:
			return "SEC_E_UNKNOWN_CREDENTIALS";

		case SEC_E_NO_CREDENTIALS:
			return "SEC_E_NO_CREDENTIALS";

		case SEC_E_MESSAGE_ALTERED:
			return "SEC_E_MESSAGE_ALTERED";

		case SEC_E_OUT_OF_SEQUENCE:
			return "SEC_E_OUT_OF_SEQUENCE";

		case SEC_E_NO_AUTHENTICATING_AUTHORITY:
			return "SEC_E_NO_AUTHENTICATING_AUTHORITY";

		case SEC_E_BAD_PKGID:
			return "SEC_E_BAD_PKGID";

		case SEC_E_CONTEXT_EXPIRED:
			return "SEC_E_CONTEXT_EXPIRED";

		case SEC_E_INCOMPLETE_MESSAGE:
			return "SEC_E_INCOMPLETE_MESSAGE";

		case SEC_E_INCOMPLETE_CREDENTIALS:
			return "SEC_E_INCOMPLETE_CREDENTIALS";

		case SEC_E_BUFFER_TOO_SMALL:
			return "SEC_E_BUFFER_TOO_SMALL";

		case SEC_E_WRONG_PRINCIPAL:
			return "SEC_E_WRONG_PRINCIPAL";

		case SEC_E_TIME_SKEW:
			return "SEC_E_TIME_SKEW";

		case SEC_E_UNTRUSTED_ROOT:
			return "SEC_E_UNTRUSTED_ROOT";

		case SEC_E_ILLEGAL_MESSAGE:
			return "SEC_E_ILLEGAL_MESSAGE";

		case SEC_E_CERT_UNKNOWN:
			return "SEC_E_CERT_UNKNOWN";

		case SEC_E_CERT_EXPIRED:
			return "SEC_E_CERT_EXPIRED";

		case SEC_E_ENCRYPT_FAILURE:
			return "SEC_E_ENCRYPT_FAILURE";

		case SEC_E_DECRYPT_FAILURE:
			return "SEC_E_DECRYPT_FAILURE";

		case SEC_E_ALGORITHM_MISMATCH:
			return "SEC_E_ALGORITHM_MISMATCH";

		case SEC_E_SECURITY_QOS_FAILED:
			return "SEC_E_SECURITY_QOS_FAILED";

		case SEC_E_UNFINISHED_CONTEXT_DELETED:
			return "SEC_E_UNFINISHED_CONTEXT_DELETED";

		case SEC_E_NO_TGT_REPLY:
			return "SEC_E_NO_TGT_REPLY";

		case SEC_E_NO_IP_ADDRESSES:
			return "SEC_E_NO_IP_ADDRESSES";

		case SEC_E_WRONG_CREDENTIAL_HANDLE:
			return "SEC_E_WRONG_CREDENTIAL_HANDLE";

		case SEC_E_CRYPTO_SYSTEM_INVALID:
			return "SEC_E_CRYPTO_SYSTEM_INVALID";

		case SEC_E_MAX_REFERRALS_EXCEEDED:
			return "SEC_E_MAX_REFERRALS_EXCEEDED";

		case SEC_E_MUST_BE_KDC:
			return "SEC_E_MUST_BE_KDC";

		case SEC_E_STRONG_CRYPTO_NOT_SUPPORTED:
			return "SEC_E_STRONG_CRYPTO_NOT_SUPPORTED";

		case SEC_E_TOO_MANY_PRINCIPALS:
			return "SEC_E_TOO_MANY_PRINCIPALS";

		case SEC_E_NO_PA_DATA:
			return "SEC_E_NO_PA_DATA";

		case SEC_E_PKINIT_NAME_MISMATCH:
			return "SEC_E_PKINIT_NAME_MISMATCH";

		case SEC_E_SMARTCARD_LOGON_REQUIRED:
			return "SEC_E_SMARTCARD_LOGON_REQUIRED";

		case SEC_E_SHUTDOWN_IN_PROGRESS:
			return "SEC_E_SHUTDOWN_IN_PROGRESS";

		case SEC_E_KDC_INVALID_REQUEST:
			return "SEC_E_KDC_INVALID_REQUEST";

		case SEC_E_KDC_UNABLE_TO_REFER:
			return "SEC_E_KDC_UNABLE_TO_REFER";

		case SEC_E_KDC_UNKNOWN_ETYPE:
			return "SEC_E_KDC_UNKNOWN_ETYPE";

		case SEC_E_UNSUPPORTED_PREAUTH:
			return "SEC_E_UNSUPPORTED_PREAUTH";

		case SEC_E_DELEGATION_REQUIRED:
			return "SEC_E_DELEGATION_REQUIRED";

		case SEC_E_BAD_BINDINGS:
			return "SEC_E_BAD_BINDINGS";

		case SEC_E_MULTIPLE_ACCOUNTS:
			return "SEC_E_MULTIPLE_ACCOUNTS";

		case SEC_E_NO_KERB_KEY:
			return "SEC_E_NO_KERB_KEY";

		case SEC_E_CERT_WRONG_USAGE:
			return "SEC_E_CERT_WRONG_USAGE";

		case SEC_E_DOWNGRADE_DETECTED:
			return "SEC_E_DOWNGRADE_DETECTED";

		case SEC_E_SMARTCARD_CERT_REVOKED:
			return "SEC_E_SMARTCARD_CERT_REVOKED";

		case SEC_E_ISSUING_CA_UNTRUSTED:
			return "SEC_E_ISSUING_CA_UNTRUSTED";

		case SEC_E_REVOCATION_OFFLINE_C:
			return "SEC_E_REVOCATION_OFFLINE_C";

		case SEC_E_PKINIT_CLIENT_FAILURE:
			return "SEC_E_PKINIT_CLIENT_FAILURE";

		case SEC_E_SMARTCARD_CERT_EXPIRED:
			return "SEC_E_SMARTCARD_CERT_EXPIRED";

		case SEC_E_NO_S4U_PROT_SUPPORT:
			return "SEC_E_NO_S4U_PROT_SUPPORT";

		case SEC_E_CROSSREALM_DELEGATION_FAILURE:
			return "SEC_E_CROSSREALM_DELEGATION_FAILURE";

		case SEC_E_REVOCATION_OFFLINE_KDC:
			return "SEC_E_REVOCATION_OFFLINE_KDC";

		case SEC_E_ISSUING_CA_UNTRUSTED_KDC:
			return "SEC_E_ISSUING_CA_UNTRUSTED_KDC";

		case SEC_E_KDC_CERT_EXPIRED:
			return "SEC_E_KDC_CERT_EXPIRED";

		case SEC_E_KDC_CERT_REVOKED:
			return "SEC_E_KDC_CERT_REVOKED";

		case SEC_E_INVALID_PARAMETER:
			return "SEC_E_INVALID_PARAMETER";

		case SEC_E_DELEGATION_POLICY:
			return "SEC_E_DELEGATION_POLICY";

		case SEC_E_POLICY_NLTM_ONLY:
			return "SEC_E_POLICY_NLTM_ONLY";

		case SEC_E_NO_CONTEXT:
			return "SEC_E_NO_CONTEXT";

		case SEC_E_PKU2U_CERT_FAILURE:
			return "SEC_E_PKU2U_CERT_FAILURE";

		case SEC_E_MUTUAL_AUTH_FAILED:
			return "SEC_E_MUTUAL_AUTH_FAILED";

		case SEC_I_CONTINUE_NEEDED:
			return "SEC_I_CONTINUE_NEEDED";

		case SEC_I_COMPLETE_NEEDED:
			return "SEC_I_COMPLETE_NEEDED";

		case SEC_I_COMPLETE_AND_CONTINUE:
			return "SEC_I_COMPLETE_AND_CONTINUE";

		case SEC_I_LOCAL_LOGON:
			return "SEC_I_LOCAL_LOGON";

		case SEC_I_CONTEXT_EXPIRED:
			return "SEC_I_CONTEXT_EXPIRED";

		case SEC_I_INCOMPLETE_CREDENTIALS:
			return "SEC_I_INCOMPLETE_CREDENTIALS";

		case SEC_I_RENEGOTIATE:
			return "SEC_I_RENEGOTIATE";

		case SEC_I_NO_LSA_CONTEXT:
			return "SEC_I_NO_LSA_CONTEXT";

		case SEC_I_SIGNATURE_NEEDED:
			return "SEC_I_SIGNATURE_NEEDED";

		case SEC_I_NO_RENEGOTIATION:
			return "SEC_I_NO_RENEGOTIATION";
	}

	return "SEC_E_UNKNOWN";
}

BOOL IsSecurityStatusError(SECURITY_STATUS status)
{
	BOOL error = TRUE;

	switch (status)
	{
		case SEC_E_OK:
		case SEC_I_CONTINUE_NEEDED:
		case SEC_I_COMPLETE_NEEDED:
		case SEC_I_COMPLETE_AND_CONTINUE:
		case SEC_I_LOCAL_LOGON:
		case SEC_I_CONTEXT_EXPIRED:
		case SEC_I_INCOMPLETE_CREDENTIALS:
		case SEC_I_RENEGOTIATE:
		case SEC_I_NO_LSA_CONTEXT:
		case SEC_I_SIGNATURE_NEEDED:
		case SEC_I_NO_RENEGOTIATION:
			error = FALSE;
			break;
	}

	return error;
}

SecurityFunctionTableW* SEC_ENTRY InitSecurityInterfaceExW(DWORD flags)
{
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, &flags, NULL);
	WLog_Print(g_Log, WLOG_DEBUG, "InitSecurityInterfaceExW");
	return g_SspiW;
}

SecurityFunctionTableA* SEC_ENTRY InitSecurityInterfaceExA(DWORD flags)
{
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, &flags, NULL);
	WLog_Print(g_Log, WLOG_DEBUG, "InitSecurityInterfaceExA");
	return g_SspiA;
}

/**
 * Standard SSPI API
 */

/* Package Management */

SECURITY_STATUS SEC_ENTRY sspi_EnumerateSecurityPackagesW(ULONG* pcPackages,
        PSecPkgInfoW* ppPackageInfo)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiW && g_SspiW->EnumerateSecurityPackagesW))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->EnumerateSecurityPackagesW(pcPackages, ppPackageInfo);
	WLog_Print(g_Log, WLOG_DEBUG, "EnumerateSecurityPackagesW: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_EnumerateSecurityPackagesA(ULONG* pcPackages,
        PSecPkgInfoA* ppPackageInfo)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiA && g_SspiA->EnumerateSecurityPackagesA))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiA->EnumerateSecurityPackagesA(pcPackages, ppPackageInfo);
	WLog_Print(g_Log, WLOG_DEBUG, "EnumerateSecurityPackagesA: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SecurityFunctionTableW* SEC_ENTRY sspi_InitSecurityInterfaceW(void)
{
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);
	WLog_Print(g_Log, WLOG_DEBUG, "InitSecurityInterfaceW");
	return g_SspiW;
}

SecurityFunctionTableA* SEC_ENTRY sspi_InitSecurityInterfaceA(void)
{
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);
	WLog_Print(g_Log, WLOG_DEBUG, "InitSecurityInterfaceA");
	return g_SspiA;
}

SECURITY_STATUS SEC_ENTRY sspi_QuerySecurityPackageInfoW(SEC_WCHAR* pszPackageName,
        PSecPkgInfoW* ppPackageInfo)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiW && g_SspiW->QuerySecurityPackageInfoW))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->QuerySecurityPackageInfoW(pszPackageName, ppPackageInfo);
	WLog_Print(g_Log, WLOG_DEBUG, "QuerySecurityPackageInfoW: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_QuerySecurityPackageInfoA(SEC_CHAR* pszPackageName,
        PSecPkgInfoA* ppPackageInfo)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiA && g_SspiA->QuerySecurityPackageInfoA))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiA->QuerySecurityPackageInfoA(pszPackageName, ppPackageInfo);
	WLog_Print(g_Log, WLOG_DEBUG, "QuerySecurityPackageInfoA: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

/* Credential Management */

SECURITY_STATUS SEC_ENTRY sspi_AcquireCredentialsHandleW(SEC_WCHAR* pszPrincipal,
        SEC_WCHAR* pszPackage,
        ULONG fCredentialUse, void* pvLogonID, void* pAuthData, SEC_GET_KEY_FN pGetKeyFn,
        void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiW && g_SspiW->AcquireCredentialsHandleW))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->AcquireCredentialsHandleW(pszPrincipal, pszPackage, fCredentialUse,
	         pvLogonID, pAuthData, pGetKeyFn, pvGetKeyArgument, phCredential, ptsExpiry);
	WLog_Print(g_Log, WLOG_DEBUG, "AcquireCredentialsHandleW: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_AcquireCredentialsHandleA(SEC_CHAR* pszPrincipal,
        SEC_CHAR* pszPackage,
        ULONG fCredentialUse, void* pvLogonID, void* pAuthData, SEC_GET_KEY_FN pGetKeyFn,
        void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiA && g_SspiA->AcquireCredentialsHandleA))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiA->AcquireCredentialsHandleA(pszPrincipal, pszPackage, fCredentialUse,
	         pvLogonID, pAuthData, pGetKeyFn, pvGetKeyArgument, phCredential, ptsExpiry);
	WLog_Print(g_Log, WLOG_DEBUG, "AcquireCredentialsHandleA: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_ExportSecurityContext(PCtxtHandle phContext, ULONG fFlags,
        PSecBuffer pPackedContext, HANDLE* pToken)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiW && g_SspiW->ExportSecurityContext))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->ExportSecurityContext(phContext, fFlags, pPackedContext, pToken);
	WLog_Print(g_Log, WLOG_DEBUG, "ExportSecurityContext: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_FreeCredentialsHandle(PCredHandle phCredential)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiW && g_SspiW->FreeCredentialsHandle))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->FreeCredentialsHandle(phCredential);
	WLog_Print(g_Log, WLOG_DEBUG, "FreeCredentialsHandle: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_ImportSecurityContextW(SEC_WCHAR* pszPackage,
        PSecBuffer pPackedContext, HANDLE pToken, PCtxtHandle phContext)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiW && g_SspiW->ImportSecurityContextW))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->ImportSecurityContextW(pszPackage, pPackedContext, pToken, phContext);
	WLog_Print(g_Log, WLOG_DEBUG, "ImportSecurityContextW: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_ImportSecurityContextA(SEC_CHAR* pszPackage,
        PSecBuffer pPackedContext, HANDLE pToken, PCtxtHandle phContext)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiA && g_SspiA->ImportSecurityContextA))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiA->ImportSecurityContextA(pszPackage, pPackedContext, pToken, phContext);
	WLog_Print(g_Log, WLOG_DEBUG, "ImportSecurityContextA: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_QueryCredentialsAttributesW(PCredHandle phCredential,
        ULONG ulAttribute, void* pBuffer)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiW && g_SspiW->QueryCredentialsAttributesW))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->QueryCredentialsAttributesW(phCredential, ulAttribute, pBuffer);
	WLog_Print(g_Log, WLOG_DEBUG, "QueryCredentialsAttributesW: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_QueryCredentialsAttributesA(PCredHandle phCredential,
        ULONG ulAttribute, void* pBuffer)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiA && g_SspiA->QueryCredentialsAttributesA))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiA->QueryCredentialsAttributesA(phCredential, ulAttribute, pBuffer);
	WLog_Print(g_Log, WLOG_DEBUG, "QueryCredentialsAttributesA: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

/* Context Management */

SECURITY_STATUS SEC_ENTRY sspi_AcceptSecurityContext(PCredHandle phCredential,
        PCtxtHandle phContext,
        PSecBufferDesc pInput, ULONG fContextReq, ULONG TargetDataRep, PCtxtHandle phNewContext,
        PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsTimeStamp)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiW && g_SspiW->AcceptSecurityContext))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->AcceptSecurityContext(phCredential, phContext, pInput, fContextReq,
	                                        TargetDataRep, phNewContext, pOutput, pfContextAttr, ptsTimeStamp);
	WLog_Print(g_Log, WLOG_DEBUG, "AcceptSecurityContext: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_ApplyControlToken(PCtxtHandle phContext, PSecBufferDesc pInput)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiW && g_SspiW->ApplyControlToken))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->ApplyControlToken(phContext, pInput);
	WLog_Print(g_Log, WLOG_DEBUG, "ApplyControlToken: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_CompleteAuthToken(PCtxtHandle phContext, PSecBufferDesc pToken)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiW && g_SspiW->CompleteAuthToken))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->CompleteAuthToken(phContext, pToken);
	WLog_Print(g_Log, WLOG_DEBUG, "CompleteAuthToken: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_DeleteSecurityContext(PCtxtHandle phContext)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiW && g_SspiW->DeleteSecurityContext))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->DeleteSecurityContext(phContext);
	WLog_Print(g_Log, WLOG_DEBUG, "DeleteSecurityContext: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_FreeContextBuffer(void* pvContextBuffer)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiW && g_SspiW->FreeContextBuffer))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->FreeContextBuffer(pvContextBuffer);
	WLog_Print(g_Log, WLOG_DEBUG, "FreeContextBuffer: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_ImpersonateSecurityContext(PCtxtHandle phContext)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiW && g_SspiW->ImpersonateSecurityContext))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->ImpersonateSecurityContext(phContext);
	WLog_Print(g_Log, WLOG_DEBUG, "ImpersonateSecurityContext: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_InitializeSecurityContextW(PCredHandle phCredential,
        PCtxtHandle phContext,
        SEC_WCHAR* pszTargetName, ULONG fContextReq, ULONG Reserved1, ULONG TargetDataRep,
        PSecBufferDesc pInput, ULONG Reserved2, PCtxtHandle phNewContext,
        PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiW && g_SspiW->InitializeSecurityContextW))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->InitializeSecurityContextW(phCredential, phContext,
	         pszTargetName, fContextReq, Reserved1, TargetDataRep, pInput,
	         Reserved2, phNewContext, pOutput, pfContextAttr, ptsExpiry);
	WLog_Print(g_Log, WLOG_DEBUG, "InitializeSecurityContextW: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_InitializeSecurityContextA(PCredHandle phCredential,
        PCtxtHandle phContext,
        SEC_CHAR* pszTargetName, ULONG fContextReq, ULONG Reserved1, ULONG TargetDataRep,
        PSecBufferDesc pInput, ULONG Reserved2, PCtxtHandle phNewContext,
        PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiA && g_SspiA->InitializeSecurityContextA))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiA->InitializeSecurityContextA(phCredential, phContext,
	         pszTargetName, fContextReq, Reserved1, TargetDataRep, pInput,
	         Reserved2, phNewContext, pOutput, pfContextAttr, ptsExpiry);
	WLog_Print(g_Log, WLOG_DEBUG, "InitializeSecurityContextA: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_QueryContextAttributesW(PCtxtHandle phContext, ULONG ulAttribute,
        void* pBuffer)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiW && g_SspiW->QueryContextAttributesW))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->QueryContextAttributesW(phContext, ulAttribute, pBuffer);
	WLog_Print(g_Log, WLOG_DEBUG, "QueryContextAttributesW: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_QueryContextAttributesA(PCtxtHandle phContext, ULONG ulAttribute,
        void* pBuffer)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiA && g_SspiA->QueryContextAttributesA))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiA->QueryContextAttributesA(phContext, ulAttribute, pBuffer);
	WLog_Print(g_Log, WLOG_DEBUG, "QueryContextAttributesA: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_QuerySecurityContextToken(PCtxtHandle phContext, HANDLE* phToken)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiW && g_SspiW->QuerySecurityContextToken))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->QuerySecurityContextToken(phContext, phToken);
	WLog_Print(g_Log, WLOG_DEBUG, "QuerySecurityContextToken: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_SetContextAttributesW(PCtxtHandle phContext, ULONG ulAttribute,
        void* pBuffer, ULONG cbBuffer)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiW && g_SspiW->SetContextAttributesW))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->SetContextAttributesW(phContext, ulAttribute, pBuffer, cbBuffer);
	WLog_Print(g_Log, WLOG_DEBUG, "SetContextAttributesW: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_SetContextAttributesA(PCtxtHandle phContext, ULONG ulAttribute,
        void* pBuffer, ULONG cbBuffer)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiA && g_SspiA->SetContextAttributesA))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiA->SetContextAttributesA(phContext, ulAttribute, pBuffer, cbBuffer);
	WLog_Print(g_Log, WLOG_DEBUG, "SetContextAttributesA: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_RevertSecurityContext(PCtxtHandle phContext)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiW && g_SspiW->RevertSecurityContext))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->RevertSecurityContext(phContext);
	WLog_Print(g_Log, WLOG_DEBUG, "RevertSecurityContext: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

/* Message Support */

SECURITY_STATUS SEC_ENTRY sspi_DecryptMessage(PCtxtHandle phContext, PSecBufferDesc pMessage,
        ULONG MessageSeqNo, PULONG pfQOP)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiW && g_SspiW->DecryptMessage))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->DecryptMessage(phContext, pMessage, MessageSeqNo, pfQOP);
	WLog_Print(g_Log, WLOG_DEBUG, "DecryptMessage: %s (0x%08"PRIX32")", GetSecurityStatusString(status),
	           status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_EncryptMessage(PCtxtHandle phContext, ULONG fQOP,
        PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiW && g_SspiW->EncryptMessage))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->EncryptMessage(phContext, fQOP, pMessage, MessageSeqNo);
	WLog_Print(g_Log, WLOG_DEBUG, "EncryptMessage: %s (0x%08"PRIX32")", GetSecurityStatusString(status),
	           status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_MakeSignature(PCtxtHandle phContext, ULONG fQOP,
        PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiW && g_SspiW->MakeSignature))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->MakeSignature(phContext, fQOP, pMessage, MessageSeqNo);
	WLog_Print(g_Log, WLOG_DEBUG, "MakeSignature: %s (0x%08"PRIX32")", GetSecurityStatusString(status),
	           status);
	return status;
}

SECURITY_STATUS SEC_ENTRY sspi_VerifySignature(PCtxtHandle phContext, PSecBufferDesc pMessage,
        ULONG MessageSeqNo, PULONG pfQOP)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, InitializeSspiModuleInt, NULL, NULL);

	if (!(g_SspiW && g_SspiW->VerifySignature))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_SspiW->VerifySignature(phContext, pMessage, MessageSeqNo, pfQOP);
	WLog_Print(g_Log, WLOG_DEBUG, "VerifySignature: %s (0x%08"PRIX32")",
	           GetSecurityStatusString(status), status);
	return status;
}

SecurityFunctionTableA sspi_SecurityFunctionTableA =
{
	1, /* dwVersion */
	sspi_EnumerateSecurityPackagesA, /* EnumerateSecurityPackages */
	sspi_QueryCredentialsAttributesA, /* QueryCredentialsAttributes */
	sspi_AcquireCredentialsHandleA, /* AcquireCredentialsHandle */
	sspi_FreeCredentialsHandle, /* FreeCredentialsHandle */
	NULL, /* Reserved2 */
	sspi_InitializeSecurityContextA, /* InitializeSecurityContext */
	sspi_AcceptSecurityContext, /* AcceptSecurityContext */
	sspi_CompleteAuthToken, /* CompleteAuthToken */
	sspi_DeleteSecurityContext, /* DeleteSecurityContext */
	sspi_ApplyControlToken, /* ApplyControlToken */
	sspi_QueryContextAttributesA, /* QueryContextAttributes */
	sspi_ImpersonateSecurityContext, /* ImpersonateSecurityContext */
	sspi_RevertSecurityContext, /* RevertSecurityContext */
	sspi_MakeSignature, /* MakeSignature */
	sspi_VerifySignature, /* VerifySignature */
	sspi_FreeContextBuffer, /* FreeContextBuffer */
	sspi_QuerySecurityPackageInfoA, /* QuerySecurityPackageInfo */
	NULL, /* Reserved3 */
	NULL, /* Reserved4 */
	sspi_ExportSecurityContext, /* ExportSecurityContext */
	sspi_ImportSecurityContextA, /* ImportSecurityContext */
	NULL, /* AddCredentials */
	NULL, /* Reserved8 */
	sspi_QuerySecurityContextToken, /* QuerySecurityContextToken */
	sspi_EncryptMessage, /* EncryptMessage */
	sspi_DecryptMessage, /* DecryptMessage */
	sspi_SetContextAttributesA, /* SetContextAttributes */
};

SecurityFunctionTableW sspi_SecurityFunctionTableW =
{
	1, /* dwVersion */
	sspi_EnumerateSecurityPackagesW, /* EnumerateSecurityPackages */
	sspi_QueryCredentialsAttributesW, /* QueryCredentialsAttributes */
	sspi_AcquireCredentialsHandleW, /* AcquireCredentialsHandle */
	sspi_FreeCredentialsHandle, /* FreeCredentialsHandle */
	NULL, /* Reserved2 */
	sspi_InitializeSecurityContextW, /* InitializeSecurityContext */
	sspi_AcceptSecurityContext, /* AcceptSecurityContext */
	sspi_CompleteAuthToken, /* CompleteAuthToken */
	sspi_DeleteSecurityContext, /* DeleteSecurityContext */
	sspi_ApplyControlToken, /* ApplyControlToken */
	sspi_QueryContextAttributesW, /* QueryContextAttributes */
	sspi_ImpersonateSecurityContext, /* ImpersonateSecurityContext */
	sspi_RevertSecurityContext, /* RevertSecurityContext */
	sspi_MakeSignature, /* MakeSignature */
	sspi_VerifySignature, /* VerifySignature */
	sspi_FreeContextBuffer, /* FreeContextBuffer */
	sspi_QuerySecurityPackageInfoW, /* QuerySecurityPackageInfo */
	NULL, /* Reserved3 */
	NULL, /* Reserved4 */
	sspi_ExportSecurityContext, /* ExportSecurityContext */
	sspi_ImportSecurityContextW, /* ImportSecurityContext */
	NULL, /* AddCredentials */
	NULL, /* Reserved8 */
	sspi_QuerySecurityContextToken, /* QuerySecurityContextToken */
	sspi_EncryptMessage, /* EncryptMessage */
	sspi_DecryptMessage, /* DecryptMessage */
	sspi_SetContextAttributesW, /* SetContextAttributes */
};

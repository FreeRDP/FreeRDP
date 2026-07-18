/**
 * WinPR: Windows Portable Runtime
 * NTLM Security Package
 *
 * Copyright 2011-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/sspi.h>
#include <winpr/print.h>
#include <winpr/string.h>
#include <winpr/tchar.h>
#include <winpr/sysinfo.h>
#include <winpr/registry.h>
#include <winpr/endian.h>
#include <winpr/build-config.h>

#include "ntlm.h"
#include "ntlm_export.h"
#include "../sspi.h"

#include "ntlm_message.h"

#include "../../utils.h"

#include "../../log.h"
#define TAG WINPR_TAG("sspi.NTLM")

#ifndef MIN
#define MIN(a, b) ((a) < (b)) ? (a) : (b)
#endif

#define WINPR_KEY "Software\\%s\\WinPR\\NTLM"

static char* NTLM_PACKAGE_NAME = "NTLM";

#define check_context(ctx) check_context_((ctx), __FILE__, __func__, __LINE__)

WINPR_ATTR_NODISCARD
static BOOL check_context_(NTLM_CONTEXT* context, const char* file, const char* fkt, size_t line)
{
	BOOL rc = TRUE;
	wLog* log = WLog_Get(TAG);
	const DWORD log_level = WLOG_ERROR;

	if (!context)
	{
		if (WLog_IsLevelActive(log, log_level))
			WLog_PrintTextMessage(log, log_level, line, file, fkt, "invalid context");

		return FALSE;
	}

	if (!context->RecvRc4Seal)
	{
		if (WLog_IsLevelActive(log, log_level))
			WLog_PrintTextMessage(log, log_level, line, file, fkt, "invalid context->RecvRc4Seal");
		rc = FALSE;
	}
	if (!context->SendRc4Seal)
	{
		if (WLog_IsLevelActive(log, log_level))
			WLog_PrintTextMessage(log, log_level, line, file, fkt, "invalid context->SendRc4Seal");
		rc = FALSE;
	}

	if (!context->SendSigningKey)
	{
		if (WLog_IsLevelActive(log, log_level))
			WLog_PrintTextMessage(log, log_level, line, file, fkt,
			                      "invalid context->SendSigningKey");
		rc = FALSE;
	}
	if (!context->RecvSigningKey)
	{
		if (WLog_IsLevelActive(log, log_level))
			WLog_PrintTextMessage(log, log_level, line, file, fkt,
			                      "invalid context->RecvSigningKey");
		rc = FALSE;
	}
	if (!context->SendSealingKey)
	{
		if (WLog_IsLevelActive(log, log_level))
			WLog_PrintTextMessage(log, log_level, line, file, fkt,
			                      "invalid context->SendSealingKey");
		rc = FALSE;
	}
	if (!context->RecvSealingKey)
	{
		if (WLog_IsLevelActive(log, log_level))
			WLog_PrintTextMessage(log, log_level, line, file, fkt,
			                      "invalid context->RecvSealingKey");
		rc = FALSE;
	}
	return rc;
}

WINPR_ATTR_MALLOC(free, 1)
static char* get_computer_name(COMPUTER_NAME_FORMAT type, size_t* pSize)
{
	DWORD nSize = 0;

	if (pSize)
		*pSize = 0;

	if (GetComputerNameExA(type, nullptr, &nSize))
		return nullptr;

	if (GetLastError() != ERROR_MORE_DATA)
		return nullptr;

	char* computerName = calloc(1, nSize);

	if (!computerName)
		return nullptr;

	if (!GetComputerNameExA(type, computerName, &nSize))
	{
		free(computerName);
		return nullptr;
	}

	if (pSize)
		*pSize = nSize;
	return computerName;
}

WINPR_ATTR_NODISCARD
SECURITY_STATUS ntlm_SetContextWorkstationX(NTLM_CONTEXT* context, BOOL unicode, const void* data,
                                            size_t length)
{
	WINPR_ASSERT(context);
	ntlm_free_unicode_string(&context->Workstation);

	if (length == 0)
		return SEC_E_OK;

	WINPR_ASSERT(data);
	if (unicode)
		context->Workstation = ntlm_from_unicode_string_w(data, length / sizeof(WCHAR));
	else
		context->Workstation = ntlm_from_unicode_string_utf8(data, length);

	if (ntlm_is_unicode_string_empty(&context->Workstation))
		return SEC_E_INSUFFICIENT_MEMORY;

	return SEC_E_OK;
}

WINPR_ATTR_NODISCARD
static int ntlm_SetContextWorkstation(NTLM_CONTEXT* context, const char* Workstation)
{
	const char* ws = Workstation;
	CHAR* computerName = nullptr;

	if (!Workstation)
	{
		computerName = get_computer_name(ComputerNameNetBIOS, nullptr);
		if (!computerName)
			return -1;
		ws = computerName;
	}

	const size_t len = strlen(ws);
	const SECURITY_STATUS status = ntlm_SetContextWorkstationX(context, FALSE, ws, len);
	free(computerName);

	return (status == SEC_E_OK) ? 1 : -1;
}

WINPR_ATTR_NODISCARD
static int ntlm_SetContextServicePrincipalNameW(NTLM_CONTEXT* context, LPWSTR ServicePrincipalName)
{
	WINPR_ASSERT(context);

	ntlm_free_unicode_string(&context->ServicePrincipalName);
	if (!ServicePrincipalName)
		return 1;

	const size_t len = _wcslen(ServicePrincipalName);
	context->ServicePrincipalName = ntlm_from_unicode_string_w(ServicePrincipalName, len);
	if (ntlm_is_unicode_string_empty(&context->ServicePrincipalName))
		return -1;

	return 1;
}

WINPR_ATTR_NODISCARD
static int ntlm_SetContextTargetName(NTLM_CONTEXT* context, char* TargetName)
{
	char* name = TargetName;
	WINPR_ASSERT(context);

	if (!name)
	{
		size_t nSize = 0;
		char* computerName = get_computer_name(ComputerNameNetBIOS, &nSize);

		if (!computerName)
			return -1;

		if (nSize > MAX_COMPUTERNAME_LENGTH)
			computerName[MAX_COMPUTERNAME_LENGTH] = '\0';

		name = computerName;

		if (!name)
			return -1;

		CharUpperA(name);
	}

	size_t len = 0;
	sspi_SecBufferFree(&context->TargetName);
	context->TargetName.pvBuffer = ConvertUtf8ToWCharAlloc(name, &len);

	if (!context->TargetName.pvBuffer || (len > UINT16_MAX / sizeof(WCHAR)))
	{
		free(context->TargetName.pvBuffer);
		context->TargetName.pvBuffer = nullptr;

		if (!TargetName)
			free(name);

		return -1;
	}

	context->TargetName.cbBuffer = (USHORT)(len * sizeof(WCHAR));

	if (!TargetName)
		free(name);

	return 1;
}

static void ntlm_ContextFree(NTLM_CONTEXT* context)
{
	if (!context)
		return;

	winpr_RC4_Free(context->SendRc4Seal);
	winpr_RC4_Free(context->RecvRc4Seal);
	sspi_SecBufferFree(&context->NegotiateMessage);
	sspi_SecBufferFree(&context->ChallengeMessage);
	sspi_SecBufferFree(&context->AuthenticateMessage);
	sspi_SecBufferFree(&context->ChallengeTargetInfo);
	sspi_SecBufferFree(&context->AuthenticateTargetInfo);
	sspi_SecBufferFree(&context->TargetName);
	sspi_SecBufferFree(&context->NtChallengeResponse);
	sspi_SecBufferFree(&context->LmChallengeResponse);
	ntlm_free_unicode_string(&context->ServicePrincipalName);
	ntlm_free_unicode_string(&context->Workstation);
	ntlm_free_unicode_string(&context->NbComputerName);
	ntlm_free_unicode_string(&context->NbDomainName);
	ntlm_free_unicode_string(&context->DnsComputerName);
	ntlm_free_unicode_string(&context->DnsDomainName);

	ntlm_free_messages(context);

	/* Zero sensitive key material before freeing the context */
	memset(context->NtlmHash, 0, sizeof(context->NtlmHash));
	memset(context->NtlmV2Hash, 0, sizeof(context->NtlmV2Hash));
	memset(context->SessionBaseKey, 0, sizeof(context->SessionBaseKey));
	memset(context->KeyExchangeKey, 0, sizeof(context->KeyExchangeKey));
	memset(context->RandomSessionKey, 0, sizeof(context->RandomSessionKey));
	memset(context->ExportedSessionKey, 0, sizeof(context->ExportedSessionKey));
	memset(context->EncryptedRandomSessionKey, 0, sizeof(context->EncryptedRandomSessionKey));
	memset(context->NtProofString, 0, sizeof(context->NtProofString));
	free(context);
}

WINPR_ATTR_NODISCARD
static int ntlm_get_target_computer_name(PUNICODE_STRING pName,
                                         WINPR_ATTR_UNUSED COMPUTER_NAME_FORMAT type)
{
	WINPR_ASSERT(pName);
	ntlm_free_unicode_string(pName);

	size_t len = 0;
	char* name = get_computer_name(ComputerNameNetBIOS, &len);
	if (!name)
		return -1;

	CharUpperA(name);

	*pName = ntlm_from_unicode_string_utf8(name, len);
	free(name);

	return !ntlm_is_unicode_string_empty(pName);
}

WINPR_ATTR_NODISCARD
static BOOL ntlm_ContextFillDefaultNames(NTLM_CONTEXT* context)
{
	WINPR_ASSERT(context);

	if (ntlm_SetContextWorkstation(context, nullptr) < 0)
		return FALSE;

	if (ntlm_get_target_computer_name(&context->NbDomainName, ComputerNameNetBIOS) < 0)
		return FALSE;

	if (ntlm_get_target_computer_name(&context->NbComputerName, ComputerNameNetBIOS) < 0)
		return FALSE;

	if (ntlm_get_target_computer_name(&context->DnsDomainName, ComputerNameDnsDomain) < 0)
		return FALSE;

	if (ntlm_get_target_computer_name(&context->DnsComputerName, ComputerNameDnsHostname) < 0)
		return FALSE;
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL ntlm_try_set_from_registry(HKEY hKey, const char* key, UNICODE_STRING* ustr)
{
	WINPR_ASSERT(hKey);
	WINPR_ASSERT(key);

	UNICODE_STRING str = WINPR_C_ARRAY_INIT;

	WCHAR wkey[64] = WINPR_C_ARRAY_INIT;
	const SSIZE_T res = ConvertUtf8ToWChar(key, wkey, ARRAYSIZE(wkey));
	if (res < 0)
		goto fail;
	WINPR_ASSERT((size_t)res < ARRAYSIZE(wkey));

	DWORD dwSize = 0;
	DWORD dwType = 0;
	if (RegQueryValueExW(hKey, wkey, nullptr, &dwType, nullptr, &dwSize) != ERROR_SUCCESS)
		goto fail;

	if ((dwSize > UINT16_MAX) || ((dwSize % 2) != 0))
		goto fail;

	str.Buffer = calloc(dwSize / sizeof(WCHAR) + 1, sizeof(WCHAR));
	if (!str.Buffer)
		goto fail;
	str.Length = WINPR_ASSERTING_INT_CAST(UINT16, dwSize);
	str.MaximumLength = WINPR_ASSERTING_INT_CAST(UINT16, dwSize);

	const LONG rc = RegQueryValueExW(hKey, wkey, nullptr, &dwType, (BYTE*)str.Buffer, &dwSize);
	if (rc != ERROR_SUCCESS)
		goto fail;
	ntlm_free_unicode_string(ustr);
	*ustr = str;
	return TRUE;

fail:
	ntlm_free_unicode_string(&str);
	return FALSE;
}

WINPR_ATTR_NODISCARD
static BOOL ntlm_ContextFromConfig(NTLM_CONTEXT* context)
{
	{
		WINPR_ASSERT(context);

		char* key = winpr_getApplicatonDetailsRegKey(WINPR_KEY);
		if (key)
		{
			HKEY hKey = nullptr;

			const LONG status =
			    RegOpenKeyExA(HKEY_LOCAL_MACHINE, key, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);
			free(key);

			if (status == ERROR_SUCCESS)
			{
				DWORD dwValue = 0;
				DWORD dwSize = 0;
				DWORD dwType = 0;

				if (RegQueryValueEx(hKey, _T("NTLMv2"), nullptr, &dwType, (BYTE*)&dwValue,
				                    &dwSize) == ERROR_SUCCESS)
					context->NTLMv2 = dwValue ? 1 : 0;

				if (RegQueryValueEx(hKey, _T("UseMIC"), nullptr, &dwType, (BYTE*)&dwValue,
				                    &dwSize) == ERROR_SUCCESS)
					context->UseMIC = dwValue ? 1 : 0;

				if (RegQueryValueEx(hKey, _T("SendVersionInfo"), nullptr, &dwType, (BYTE*)&dwValue,
				                    &dwSize) == ERROR_SUCCESS)
					context->SendVersionInfo = dwValue ? 1 : 0;

				if (RegQueryValueEx(hKey, _T("SendSingleHostData"), nullptr, &dwType,
				                    (BYTE*)&dwValue, &dwSize) == ERROR_SUCCESS)
					context->SendSingleHostData = dwValue ? 1 : 0;

				if (RegQueryValueEx(hKey, _T("SendWorkstationName"), nullptr, &dwType,
				                    (BYTE*)&dwValue, &dwSize) == ERROR_SUCCESS)
					context->SendWorkstationName = dwValue ? 1 : 0;

				(void)ntlm_try_set_from_registry(hKey, "WorkstationName", &context->Workstation);
				(void)ntlm_try_set_from_registry(hKey, "NbDomainName", &context->NbDomainName);
				(void)ntlm_try_set_from_registry(hKey, "NbComputerName", &context->NbComputerName);
				(void)ntlm_try_set_from_registry(hKey, "DnsDomainName", &context->DnsDomainName);
				(void)ntlm_try_set_from_registry(hKey, "DnsComputerName",
				                                 &context->DnsComputerName);

				RegCloseKey(hKey);
			}
		}
	}

	HKEY hKey = nullptr;
	const LONG status =
	    RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Control\\LSA"), 0,
	                 KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		DWORD dwType = 0;
		DWORD dwSize = 0;
		DWORD dwValue = 0;
		if (RegQueryValueEx(hKey, _T("SuppressExtendedProtection"), nullptr, &dwType,
		                    (BYTE*)&dwValue, &dwSize) == ERROR_SUCCESS)
			context->SuppressExtendedProtection = dwValue ? 1 : 0;

		RegCloseKey(hKey);
	}

	/*
	 * Extended Protection is enabled by default in Windows 7,
	 * but enabling it in WinPR breaks TS Gateway at this point
	 */
	context->SuppressExtendedProtection = FALSE;
	return TRUE;
}

WINPR_ATTR_MALLOC(ntlm_ContextFree, 1)
static NTLM_CONTEXT* ntlm_ContextNew(void)
{
	NTLM_CONTEXT* context = (NTLM_CONTEXT*)calloc(1, sizeof(NTLM_CONTEXT));

	if (!context)
		return nullptr;

	context->NTLMv2 = TRUE;
	context->UseMIC = FALSE;
	context->SendVersionInfo = TRUE;
	context->SendSingleHostData = FALSE;
	context->SendWorkstationName = TRUE;
	context->NegotiateKeyExchange = TRUE;
	context->UseSamFileDatabase = TRUE;

	context->NegotiateFlags = 0;
	context->LmCompatibilityLevel = 3;
	ntlm_change_state(context, NTLM_STATE_INITIAL);
	FillMemory(context->MachineID, sizeof(context->MachineID), 0xAA);

	if (context->NTLMv2)
		context->UseMIC = TRUE;

	if (!ntlm_ContextFillDefaultNames(context))
		goto fail;
	if (!ntlm_ContextFromConfig(context))
		goto fail;

	return context;

fail:
	ntlm_ContextFree(context);
	return nullptr;
}

WINPR_ATTR_NODISCARD
static SECURITY_STATUS SEC_ENTRY ntlm_AcquireCredentialsHandleW(
    WINPR_ATTR_UNUSED SEC_WCHAR* pszPrincipal, WINPR_ATTR_UNUSED SEC_WCHAR* pszPackage,
    ULONG fCredentialUse, WINPR_ATTR_UNUSED void* pvLogonID, void* pAuthData,
    SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
    WINPR_ATTR_UNUSED PTimeStamp ptsExpiry)
{
	SEC_WINPR_NTLM_SETTINGS* settings = nullptr;

	if ((fCredentialUse != SECPKG_CRED_OUTBOUND) && (fCredentialUse != SECPKG_CRED_INBOUND) &&
	    (fCredentialUse != SECPKG_CRED_BOTH))
	{
		return SEC_E_INVALID_PARAMETER;
	}

	SSPI_CREDENTIALS* credentials = sspi_CredentialsNew();

	if (!credentials)
		return SEC_E_INTERNAL_ERROR;

	credentials->fCredentialUse = fCredentialUse;
	credentials->pGetKeyFn = pGetKeyFn;
	credentials->pvGetKeyArgument = pvGetKeyArgument;

	if (pAuthData)
	{
		UINT32 identityFlags = sspi_GetAuthIdentityFlags(pAuthData);

		if (sspi_CopyAuthIdentity(&(credentials->identity),
		                          (const SEC_WINNT_AUTH_IDENTITY_INFO*)pAuthData) < 0)
		{
			sspi_CredentialsFree(credentials);
			return SEC_E_INVALID_PARAMETER;
		}

		if (identityFlags & SEC_WINNT_AUTH_IDENTITY_EXTENDED)
			settings = (((SEC_WINNT_AUTH_IDENTITY_WINPR*)pAuthData)->ntlmSettings);
	}

	if (settings)
	{
		if (settings->samFile)
		{
			credentials->ntlmSettings.samFile = _strdup(settings->samFile);
			if (!credentials->ntlmSettings.samFile)
			{
				sspi_CredentialsFree(credentials);
				return SEC_E_INSUFFICIENT_MEMORY;
			}
		}
		credentials->ntlmSettings.hashCallback = settings->hashCallback;
		credentials->ntlmSettings.hashCallbackArg = settings->hashCallbackArg;
	}

	sspi_SecureHandleSetLowerPointer(phCredential, (void*)credentials);
	sspi_SecureHandleSetUpperPointer(phCredential, (void*)NTLM_PACKAGE_NAME);
	return SEC_E_OK;
}

WINPR_ATTR_NODISCARD
static SECURITY_STATUS SEC_ENTRY ntlm_AcquireCredentialsHandleA(
    SEC_CHAR* pszPrincipal, SEC_CHAR* pszPackage, ULONG fCredentialUse, void* pvLogonID,
    void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
    PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status = SEC_E_INSUFFICIENT_MEMORY;
	SEC_WCHAR* principal = nullptr;
	SEC_WCHAR* package = nullptr;

	if (pszPrincipal)
	{
		principal = ConvertUtf8ToWCharAlloc(pszPrincipal, nullptr);
		if (!principal)
			goto fail;
	}
	if (pszPackage)
	{
		package = ConvertUtf8ToWCharAlloc(pszPackage, nullptr);
		if (!package)
			goto fail;
	}

	status =
	    ntlm_AcquireCredentialsHandleW(principal, package, fCredentialUse, pvLogonID, pAuthData,
	                                   pGetKeyFn, pvGetKeyArgument, phCredential, ptsExpiry);

fail:
	free(principal);
	free(package);

	return status;
}

WINPR_ATTR_NODISCARD
static SECURITY_STATUS SEC_ENTRY ntlm_FreeCredentialsHandle(PCredHandle phCredential)
{
	if (!phCredential)
		return SEC_E_INVALID_HANDLE;

	SSPI_CREDENTIALS* credentials =
	    (SSPI_CREDENTIALS*)sspi_SecureHandleGetLowerPointer(phCredential);
	sspi_SecureHandleInvalidate(phCredential);
	if (!credentials)
		return SEC_E_INVALID_HANDLE;

	sspi_CredentialsFree(credentials);
	return SEC_E_OK;
}

WINPR_ATTR_NODISCARD
static SECURITY_STATUS SEC_ENTRY ntlm_QueryCredentialsAttributesW(
    WINPR_ATTR_UNUSED PCredHandle phCredential, WINPR_ATTR_UNUSED ULONG ulAttribute,
    WINPR_ATTR_UNUSED void* pBuffer)
{
	if (ulAttribute == SECPKG_CRED_ATTR_NAMES)
	{
		return SEC_E_OK;
	}

	WLog_ERR(TAG, "TODO: Implement");
	return SEC_E_UNSUPPORTED_FUNCTION;
}

WINPR_ATTR_NODISCARD
static SECURITY_STATUS SEC_ENTRY ntlm_QueryCredentialsAttributesA(PCredHandle phCredential,
                                                                  ULONG ulAttribute, void* pBuffer)
{
	return ntlm_QueryCredentialsAttributesW(phCredential, ulAttribute, pBuffer);
}

/**
 * @see http://msdn.microsoft.com/en-us/library/windows/desktop/aa374707
 */
WINPR_ATTR_NODISCARD
static SECURITY_STATUS SEC_ENTRY ntlm_AcceptSecurityContext(
    PCredHandle phCredential, PCtxtHandle phContext, PSecBufferDesc pInput, ULONG fContextReq,
    WINPR_ATTR_UNUSED ULONG TargetDataRep, PCtxtHandle phNewContext, PSecBufferDesc pOutput,
    WINPR_ATTR_UNUSED PULONG pfContextAttr, WINPR_ATTR_UNUSED PTimeStamp ptsTimeStamp)
{
	SECURITY_STATUS status = 0;
	SSPI_CREDENTIALS* credentials = nullptr;
	PSecBuffer input_buffer = nullptr;
	PSecBuffer output_buffer = nullptr;

	/* behave like windows SSPIs that don't want empty context */
	if (phContext && !phContext->dwLower && !phContext->dwUpper)
		return SEC_E_INVALID_HANDLE;

	NTLM_CONTEXT* context = (NTLM_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
	{
		context = ntlm_ContextNew();

		if (!context)
			return SEC_E_INSUFFICIENT_MEMORY;

		context->server = TRUE;

		if (fContextReq & ASC_REQ_CONFIDENTIALITY)
			context->confidentiality = TRUE;

		credentials = (SSPI_CREDENTIALS*)sspi_SecureHandleGetLowerPointer(phCredential);
		context->credentials = credentials;
		context->SamFile = credentials->ntlmSettings.samFile;
		context->HashCallback = credentials->ntlmSettings.hashCallback;
		context->HashCallbackArg = credentials->ntlmSettings.hashCallbackArg;

		if (!ntlm_SetContextTargetName(context, nullptr))
			return SEC_E_INVALID_HANDLE;
		sspi_SecureHandleSetLowerPointer(phNewContext, context);
		sspi_SecureHandleSetUpperPointer(phNewContext, (void*)NTLM_PACKAGE_NAME);
	}

	switch (ntlm_get_state(context))
	{
		case NTLM_STATE_INITIAL:
		{
			ntlm_change_state(context, NTLM_STATE_NEGOTIATE);

			if (!pInput)
				return SEC_E_INVALID_TOKEN;

			if (pInput->cBuffers < 1)
				return SEC_E_INVALID_TOKEN;

			input_buffer = sspi_FindSecBuffer(pInput, SECBUFFER_TOKEN);

			if (!input_buffer)
				return SEC_E_INVALID_TOKEN;

			if (input_buffer->cbBuffer < 1)
				return SEC_E_INVALID_TOKEN;

			status = ntlm_read_NegotiateMessage(context, input_buffer);
			if (status != SEC_I_CONTINUE_NEEDED)
				return status;

			if (ntlm_get_state(context) == NTLM_STATE_CHALLENGE)
			{
				if (!pOutput)
					return SEC_E_INVALID_TOKEN;

				if (pOutput->cBuffers < 1)
					return SEC_E_INVALID_TOKEN;

				output_buffer = sspi_FindSecBuffer(pOutput, SECBUFFER_TOKEN);

				if (!output_buffer->BufferType)
					return SEC_E_INVALID_TOKEN;

				if (output_buffer->cbBuffer < 1)
					return SEC_E_INSUFFICIENT_MEMORY;

				return ntlm_write_ChallengeMessage(context, output_buffer);
			}

			return SEC_E_OUT_OF_SEQUENCE;
		}

		case NTLM_STATE_AUTHENTICATE:
		{
			if (!pInput)
				return SEC_E_INVALID_TOKEN;

			if (pInput->cBuffers < 1)
				return SEC_E_INVALID_TOKEN;

			input_buffer = sspi_FindSecBuffer(pInput, SECBUFFER_TOKEN);

			if (!input_buffer)
				return SEC_E_INVALID_TOKEN;

			if (input_buffer->cbBuffer < 1)
				return SEC_E_INVALID_TOKEN;

			status = ntlm_read_AuthenticateMessage(context, input_buffer);

			if (pOutput)
			{
				for (ULONG i = 0; i < pOutput->cBuffers; i++)
				{
					pOutput->pBuffers[i].cbBuffer = 0;
					pOutput->pBuffers[i].BufferType = SECBUFFER_TOKEN;
				}
			}

			return status;
		}

		default:
			return SEC_E_OUT_OF_SEQUENCE;
	}
}

WINPR_ATTR_NODISCARD
static SECURITY_STATUS SEC_ENTRY
ntlm_ImpersonateSecurityContext(WINPR_ATTR_UNUSED PCtxtHandle phContext)
{
	return SEC_E_OK;
}

WINPR_ATTR_NODISCARD
static SECURITY_STATUS SEC_ENTRY ntlm_InitializeSecurityContextW(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_WCHAR* pszTargetName, ULONG fContextReq,
    WINPR_ATTR_UNUSED ULONG Reserved1, WINPR_ATTR_UNUSED ULONG TargetDataRep, PSecBufferDesc pInput,
    WINPR_ATTR_UNUSED ULONG Reserved2, PCtxtHandle phNewContext, PSecBufferDesc pOutput,
    WINPR_ATTR_UNUSED PULONG pfContextAttr, WINPR_ATTR_UNUSED PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status = 0;
	SSPI_CREDENTIALS* credentials = nullptr;
	PSecBuffer input_buffer = nullptr;
	PSecBuffer output_buffer = nullptr;

	/* behave like windows SSPIs that don't want empty context */
	if (phContext && !phContext->dwLower && !phContext->dwUpper)
		return SEC_E_INVALID_HANDLE;

	NTLM_CONTEXT* context = (NTLM_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (pInput)
	{
		input_buffer = sspi_FindSecBuffer(pInput, SECBUFFER_TOKEN);
	}

	if (!context)
	{
		context = ntlm_ContextNew();

		if (!context)
			return SEC_E_INSUFFICIENT_MEMORY;

		if (fContextReq & ISC_REQ_CONFIDENTIALITY)
			context->confidentiality = TRUE;

		credentials = (SSPI_CREDENTIALS*)sspi_SecureHandleGetLowerPointer(phCredential);
		context->credentials = credentials;

		if (ntlm_SetContextServicePrincipalNameW(context, pszTargetName) < 0)
		{
			ntlm_ContextFree(context);
			return SEC_E_INTERNAL_ERROR;
		}

		sspi_SecureHandleSetLowerPointer(phNewContext, context);
		sspi_SecureHandleSetUpperPointer(phNewContext, NTLM_SSP_NAME);
	}

	if ((!input_buffer) || (ntlm_get_state(context) == NTLM_STATE_AUTHENTICATE))
	{
		if (!pOutput)
			return SEC_E_INVALID_TOKEN;

		if (pOutput->cBuffers < 1)
			return SEC_E_INVALID_TOKEN;

		output_buffer = sspi_FindSecBuffer(pOutput, SECBUFFER_TOKEN);

		if (!output_buffer)
			return SEC_E_INVALID_TOKEN;

		if (output_buffer->cbBuffer < 1)
			return SEC_E_INVALID_TOKEN;

		if (ntlm_get_state(context) == NTLM_STATE_INITIAL)
			ntlm_change_state(context, NTLM_STATE_NEGOTIATE);

		if (ntlm_get_state(context) == NTLM_STATE_NEGOTIATE)
			return ntlm_write_NegotiateMessage(context, output_buffer);

		return SEC_E_OUT_OF_SEQUENCE;
	}
	else
	{
		if (!input_buffer)
			return SEC_E_INVALID_TOKEN;

		if (input_buffer->cbBuffer < 1)
			return SEC_E_INVALID_TOKEN;

		PSecBuffer channel_bindings = sspi_FindSecBuffer(pInput, SECBUFFER_CHANNEL_BINDINGS);

		if (channel_bindings)
		{
			context->Bindings.BindingsLength = channel_bindings->cbBuffer;
			context->Bindings.Bindings = (SEC_CHANNEL_BINDINGS*)channel_bindings->pvBuffer;
		}

		if (ntlm_get_state(context) == NTLM_STATE_CHALLENGE)
		{
			status = ntlm_read_ChallengeMessage(context, input_buffer);

			if (status != SEC_I_CONTINUE_NEEDED)
				return status;

			if (!pOutput)
				return SEC_E_INVALID_TOKEN;

			if (pOutput->cBuffers < 1)
				return SEC_E_INVALID_TOKEN;

			output_buffer = sspi_FindSecBuffer(pOutput, SECBUFFER_TOKEN);

			if (!output_buffer)
				return SEC_E_INVALID_TOKEN;

			if (output_buffer->cbBuffer < 1)
				return SEC_E_INSUFFICIENT_MEMORY;

			if (ntlm_get_state(context) == NTLM_STATE_AUTHENTICATE)
				return ntlm_write_AuthenticateMessage(context, output_buffer);
		}

		return SEC_E_OUT_OF_SEQUENCE;
	}

	return SEC_E_OUT_OF_SEQUENCE;
}

/**
 * @see http://msdn.microsoft.com/en-us/library/windows/desktop/aa375512%28v=vs.85%29.aspx
 */
WINPR_ATTR_NODISCARD
static SECURITY_STATUS SEC_ENTRY ntlm_InitializeSecurityContextA(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_CHAR* pszTargetName, ULONG fContextReq,
    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status = 0;
	SEC_WCHAR* pszTargetNameW = nullptr;

	if (pszTargetName)
	{
		pszTargetNameW = ConvertUtf8ToWCharAlloc(pszTargetName, nullptr);
		if (!pszTargetNameW)
			return SEC_E_INTERNAL_ERROR;
	}

	status = ntlm_InitializeSecurityContextW(phCredential, phContext, pszTargetNameW, fContextReq,
	                                         Reserved1, TargetDataRep, pInput, Reserved2,
	                                         phNewContext, pOutput, pfContextAttr, ptsExpiry);
	free(pszTargetNameW);
	return status;
}

/* http://msdn.microsoft.com/en-us/library/windows/desktop/aa375354 */
WINPR_ATTR_NODISCARD
static SECURITY_STATUS SEC_ENTRY ntlm_DeleteSecurityContext(PCtxtHandle phContext)
{
	NTLM_CONTEXT* context = (NTLM_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);
	sspi_SecureHandleInvalidate(phContext);
	ntlm_ContextFree(context);
	return SEC_E_OK;
}

SECURITY_STATUS ntlm_computeProofValue(NTLM_CONTEXT* ntlm, SecBuffer* ntproof)
{
	BYTE* blob = nullptr;
	SecBuffer* target = nullptr;

	WINPR_ASSERT(ntlm);
	WINPR_ASSERT(ntproof);

	target = &ntlm->ChallengeTargetInfo;

	if (!sspi_SecBufferAlloc(ntproof, 36 + target->cbBuffer))
		return SEC_E_INSUFFICIENT_MEMORY;

	blob = (BYTE*)ntproof->pvBuffer;
	CopyMemory(blob, ntlm->ServerChallenge, 8); /* Server challenge. */
	blob[8] = 1;                                /* Response version. */
	blob[9] = 1; /* Highest response version understood by the client. */
	/* Reserved 6B. */
	CopyMemory(&blob[16], ntlm->Timestamp, 8);       /* Time. */
	CopyMemory(&blob[24], ntlm->ClientChallenge, 8); /* Client challenge. */
	/* Reserved 4B. */
	/* Server name. */
	CopyMemory(&blob[36], target->pvBuffer, target->cbBuffer);
	return SEC_E_OK;
}

SECURITY_STATUS ntlm_computeMicValue(NTLM_CONTEXT* ntlm, SecBuffer* micvalue)
{
	BYTE* blob = nullptr;
	ULONG msgSize = 0;

	WINPR_ASSERT(ntlm);
	WINPR_ASSERT(micvalue);

	msgSize = ntlm->NegotiateMessage.cbBuffer + ntlm->ChallengeMessage.cbBuffer +
	          ntlm->AuthenticateMessage.cbBuffer;

	if (!sspi_SecBufferAlloc(micvalue, msgSize))
		return SEC_E_INSUFFICIENT_MEMORY;

	blob = (BYTE*)micvalue->pvBuffer;
	CopyMemory(blob, ntlm->NegotiateMessage.pvBuffer, ntlm->NegotiateMessage.cbBuffer);
	blob += ntlm->NegotiateMessage.cbBuffer;
	CopyMemory(blob, ntlm->ChallengeMessage.pvBuffer, ntlm->ChallengeMessage.cbBuffer);
	blob += ntlm->ChallengeMessage.cbBuffer;
	CopyMemory(blob, ntlm->AuthenticateMessage.pvBuffer, ntlm->AuthenticateMessage.cbBuffer);
	blob += ntlm->MessageIntegrityCheckOffset;
	ZeroMemory(blob, 16);
	return SEC_E_OK;
}

WINPR_ATTR_NODISCARD
static bool identityToAuthIdentity(const SEC_WINNT_AUTH_IDENTITY* identity,
                                   SecPkgContext_AuthIdentity* pAuthIdentity)
{
	WINPR_ASSERT(identity);

	if (!pAuthIdentity)
		return false;

	const SecPkgContext_AuthIdentity empty = WINPR_C_ARRAY_INIT;
	*pAuthIdentity = empty;

	if ((identity->Flags & SEC_WINNT_AUTH_IDENTITY_UNICODE) != 0)
	{
		if (identity->UserLength > 0)
		{
			if (ConvertWCharNToUtf8(identity->User, identity->UserLength, pAuthIdentity->User,
			                        ARRAYSIZE(pAuthIdentity->User)) <= 0)
				return false;
		}

		if (identity->DomainLength > 0)
		{
			if (ConvertWCharNToUtf8(identity->Domain, identity->DomainLength, pAuthIdentity->Domain,
			                        ARRAYSIZE(pAuthIdentity->Domain)) <= 0)
				return false;
		}
	}
	else if ((identity->Flags & SEC_WINNT_AUTH_IDENTITY_ANSI) != 0)
	{
		if (identity->UserLength > 0)
		{
			const size_t len = MIN(ARRAYSIZE(pAuthIdentity->User) - 1, identity->UserLength);
			strncpy(pAuthIdentity->User, (char*)identity->User, len);
			pAuthIdentity->User[len] = '\0';
		}

		if (identity->DomainLength > 0)
		{
			const size_t len = MIN(ARRAYSIZE(pAuthIdentity->Domain) - 1, identity->DomainLength);
			strncpy(pAuthIdentity->Domain, (char*)identity->Domain, len);
			pAuthIdentity->Domain[len] = '\0';
		}
	}
	else
		return false;
	return true;
}

WINPR_ATTR_NODISCARD
static SECURITY_STATUS SEC_ENTRY ntlm_QueryContextAttributesCommon(PCtxtHandle phContext,
                                                                   ULONG ulAttribute, void* pBuffer)
{
	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	NTLM_CONTEXT* context = (NTLM_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);
	if (!check_context(context))
		return SEC_E_INVALID_HANDLE;

	switch (ulAttribute)
	{
		case SECPKG_ATTR_AUTH_IDENTITY:
		{
			SecPkgContext_AuthIdentity* AuthIdentity = (SecPkgContext_AuthIdentity*)pBuffer;
			SSPI_CREDENTIALS* credentials = context->credentials;
			if (!credentials)
				return SEC_E_INTERNAL_ERROR;
			if (!identityToAuthIdentity(&credentials->identity, AuthIdentity))
				return SEC_E_INTERNAL_ERROR;
			context->UseSamFileDatabase = FALSE;
			return SEC_E_OK;
		}
		case SECPKG_ATTR_SIZES:
		{
			SecPkgContext_Sizes* ContextSizes = (SecPkgContext_Sizes*)pBuffer;
			ContextSizes->cbMaxToken = 2010;
			ContextSizes->cbMaxSignature = 16;    /* the size of expected signature is 16 bytes */
			ContextSizes->cbBlockSize = 0;        /* no padding */
			ContextSizes->cbSecurityTrailer = 16; /* no security trailer appended in NTLM
			                            contrary to Kerberos */
			return SEC_E_OK;
		}
		case SECPKG_ATTR_AUTH_NTLM_NTPROOF_VALUE:
			return ntlm_computeProofValue(context, (SecBuffer*)pBuffer);

		case SECPKG_ATTR_AUTH_NTLM_RANDKEY:
		{
			SecBuffer* randkey = (SecBuffer*)pBuffer;

			if (!sspi_SecBufferAlloc(randkey, 16))
				return (SEC_E_INSUFFICIENT_MEMORY);

			CopyMemory(randkey->pvBuffer, context->EncryptedRandomSessionKey, 16);
			return (SEC_E_OK);
		}

		case SECPKG_ATTR_AUTH_NTLM_MIC:
		{
			SecBuffer* mic = (SecBuffer*)pBuffer;
			NTLM_AUTHENTICATE_MESSAGE* message = &context->AUTHENTICATE_MESSAGE;

			if (!sspi_SecBufferAlloc(mic, 16))
				return (SEC_E_INSUFFICIENT_MEMORY);

			CopyMemory(mic->pvBuffer, message->MessageIntegrityCheck, 16);
			return (SEC_E_OK);
		}

		case SECPKG_ATTR_AUTH_NTLM_MIC_VALUE:
			return ntlm_computeMicValue(context, (SecBuffer*)pBuffer);

		default:
			WLog_ERR(TAG, "TODO: Implement ulAttribute=0x%08" PRIx32, ulAttribute);
			return SEC_E_UNSUPPORTED_FUNCTION;
	}
}

/* http://msdn.microsoft.com/en-us/library/windows/desktop/aa379337/ */
WINPR_ATTR_NODISCARD
static SECURITY_STATUS SEC_ENTRY ntlm_QueryContextAttributesW(PCtxtHandle phContext,
                                                              ULONG ulAttribute, void* pBuffer)
{
	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	NTLM_CONTEXT* context = (NTLM_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);
	if (!check_context(context))
		return SEC_E_INVALID_HANDLE;

	switch (ulAttribute)
	{
		case SECPKG_ATTR_AUTH_NTLM_HOSTNAME:
		{
			memcpy(pBuffer, context->Workstation.Buffer, context->Workstation.Length);
			return SEC_E_OK;
		}
		case SECPKG_ATTR_AUTH_NTLM_NB_DOMAIN_NAME:
		{
			memcpy(pBuffer, context->NbDomainName.Buffer, context->NbDomainName.Length);
			return SEC_E_OK;
		}
		case SECPKG_ATTR_AUTH_NTLM_NB_COMPUTER_NAME:
		{
			memcpy(pBuffer, context->NbComputerName.Buffer, context->NbComputerName.Length);
			return SEC_E_OK;
		}
		case SECPKG_ATTR_AUTH_NTLM_DNS_DOMAIN_NAME:
		{
			memcpy(pBuffer, context->DnsDomainName.Buffer, context->DnsDomainName.Length);
			return SEC_E_OK;
		}
		case SECPKG_ATTR_AUTH_NTLM_DNS_COMPUTER_NAME:
		{
			memcpy(pBuffer, context->DnsComputerName.Buffer, context->DnsComputerName.Length);
			return SEC_E_OK;
		}

		case SECPKG_ATTR_PACKAGE_INFO:
		{
			SecPkgContext_PackageInfoW* PackageInfo = (SecPkgContext_PackageInfoW*)pBuffer;
			size_t size = sizeof(SecPkgInfoW);
			SecPkgInfoW* pPackageInfo =
			    (SecPkgInfoW*)sspi_ContextBufferAlloc(QuerySecurityPackageInfoIndex, size);

			if (!pPackageInfo)
				return SEC_E_INSUFFICIENT_MEMORY;

			pPackageInfo->fCapabilities = NTLM_SecPkgInfoW.fCapabilities;
			pPackageInfo->wVersion = NTLM_SecPkgInfoW.wVersion;
			pPackageInfo->wRPCID = NTLM_SecPkgInfoW.wRPCID;
			pPackageInfo->cbMaxToken = NTLM_SecPkgInfoW.cbMaxToken;
			pPackageInfo->Name = _wcsdup(NTLM_SecPkgInfoW.Name);
			pPackageInfo->Comment = _wcsdup(NTLM_SecPkgInfoW.Comment);

			if (!pPackageInfo->Name || !pPackageInfo->Comment)
			{
				sspi_ContextBufferFree(pPackageInfo);
				return SEC_E_INSUFFICIENT_MEMORY;
			}
			PackageInfo->PackageInfo = pPackageInfo;
			return SEC_E_OK;
		}
		default:
			return ntlm_QueryContextAttributesCommon(phContext, ulAttribute, pBuffer);
	}
}

WINPR_ATTR_NODISCARD
static SECURITY_STATUS utf8len(const UNICODE_STRING* str, void* pBuffer)
{
	WINPR_ASSERT(str);
	WINPR_ASSERT(pBuffer);
	ULONG* val = (ULONG*)pBuffer;
	const SSIZE_T rc = ConvertWCharNToUtf8(str->Buffer, str->Length, nullptr, 0);
	if (rc < 0)
		return SEC_E_INVALID_PARAMETER;
	*val = WINPR_ASSERTING_INT_CAST(ULONG, rc);
	return SEC_E_OK;
}

WINPR_ATTR_NODISCARD
static SECURITY_STATUS SEC_ENTRY ntlm_QueryContextAttributesA(PCtxtHandle phContext,
                                                              ULONG ulAttribute, void* pBuffer)
{
	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	NTLM_CONTEXT* context = (NTLM_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	switch (ulAttribute)
	{
		case SECPKG_ATTR_AUTH_NTLM_HOSTNAME_LEN:
			return utf8len(&context->Workstation, pBuffer);
		case SECPKG_ATTR_AUTH_NTLM_NB_DOMAIN_NAME_LEN:
			return utf8len(&context->NbDomainName, pBuffer);
		case SECPKG_ATTR_AUTH_NTLM_NB_COMPUTER_NAME_LEN:
			return utf8len(&context->NbComputerName, pBuffer);
		case SECPKG_ATTR_AUTH_NTLM_DNS_DOMAIN_NAME_LEN:
			return utf8len(&context->DnsDomainName, pBuffer);
		case SECPKG_ATTR_AUTH_NTLM_DNS_COMPUTER_NAME_LEN:
			return utf8len(&context->DnsComputerName, pBuffer);
		case SECPKG_ATTR_AUTH_NTLM_HOSTNAME:
		{
			ConvertWCharNToUtf8(context->Workstation.Buffer, context->Workstation.Length, pBuffer,
			                    context->Workstation.Length);
			return SEC_E_OK;
		}
		case SECPKG_ATTR_AUTH_NTLM_NB_DOMAIN_NAME:
		{
			ConvertWCharNToUtf8(context->NbDomainName.Buffer, context->NbDomainName.Length, pBuffer,
			                    context->NbDomainName.Length);
			return SEC_E_OK;
		}
		case SECPKG_ATTR_AUTH_NTLM_NB_COMPUTER_NAME:
		{
			ConvertWCharNToUtf8(context->NbComputerName.Buffer, context->NbComputerName.Length,
			                    pBuffer, context->NbComputerName.Length);
			return SEC_E_OK;
		}
		case SECPKG_ATTR_AUTH_NTLM_DNS_DOMAIN_NAME:
		{
			ConvertWCharNToUtf8(context->DnsDomainName.Buffer, context->DnsDomainName.Length,
			                    pBuffer, context->DnsDomainName.Length);
			return SEC_E_OK;
		}
		case SECPKG_ATTR_AUTH_NTLM_DNS_COMPUTER_NAME:
		{
			ConvertWCharNToUtf8(context->DnsComputerName.Buffer, context->DnsComputerName.Length,
			                    pBuffer, context->DnsComputerName.Length);
			return SEC_E_OK;
		}
		case SECPKG_ATTR_PACKAGE_INFO:
		{
			SecPkgContext_PackageInfoA* PackageInfo = (SecPkgContext_PackageInfoA*)pBuffer;
			size_t size = sizeof(SecPkgInfoA);
			SecPkgInfoA* pPackageInfo =
			    (SecPkgInfoA*)sspi_ContextBufferAlloc(QuerySecurityPackageInfoIndex, size);

			if (!pPackageInfo)
				return SEC_E_INSUFFICIENT_MEMORY;

			pPackageInfo->fCapabilities = NTLM_SecPkgInfoA.fCapabilities;
			pPackageInfo->wVersion = NTLM_SecPkgInfoA.wVersion;
			pPackageInfo->wRPCID = NTLM_SecPkgInfoA.wRPCID;
			pPackageInfo->cbMaxToken = NTLM_SecPkgInfoA.cbMaxToken;
			pPackageInfo->Name = _strdup(NTLM_SecPkgInfoA.Name);
			pPackageInfo->Comment = _strdup(NTLM_SecPkgInfoA.Comment);

			if (!pPackageInfo->Name || !pPackageInfo->Comment)
			{
				sspi_ContextBufferFree(pPackageInfo);
				return SEC_E_INSUFFICIENT_MEMORY;
			}
			PackageInfo->PackageInfo = pPackageInfo;
			return SEC_E_OK;
		}

		default:
			return ntlm_QueryContextAttributesCommon(phContext, ulAttribute, pBuffer);
	}
}

WINPR_ATTR_NODISCARD
static SECURITY_STATUS SEC_ENTRY ntlm_SetContextAttributesCommon(PCtxtHandle phContext,
                                                                 ULONG ulAttribute, void* pBuffer,
                                                                 ULONG cbBuffer)
{
	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INVALID_PARAMETER;

	NTLM_CONTEXT* context = (NTLM_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);
	if (!context)
		return SEC_E_INVALID_HANDLE;

	switch (ulAttribute)
	{
		case SECPKG_ATTR_AUTH_NTLM_HASH:
		{
			SecPkgContext_AuthNtlmHash* AuthNtlmHash = (SecPkgContext_AuthNtlmHash*)pBuffer;

			if (cbBuffer < sizeof(SecPkgContext_AuthNtlmHash))
				return SEC_E_INVALID_PARAMETER;

			if (AuthNtlmHash->Version == 1)
				CopyMemory(context->NtlmHash, AuthNtlmHash->NtlmHash, 16);
			else if (AuthNtlmHash->Version == 2)
				CopyMemory(context->NtlmV2Hash, AuthNtlmHash->NtlmHash, 16);

			return SEC_E_OK;
		}

		case SECPKG_ATTR_AUTH_NTLM_MESSAGE:
		{
			SecPkgContext_AuthNtlmMessage* AuthNtlmMessage =
			    (SecPkgContext_AuthNtlmMessage*)pBuffer;

			if (cbBuffer < sizeof(SecPkgContext_AuthNtlmMessage))
				return SEC_E_INVALID_PARAMETER;

			if (AuthNtlmMessage->type == 1)
			{
				if (!ntlm_SecBufferRealloc(&context->NegotiateMessage, AuthNtlmMessage->length))
					return SEC_E_INSUFFICIENT_MEMORY;

				CopyMemory(context->NegotiateMessage.pvBuffer, AuthNtlmMessage->buffer,
				           AuthNtlmMessage->length);
			}
			else if (AuthNtlmMessage->type == 2)
			{
				if (!ntlm_SecBufferRealloc(&context->ChallengeMessage, AuthNtlmMessage->length))
					return SEC_E_INSUFFICIENT_MEMORY;

				CopyMemory(context->ChallengeMessage.pvBuffer, AuthNtlmMessage->buffer,
				           AuthNtlmMessage->length);
			}
			else if (AuthNtlmMessage->type == 3)
			{
				if (!ntlm_SecBufferRealloc(&context->AuthenticateMessage, AuthNtlmMessage->length))
					return SEC_E_INSUFFICIENT_MEMORY;

				CopyMemory(context->AuthenticateMessage.pvBuffer, AuthNtlmMessage->buffer,
				           AuthNtlmMessage->length);
			}

			return SEC_E_OK;
	}

	case SECPKG_ATTR_AUTH_NTLM_TIMESTAMP:
	{
		SecPkgContext_AuthNtlmTimestamp* AuthNtlmTimestamp =
		    (SecPkgContext_AuthNtlmTimestamp*)pBuffer;

		if (cbBuffer < sizeof(SecPkgContext_AuthNtlmTimestamp))
			return SEC_E_INVALID_PARAMETER;

		if (AuthNtlmTimestamp->ChallengeOrResponse)
			CopyMemory(context->ChallengeTimestamp, AuthNtlmTimestamp->Timestamp, 8);
		else
			CopyMemory(context->Timestamp, AuthNtlmTimestamp->Timestamp, 8);

		return SEC_E_OK;
	}

	case SECPKG_ATTR_AUTH_NTLM_CLIENT_CHALLENGE:
	{
		SecPkgContext_AuthNtlmClientChallenge* AuthNtlmClientChallenge =
			(SecPkgContext_AuthNtlmClientChallenge*)pBuffer;

		if (cbBuffer < sizeof(SecPkgContext_AuthNtlmClientChallenge))
			return SEC_E_INVALID_PARAMETER;

		CopyMemory(context->ClientChallenge, AuthNtlmClientChallenge->ClientChallenge, 8);
		return SEC_E_OK;
	}

	case SECPKG_ATTR_AUTH_NTLM_SERVER_CHALLENGE:
	{
		SecPkgContext_AuthNtlmServerChallenge* AuthNtlmServerChallenge =
			(SecPkgContext_AuthNtlmServerChallenge*)pBuffer;

		if (cbBuffer < sizeof(SecPkgContext_AuthNtlmServerChallenge))
			return SEC_E_INVALID_PARAMETER;

		CopyMemory(context->ServerChallenge, AuthNtlmServerChallenge->ServerChallenge, 8);
		return SEC_E_OK;
	}

	default:
		WLog_ERR(TAG, "TODO: Implement ulAttribute=%08" PRIx32, ulAttribute);
		return SEC_E_UNSUPPORTED_FUNCTION;
	}
}

WINPR_ATTR_NODISCARD
static SECURITY_STATUS ntml_setUnicodeStringW(UNICODE_STRING* str, const WCHAR* val, size_t bytelen)
{
	WINPR_ASSERT(str);
	ntlm_free_unicode_string(str);
	*str = ntlm_from_unicode_string_w(val, bytelen / sizeof(WCHAR));
	if (ntlm_is_unicode_string_empty(str))
		return SEC_E_INVALID_PARAMETER;
	return SEC_E_OK;
}

WINPR_ATTR_NODISCARD
static SECURITY_STATUS utf16len(const UNICODE_STRING* str, void* pBuffer)
{
	WINPR_ASSERT(str);
	WINPR_ASSERT(pBuffer);
	ULONG* val = (ULONG*)pBuffer;
	*val = str->Length;
	return SEC_E_OK;
}

WINPR_ATTR_NODISCARD
static SECURITY_STATUS SEC_ENTRY ntlm_SetContextAttributesW(PCtxtHandle phContext,
                                                            ULONG ulAttribute, void* pBuffer,
                                                            ULONG cbBuffer)
{
	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INVALID_PARAMETER;

	NTLM_CONTEXT* context = (NTLM_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);
	if (!context)
		return SEC_E_INVALID_HANDLE;

	switch (ulAttribute)
	{
		case SECPKG_ATTR_AUTH_NTLM_HOSTNAME_LEN:
			return utf16len(&context->Workstation, pBuffer);
		case SECPKG_ATTR_AUTH_NTLM_NB_DOMAIN_NAME_LEN:
			return utf16len(&context->NbDomainName, pBuffer);
		case SECPKG_ATTR_AUTH_NTLM_NB_COMPUTER_NAME_LEN:
			return utf16len(&context->NbComputerName, pBuffer);
		case SECPKG_ATTR_AUTH_NTLM_DNS_DOMAIN_NAME_LEN:
			return utf16len(&context->DnsDomainName, pBuffer);
		case SECPKG_ATTR_AUTH_NTLM_DNS_COMPUTER_NAME_LEN:
			return utf16len(&context->DnsComputerName, pBuffer);
		case SECPKG_ATTR_AUTH_NTLM_HOSTNAME:
			return ntml_setUnicodeStringW(&context->Workstation, pBuffer, cbBuffer);
		case SECPKG_ATTR_AUTH_NTLM_NB_DOMAIN_NAME:
			return ntml_setUnicodeStringW(&context->NbDomainName, pBuffer, cbBuffer);
		case SECPKG_ATTR_AUTH_NTLM_NB_COMPUTER_NAME:
			return ntml_setUnicodeStringW(&context->NbComputerName, pBuffer, cbBuffer);
		case SECPKG_ATTR_AUTH_NTLM_DNS_DOMAIN_NAME:
			return ntml_setUnicodeStringW(&context->DnsDomainName, pBuffer, cbBuffer);
		case SECPKG_ATTR_AUTH_NTLM_DNS_COMPUTER_NAME:
			return ntml_setUnicodeStringW(&context->DnsComputerName, pBuffer, cbBuffer);

		default:
			return ntlm_SetContextAttributesCommon(phContext, ulAttribute, pBuffer, cbBuffer);
	}
}

WINPR_ATTR_NODISCARD
static SECURITY_STATUS ntml_setUnicodeStringA(UNICODE_STRING* str, const char* val, size_t charlen)
{
	WINPR_ASSERT(str);
	ntlm_free_unicode_string(str);
	*str = ntlm_from_unicode_string_utf8(val, charlen);
	if (ntlm_is_unicode_string_empty(str))
		return SEC_E_INVALID_PARAMETER;
	return SEC_E_OK;
}

WINPR_ATTR_NODISCARD
static SECURITY_STATUS SEC_ENTRY ntlm_SetContextAttributesA(PCtxtHandle phContext,
                                                            ULONG ulAttribute, void* pBuffer,
                                                            ULONG cbBuffer)
{
	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INVALID_PARAMETER;

	NTLM_CONTEXT* context = (NTLM_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);
	if (!context)
		return SEC_E_INVALID_HANDLE;

	switch (ulAttribute)
	{
		case SECPKG_ATTR_AUTH_NTLM_HOSTNAME:
			return ntml_setUnicodeStringA(&context->Workstation, pBuffer, cbBuffer);
		case SECPKG_ATTR_AUTH_NTLM_NB_DOMAIN_NAME:
			return ntml_setUnicodeStringA(&context->NbDomainName, pBuffer, cbBuffer);
		case SECPKG_ATTR_AUTH_NTLM_NB_COMPUTER_NAME:
			return ntml_setUnicodeStringA(&context->NbComputerName, pBuffer, cbBuffer);
		case SECPKG_ATTR_AUTH_NTLM_DNS_DOMAIN_NAME:
			return ntml_setUnicodeStringA(&context->DnsDomainName, pBuffer, cbBuffer);
		case SECPKG_ATTR_AUTH_NTLM_DNS_COMPUTER_NAME:
			return ntml_setUnicodeStringA(&context->DnsComputerName, pBuffer, cbBuffer);
		default:
			return ntlm_SetContextAttributesCommon(phContext, ulAttribute, pBuffer, cbBuffer);
	}
}

WINPR_ATTR_NODISCARD
static SECURITY_STATUS SEC_ENTRY ntlm_SetCredentialsAttributesW(
    WINPR_ATTR_UNUSED PCredHandle phCredential, WINPR_ATTR_UNUSED ULONG ulAttribute,
    WINPR_ATTR_UNUSED void* pBuffer, WINPR_ATTR_UNUSED ULONG cbBuffer)
{
	return SEC_E_UNSUPPORTED_FUNCTION;
}

WINPR_ATTR_NODISCARD
static SECURITY_STATUS SEC_ENTRY ntlm_SetCredentialsAttributesA(
    WINPR_ATTR_UNUSED PCredHandle phCredential, WINPR_ATTR_UNUSED ULONG ulAttribute,
    WINPR_ATTR_UNUSED void* pBuffer, WINPR_ATTR_UNUSED ULONG cbBuffer)
{
	return SEC_E_UNSUPPORTED_FUNCTION;
}

WINPR_ATTR_NODISCARD
static SECURITY_STATUS SEC_ENTRY ntlm_RevertSecurityContext(WINPR_ATTR_UNUSED PCtxtHandle phContext)
{
	return SEC_E_OK;
}

WINPR_ATTR_NODISCARD
static SECURITY_STATUS SEC_ENTRY ntlm_EncryptMessage(PCtxtHandle phContext,
                                                     WINPR_ATTR_UNUSED ULONG fQOP,
                                                     PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
	const UINT32 SeqNo = MessageSeqNo;
	UINT32 value = 0;
	BYTE digest[WINPR_MD5_DIGEST_LENGTH] = WINPR_C_ARRAY_INIT;
	BYTE checksum[8] = WINPR_C_ARRAY_INIT;
	ULONG version = 1;
	PSecBuffer data_buffer = nullptr;
	PSecBuffer signature_buffer = nullptr;
	NTLM_CONTEXT* context = (NTLM_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);
	if (!check_context(context))
		return SEC_E_INVALID_HANDLE;

	for (ULONG index = 0; index < pMessage->cBuffers; index++)
	{
		SecBuffer* cur = &pMessage->pBuffers[index];

		if (cur->BufferType & SECBUFFER_DATA)
			data_buffer = cur;
		else if (cur->BufferType & SECBUFFER_TOKEN)
			signature_buffer = cur;
	}

	if (!data_buffer)
		return SEC_E_INVALID_TOKEN;

	if (!signature_buffer)
		return SEC_E_INVALID_TOKEN;

	/* Copy original data buffer */
	ULONG length = data_buffer->cbBuffer;
	void* data = malloc(length);

	if (!data)
		return SEC_E_INSUFFICIENT_MEMORY;

	CopyMemory(data, data_buffer->pvBuffer, length);
	/* Compute the HMAC-MD5 hash of ConcatenationOf(seq_num,data) using the client signing key */
	WINPR_HMAC_CTX* hmac = winpr_HMAC_New();

	BOOL success = FALSE;
	{
		if (!hmac)
			goto hmac_fail;
		if (!winpr_HMAC_Init(hmac, WINPR_MD_MD5, context->SendSigningKey, WINPR_MD5_DIGEST_LENGTH))
			goto hmac_fail;

		winpr_Data_Write_UINT32(&value, SeqNo);

		if (!winpr_HMAC_Update(hmac, (void*)&value, 4))
			goto hmac_fail;
		if (!winpr_HMAC_Update(hmac, data, length))
			goto hmac_fail;
		if (!winpr_HMAC_Final(hmac, digest, WINPR_MD5_DIGEST_LENGTH))
			goto hmac_fail;
	}

	success = TRUE;

hmac_fail:
	winpr_HMAC_Free(hmac);
	if (!success)
	{
		free(data);
		return SEC_E_INSUFFICIENT_MEMORY;
	}

	/* Encrypt message using with RC4, result overwrites original buffer */
	if ((data_buffer->BufferType & SECBUFFER_READONLY) == 0)
	{
		if (context->confidentiality)
		{
			if (!winpr_RC4_Update(context->SendRc4Seal, length, (BYTE*)data,
			                      (BYTE*)data_buffer->pvBuffer))
			{
				free(data);
				return SEC_E_INSUFFICIENT_MEMORY;
			}
		}
		else
			CopyMemory(data_buffer->pvBuffer, data, length);
	}

#ifdef WITH_DEBUG_NTLM
	WLog_DBG(TAG, "Data Buffer (length = %" PRIu32 ")", length);
	winpr_HexDump(TAG, WLOG_DEBUG, data, length);
	WLog_DBG(TAG, "Encrypted Data Buffer (length = %" PRIu32 ")", data_buffer->cbBuffer);
	winpr_HexDump(TAG, WLOG_DEBUG, data_buffer->pvBuffer, data_buffer->cbBuffer);
#endif
	free(data);
	/* RC4-encrypt first 8 bytes of digest */
	if (!winpr_RC4_Update(context->SendRc4Seal, 8, digest, checksum))
		return SEC_E_INSUFFICIENT_MEMORY;
	if ((signature_buffer->BufferType & SECBUFFER_READONLY) == 0)
	{
		BYTE* signature = signature_buffer->pvBuffer;
		/* Concatenate version, ciphertext and sequence number to build signature */
		winpr_Data_Write_UINT32(signature, version);
		CopyMemory(&signature[4], (void*)checksum, 8);
		winpr_Data_Write_UINT32(&signature[12], SeqNo);
	}
	context->SendSeqNum++;
#ifdef WITH_DEBUG_NTLM
	WLog_DBG(TAG, "Signature (length = %" PRIu32 ")", signature_buffer->cbBuffer);
	winpr_HexDump(TAG, WLOG_DEBUG, signature_buffer->pvBuffer, signature_buffer->cbBuffer);
#endif
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY ntlm_DecryptMessage(PCtxtHandle phContext, PSecBufferDesc pMessage,
                                                     ULONG MessageSeqNo,
                                                     WINPR_ATTR_UNUSED PULONG pfQOP)
{
	const UINT32 SeqNo = (UINT32)MessageSeqNo;
	UINT32 value = 0;
	BYTE digest[WINPR_MD5_DIGEST_LENGTH] = WINPR_C_ARRAY_INIT;
	BYTE checksum[8] = WINPR_C_ARRAY_INIT;
	UINT32 version = 1;
	BYTE expected_signature[WINPR_MD5_DIGEST_LENGTH] = WINPR_C_ARRAY_INIT;
	PSecBuffer data_buffer = nullptr;
	PSecBuffer signature_buffer = nullptr;
	NTLM_CONTEXT* context = (NTLM_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);
	if (!check_context(context))
		return SEC_E_INVALID_HANDLE;

	for (ULONG index = 0; index < pMessage->cBuffers; index++)
	{
		if (pMessage->pBuffers[index].BufferType == SECBUFFER_DATA)
			data_buffer = &pMessage->pBuffers[index];
		else if (pMessage->pBuffers[index].BufferType == SECBUFFER_TOKEN)
			signature_buffer = &pMessage->pBuffers[index];
	}

	if (!data_buffer)
		return SEC_E_INVALID_TOKEN;

	if (!signature_buffer)
		return SEC_E_INVALID_TOKEN;

	/* Copy original data buffer */
	const ULONG length = data_buffer->cbBuffer;
	void* data = malloc(length);

	if (!data)
		return SEC_E_INSUFFICIENT_MEMORY;

	CopyMemory(data, data_buffer->pvBuffer, length);

	/* Decrypt message using with RC4, result overwrites original buffer */

	if (context->confidentiality)
	{
		if (!winpr_RC4_Update(context->RecvRc4Seal, length, (BYTE*)data,
		                      (BYTE*)data_buffer->pvBuffer))
		{
			free(data);
			return SEC_E_INSUFFICIENT_MEMORY;
		}
	}
	else
		CopyMemory(data_buffer->pvBuffer, data, length);

	/* Compute the HMAC-MD5 hash of ConcatenationOf(seq_num,data) using the client signing key */
	WINPR_HMAC_CTX* hmac = winpr_HMAC_New();

	BOOL success = FALSE;
	{
		if (!hmac)
			goto hmac_fail;

		if (!winpr_HMAC_Init(hmac, WINPR_MD_MD5, context->RecvSigningKey, WINPR_MD5_DIGEST_LENGTH))
			goto hmac_fail;

		winpr_Data_Write_UINT32(&value, SeqNo);

		if (!winpr_HMAC_Update(hmac, (void*)&value, 4))
			goto hmac_fail;
		if (!winpr_HMAC_Update(hmac, data_buffer->pvBuffer, data_buffer->cbBuffer))
			goto hmac_fail;
		if (!winpr_HMAC_Final(hmac, digest, WINPR_MD5_DIGEST_LENGTH))
			goto hmac_fail;

		success = TRUE;
	}
hmac_fail:
	winpr_HMAC_Free(hmac);
	if (!success)
	{
		free(data);
		return SEC_E_INSUFFICIENT_MEMORY;
	}

#ifdef WITH_DEBUG_NTLM
	WLog_DBG(TAG, "Encrypted Data Buffer (length = %" PRIu32 ")", length);
	winpr_HexDump(TAG, WLOG_DEBUG, data, length);
	WLog_DBG(TAG, "Data Buffer (length = %" PRIu32 ")", data_buffer->cbBuffer);
	winpr_HexDump(TAG, WLOG_DEBUG, data_buffer->pvBuffer, data_buffer->cbBuffer);
#endif
	free(data);
	/* RC4-encrypt first 8 bytes of digest */
	if (!winpr_RC4_Update(context->RecvRc4Seal, 8, digest, checksum))
		return SEC_E_MESSAGE_ALTERED;

	/* Concatenate version, ciphertext and sequence number to build signature */
	winpr_Data_Write_UINT32(expected_signature, version);
	CopyMemory(&expected_signature[4], (void*)checksum, 8);
	winpr_Data_Write_UINT32(&expected_signature[12], SeqNo);
	context->RecvSeqNum++;

	if (memcmp(signature_buffer->pvBuffer, expected_signature, 16) != 0)
	{
		/* signature verification failed! */
		WLog_ERR(TAG, "signature verification failed, something nasty is going on!");
#ifdef WITH_DEBUG_NTLM
		WLog_ERR(TAG, "Expected Signature:");
		winpr_HexDump(TAG, WLOG_ERROR, expected_signature, 16);
		WLog_ERR(TAG, "Actual Signature:");
		winpr_HexDump(TAG, WLOG_ERROR, (BYTE*)signature_buffer->pvBuffer, 16);
#endif
		return SEC_E_MESSAGE_ALTERED;
	}

	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY ntlm_MakeSignature(PCtxtHandle phContext,
                                                    WINPR_ATTR_UNUSED ULONG fQOP,
                                                    PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
	SECURITY_STATUS status = SEC_E_INTERNAL_ERROR;
	PSecBuffer data_buffer = nullptr;
	PSecBuffer sig_buffer = nullptr;
	UINT32 seq_no = 0;
	BYTE digest[WINPR_MD5_DIGEST_LENGTH] = WINPR_C_ARRAY_INIT;
	BYTE checksum[8] = WINPR_C_ARRAY_INIT;

	NTLM_CONTEXT* context = sspi_SecureHandleGetLowerPointer(phContext);
	if (!check_context(context))
		return SEC_E_INVALID_HANDLE;

	for (ULONG i = 0; i < pMessage->cBuffers; i++)
	{
		if (pMessage->pBuffers[i].BufferType == SECBUFFER_DATA)
			data_buffer = &pMessage->pBuffers[i];
		else if (pMessage->pBuffers[i].BufferType == SECBUFFER_TOKEN)
			sig_buffer = &pMessage->pBuffers[i];
	}

	if (!data_buffer || !sig_buffer)
		return SEC_E_INVALID_TOKEN;

	WINPR_HMAC_CTX* hmac = winpr_HMAC_New();

	if (!winpr_HMAC_Init(hmac, WINPR_MD_MD5, context->SendSigningKey, WINPR_MD5_DIGEST_LENGTH))
		goto fail;

	winpr_Data_Write_UINT32(&seq_no, MessageSeqNo);
	if (!winpr_HMAC_Update(hmac, (BYTE*)&seq_no, 4))
		goto fail;
	if (!winpr_HMAC_Update(hmac, data_buffer->pvBuffer, data_buffer->cbBuffer))
		goto fail;
	if (!winpr_HMAC_Final(hmac, digest, WINPR_MD5_DIGEST_LENGTH))
		goto fail;

	if (!winpr_RC4_Update(context->SendRc4Seal, 8, digest, checksum))
		goto fail;

	BYTE* signature = sig_buffer->pvBuffer;
	winpr_Data_Write_UINT32(signature, 1L);
	CopyMemory(&signature[4], checksum, 8);
	winpr_Data_Write_UINT32(&signature[12], seq_no);
	sig_buffer->cbBuffer = 16;

	status = SEC_E_OK;

fail:
	winpr_HMAC_Free(hmac);
	return status;
}

WINPR_ATTR_NODISCARD
static SECURITY_STATUS SEC_ENTRY ntlm_VerifySignature(PCtxtHandle phContext,
                                                      PSecBufferDesc pMessage, ULONG MessageSeqNo,
                                                      WINPR_ATTR_UNUSED PULONG pfQOP)
{
	SECURITY_STATUS status = SEC_E_INTERNAL_ERROR;
	PSecBuffer data_buffer = nullptr;
	PSecBuffer sig_buffer = nullptr;
	UINT32 seq_no = 0;
	BYTE digest[WINPR_MD5_DIGEST_LENGTH] = WINPR_C_ARRAY_INIT;
	BYTE checksum[8] = WINPR_C_ARRAY_INIT;
	BYTE signature[16] = WINPR_C_ARRAY_INIT;

	NTLM_CONTEXT* context = sspi_SecureHandleGetLowerPointer(phContext);
	if (!check_context(context))
		return SEC_E_INVALID_HANDLE;

	for (ULONG i = 0; i < pMessage->cBuffers; i++)
	{
		if (pMessage->pBuffers[i].BufferType == SECBUFFER_DATA)
			data_buffer = &pMessage->pBuffers[i];
		else if (pMessage->pBuffers[i].BufferType == SECBUFFER_TOKEN)
			sig_buffer = &pMessage->pBuffers[i];
	}

	if (!data_buffer || !sig_buffer)
		return SEC_E_INVALID_TOKEN;

	WINPR_HMAC_CTX* hmac = winpr_HMAC_New();

	if (!winpr_HMAC_Init(hmac, WINPR_MD_MD5, context->RecvSigningKey, WINPR_MD5_DIGEST_LENGTH))
		goto fail;

	winpr_Data_Write_UINT32(&seq_no, MessageSeqNo);
	if (!winpr_HMAC_Update(hmac, (BYTE*)&seq_no, 4))
		goto fail;
	if (!winpr_HMAC_Update(hmac, data_buffer->pvBuffer, data_buffer->cbBuffer))
		goto fail;
	if (!winpr_HMAC_Final(hmac, digest, WINPR_MD5_DIGEST_LENGTH))
		goto fail;

	if (!winpr_RC4_Update(context->RecvRc4Seal, 8, digest, checksum))
		goto fail;

	winpr_Data_Write_UINT32(signature, 1L);
	CopyMemory(&signature[4], checksum, 8);
	winpr_Data_Write_UINT32(&signature[12], seq_no);

	status = SEC_E_OK;
	if (memcmp(sig_buffer->pvBuffer, signature, 16) != 0)
		status = SEC_E_MESSAGE_ALTERED;

fail:
	winpr_HMAC_Free(hmac);
	return status;
}

const SecurityFunctionTableA NTLM_SecurityFunctionTableA = {
	3,                                /* dwVersion */
	nullptr,                          /* EnumerateSecurityPackages */
	ntlm_QueryCredentialsAttributesA, /* QueryCredentialsAttributes */
	ntlm_AcquireCredentialsHandleA,   /* AcquireCredentialsHandle */
	ntlm_FreeCredentialsHandle,       /* FreeCredentialsHandle */
	nullptr,                          /* Reserved2 */
	ntlm_InitializeSecurityContextA,  /* InitializeSecurityContext */
	ntlm_AcceptSecurityContext,       /* AcceptSecurityContext */
	nullptr,                          /* CompleteAuthToken */
	ntlm_DeleteSecurityContext,       /* DeleteSecurityContext */
	nullptr,                          /* ApplyControlToken */
	ntlm_QueryContextAttributesA,     /* QueryContextAttributes */
	ntlm_ImpersonateSecurityContext,  /* ImpersonateSecurityContext */
	ntlm_RevertSecurityContext,       /* RevertSecurityContext */
	ntlm_MakeSignature,               /* MakeSignature */
	ntlm_VerifySignature,             /* VerifySignature */
	nullptr,                          /* FreeContextBuffer */
	nullptr,                          /* QuerySecurityPackageInfo */
	nullptr,                          /* Reserved3 */
	nullptr,                          /* Reserved4 */
	nullptr,                          /* ExportSecurityContext */
	nullptr,                          /* ImportSecurityContext */
	nullptr,                          /* AddCredentials */
	nullptr,                          /* Reserved8 */
	nullptr,                          /* QuerySecurityContextToken */
	ntlm_EncryptMessage,              /* EncryptMessage */
	ntlm_DecryptMessage,              /* DecryptMessage */
	ntlm_SetContextAttributesA,       /* SetContextAttributes */
	ntlm_SetCredentialsAttributesA,   /* SetCredentialsAttributes */
};

const SecurityFunctionTableW NTLM_SecurityFunctionTableW = {
	3,                                /* dwVersion */
	nullptr,                          /* EnumerateSecurityPackages */
	ntlm_QueryCredentialsAttributesW, /* QueryCredentialsAttributes */
	ntlm_AcquireCredentialsHandleW,   /* AcquireCredentialsHandle */
	ntlm_FreeCredentialsHandle,       /* FreeCredentialsHandle */
	nullptr,                          /* Reserved2 */
	ntlm_InitializeSecurityContextW,  /* InitializeSecurityContext */
	ntlm_AcceptSecurityContext,       /* AcceptSecurityContext */
	nullptr,                          /* CompleteAuthToken */
	ntlm_DeleteSecurityContext,       /* DeleteSecurityContext */
	nullptr,                          /* ApplyControlToken */
	ntlm_QueryContextAttributesW,     /* QueryContextAttributes */
	ntlm_ImpersonateSecurityContext,  /* ImpersonateSecurityContext */
	ntlm_RevertSecurityContext,       /* RevertSecurityContext */
	ntlm_MakeSignature,               /* MakeSignature */
	ntlm_VerifySignature,             /* VerifySignature */
	nullptr,                          /* FreeContextBuffer */
	nullptr,                          /* QuerySecurityPackageInfo */
	nullptr,                          /* Reserved3 */
	nullptr,                          /* Reserved4 */
	nullptr,                          /* ExportSecurityContext */
	nullptr,                          /* ImportSecurityContext */
	nullptr,                          /* AddCredentials */
	nullptr,                          /* Reserved8 */
	nullptr,                          /* QuerySecurityContextToken */
	ntlm_EncryptMessage,              /* EncryptMessage */
	ntlm_DecryptMessage,              /* DecryptMessage */
	ntlm_SetContextAttributesW,       /* SetContextAttributes */
	ntlm_SetCredentialsAttributesW,   /* SetCredentialsAttributes */
};

const SecPkgInfoA NTLM_SecPkgInfoA = {
	0x00082B37,             /* fCapabilities */
	1,                      /* wVersion */
	0x000A,                 /* wRPCID */
	0x00000B48,             /* cbMaxToken */
	"NTLM",                 /* Name */
	"NTLM Security Package" /* Comment */
};

static WCHAR NTLM_SecPkgInfoW_NameBuffer[32] = WINPR_C_ARRAY_INIT;
static WCHAR NTLM_SecPkgInfoW_CommentBuffer[32] = WINPR_C_ARRAY_INIT;

const SecPkgInfoW NTLM_SecPkgInfoW = {
	0x00082B37,                    /* fCapabilities */
	1,                             /* wVersion */
	0x000A,                        /* wRPCID */
	0x00000B48,                    /* cbMaxToken */
	NTLM_SecPkgInfoW_NameBuffer,   /* Name */
	NTLM_SecPkgInfoW_CommentBuffer /* Comment */
};

char* ntlm_negotiate_flags_string(char* buffer, size_t size, UINT32 flags)
{
	if (!buffer || (size == 0))
		return buffer;

	(void)_snprintf(buffer, size, "[0x%08" PRIx32 "] ", flags);

	for (int x = 0; x < 31; x++)
	{
		const UINT32 mask = 1u << x;
		size_t len = strnlen(buffer, size);
		if (flags & mask)
		{
			const char* str = ntlm_get_negotiate_string(mask);
			const size_t flen = strlen(str);

			if ((len > 0) && (buffer[len - 1] != ' '))
			{
				if (size - len < 1)
					break;
				winpr_str_append("|", buffer, size, nullptr);
				len++;
			}

			if (size - len < flen)
				break;
			winpr_str_append(str, buffer, size, nullptr);
		}
	}

	return buffer;
}

const char* ntlm_message_type_string(UINT32 messageType)
{
	switch (messageType)
	{
		case MESSAGE_TYPE_NEGOTIATE:
			return "MESSAGE_TYPE_NEGOTIATE";
		case MESSAGE_TYPE_CHALLENGE:
			return "MESSAGE_TYPE_CHALLENGE";
		case MESSAGE_TYPE_AUTHENTICATE:
			return "MESSAGE_TYPE_AUTHENTICATE";
		default:
			return "MESSAGE_TYPE_UNKNOWN";
	}
}

const char* ntlm_state_string(NTLM_STATE state)
{
	switch (state)
	{
		case NTLM_STATE_INITIAL:
			return "NTLM_STATE_INITIAL";
		case NTLM_STATE_NEGOTIATE:
			return "NTLM_STATE_NEGOTIATE";
		case NTLM_STATE_CHALLENGE:
			return "NTLM_STATE_CHALLENGE";
		case NTLM_STATE_AUTHENTICATE:
			return "NTLM_STATE_AUTHENTICATE";
		case NTLM_STATE_FINAL:
			return "NTLM_STATE_FINAL";
		default:
			return "NTLM_STATE_UNKNOWN";
	}
}
void ntlm_change_state(NTLM_CONTEXT* ntlm, NTLM_STATE state)
{
	WINPR_ASSERT(ntlm);
	WLog_DBG(TAG, "change state from %s to %s", ntlm_state_string(ntlm->state),
	         ntlm_state_string(state));
	ntlm->state = state;
}

NTLM_STATE ntlm_get_state(NTLM_CONTEXT* ntlm)
{
	WINPR_ASSERT(ntlm);
	return ntlm->state;
}

BOOL ntlm_reset_cipher_state(PSecHandle phContext)
{
	NTLM_CONTEXT* context = sspi_SecureHandleGetLowerPointer(phContext);

	if (context)
	{
		if (!check_context(context))
			return FALSE;

		winpr_RC4_Free(context->SendRc4Seal);
		winpr_RC4_Free(context->RecvRc4Seal);
		context->SendRc4Seal = winpr_RC4_New(context->RecvSealingKey, 16);
		context->RecvRc4Seal = winpr_RC4_New(context->SendSealingKey, 16);

		if (!context->SendRc4Seal)
		{
			WLog_ERR(TAG, "Failed to allocate context->SendRc4Seal");
			return FALSE;
		}
		if (!context->RecvRc4Seal)
		{
			WLog_ERR(TAG, "Failed to allocate context->RecvRc4Seal");
			return FALSE;
		}
	}

	return TRUE;
}

BOOL NTLM_init(void)
{
	InitializeConstWCharFromUtf8(NTLM_SecPkgInfoA.Name, NTLM_SecPkgInfoW_NameBuffer,
	                             ARRAYSIZE(NTLM_SecPkgInfoW_NameBuffer));
	InitializeConstWCharFromUtf8(NTLM_SecPkgInfoA.Comment, NTLM_SecPkgInfoW_CommentBuffer,
	                             ARRAYSIZE(NTLM_SecPkgInfoW_CommentBuffer));

	return TRUE;
}

BOOL ntlm_SecBufferRealloc(SecBuffer* buffer, ULONG len)
{
	sspi_SecBufferFree(buffer);
	return sspi_SecBufferAlloc(buffer, len) != nullptr;
}

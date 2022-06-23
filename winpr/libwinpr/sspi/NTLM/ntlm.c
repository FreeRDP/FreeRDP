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

#include "../../log.h"
#define TAG WINPR_TAG("sspi.NTLM")

#define WINPR_KEY "Software\\" WINPR_VENDOR_STRING "\\" WINPR_PRODUCT_STRING "\\WinPR\\NTLM"

static char* NTLM_PACKAGE_NAME = "NTLM";

static int ntlm_SetContextWorkstation(NTLM_CONTEXT* context, char* Workstation)
{
	int status;
	char* ws = Workstation;
	DWORD nSize = 0;
	CHAR* computerName;

	WINPR_ASSERT(context);

	if (!Workstation)
	{
		if (GetComputerNameExA(ComputerNameNetBIOS, NULL, &nSize) ||
		    GetLastError() != ERROR_MORE_DATA)
			return -1;

		computerName = calloc(nSize, sizeof(CHAR));

		if (!computerName)
			return -1;

		if (!GetComputerNameExA(ComputerNameNetBIOS, computerName, &nSize))
		{
			free(computerName);
			return -1;
		}

		if (nSize > MAX_COMPUTERNAME_LENGTH)
			computerName[MAX_COMPUTERNAME_LENGTH] = '\0';

		ws = computerName;

		if (!ws)
			return -1;
	}

	context->Workstation.Buffer = NULL;
	status = ConvertToUnicode(CP_UTF8, 0, ws, -1, &context->Workstation.Buffer, 0);

	if (!Workstation)
		free(ws);

	if (status <= 0)
		return -1;

	context->Workstation.Length = (USHORT)(status - 1);
	context->Workstation.Length *= 2;
	return 1;
}

static int ntlm_SetContextServicePrincipalNameW(NTLM_CONTEXT* context, LPWSTR ServicePrincipalName)
{
	WINPR_ASSERT(context);

	if (!ServicePrincipalName)
	{
		context->ServicePrincipalName.Buffer = NULL;
		context->ServicePrincipalName.Length = 0;
		return 1;
	}

	context->ServicePrincipalName.Length = (USHORT)(_wcslen(ServicePrincipalName) * 2);
	context->ServicePrincipalName.Buffer = (PWSTR)malloc(context->ServicePrincipalName.Length + 2);

	if (!context->ServicePrincipalName.Buffer)
		return -1;

	CopyMemory(context->ServicePrincipalName.Buffer, ServicePrincipalName,
	           context->ServicePrincipalName.Length + 2);
	return 1;
}

static int ntlm_SetContextTargetName(NTLM_CONTEXT* context, char* TargetName)
{
	int status;
	char* name = TargetName;
	DWORD nSize = 0;
	CHAR* computerName = NULL;

	WINPR_ASSERT(context);

	if (!name)
	{
		if (GetComputerNameExA(ComputerNameNetBIOS, NULL, &nSize) ||
		    GetLastError() != ERROR_MORE_DATA)
			return -1;

		computerName = calloc(nSize, sizeof(CHAR));

		if (!computerName)
			return -1;

		if (!GetComputerNameExA(ComputerNameNetBIOS, computerName, &nSize))
		{
			free(computerName);
			return -1;
		}

		if (nSize > MAX_COMPUTERNAME_LENGTH)
			computerName[MAX_COMPUTERNAME_LENGTH] = '\0';

		name = computerName;

		if (!name)
			return -1;

		CharUpperA(name);
	}

	context->TargetName.pvBuffer = NULL;
	status = ConvertToUnicode(CP_UTF8, 0, name, -1, (LPWSTR*)&context->TargetName.pvBuffer, 0);

	if (status <= 0)
	{
		if (!TargetName)
			free(name);

		return -1;
	}

	context->TargetName.cbBuffer = (USHORT)((status - 1) * 2);

	if (!TargetName)
		free(name);

	return 1;
}

static NTLM_CONTEXT* ntlm_ContextNew(void)
{
	HKEY hKey;
	LONG status;
	DWORD dwType;
	DWORD dwSize;
	DWORD dwValue;
	NTLM_CONTEXT* context;
	context = (NTLM_CONTEXT*)calloc(1, sizeof(NTLM_CONTEXT));

	if (!context)
		return NULL;

	context->NTLMv2 = TRUE;
	context->UseMIC = FALSE;
	context->SendVersionInfo = TRUE;
	context->SendSingleHostData = FALSE;
	context->SendWorkstationName = TRUE;
	context->NegotiateKeyExchange = TRUE;
	context->UseSamFileDatabase = TRUE;
	status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, WINPR_KEY, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		if (RegQueryValueEx(hKey, _T("NTLMv2"), NULL, &dwType, (BYTE*)&dwValue, &dwSize) ==
		    ERROR_SUCCESS)
			context->NTLMv2 = dwValue ? 1 : 0;

		if (RegQueryValueEx(hKey, _T("UseMIC"), NULL, &dwType, (BYTE*)&dwValue, &dwSize) ==
		    ERROR_SUCCESS)
			context->UseMIC = dwValue ? 1 : 0;

		if (RegQueryValueEx(hKey, _T("SendVersionInfo"), NULL, &dwType, (BYTE*)&dwValue, &dwSize) ==
		    ERROR_SUCCESS)
			context->SendVersionInfo = dwValue ? 1 : 0;

		if (RegQueryValueEx(hKey, _T("SendSingleHostData"), NULL, &dwType, (BYTE*)&dwValue,
		                    &dwSize) == ERROR_SUCCESS)
			context->SendSingleHostData = dwValue ? 1 : 0;

		if (RegQueryValueEx(hKey, _T("SendWorkstationName"), NULL, &dwType, (BYTE*)&dwValue,
		                    &dwSize) == ERROR_SUCCESS)
			context->SendWorkstationName = dwValue ? 1 : 0;

		if (RegQueryValueEx(hKey, _T("WorkstationName"), NULL, &dwType, NULL, &dwSize) ==
		    ERROR_SUCCESS)
		{
			char* workstation = (char*)malloc(dwSize + 1);

			if (!workstation)
			{
				free(context);
				return NULL;
			}

			status = RegQueryValueExA(hKey, "WorkstationName", NULL, &dwType, (BYTE*)workstation,
			                          &dwSize);
			if (status != ERROR_SUCCESS)
				WLog_WARN(TAG, "[%s]: Key ''WorkstationName' not found", __FUNCTION__);
			workstation[dwSize] = '\0';

			if (ntlm_SetContextWorkstation(context, workstation) < 0)
			{
				free(workstation);
				free(context);
				return NULL;
			}

			free(workstation);
		}

		RegCloseKey(hKey);
	}

	/*
	 * Extended Protection is enabled by default in Windows 7,
	 * but enabling it in WinPR breaks TS Gateway at this point
	 */
	context->SuppressExtendedProtection = FALSE;
	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Control\\LSA"), 0,
	                      KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		if (RegQueryValueEx(hKey, _T("SuppressExtendedProtection"), NULL, &dwType, (BYTE*)&dwValue,
		                    &dwSize) == ERROR_SUCCESS)
			context->SuppressExtendedProtection = dwValue ? 1 : 0;

		RegCloseKey(hKey);
	}

	context->NegotiateFlags = 0;
	context->LmCompatibilityLevel = 3;
	ntlm_change_state(context, NTLM_STATE_INITIAL);
	FillMemory(context->MachineID, sizeof(context->MachineID), 0xAA);

	if (context->NTLMv2)
		context->UseMIC = TRUE;

	return context;
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
	sspi_SecBufferFree(&context->TargetName);
	sspi_SecBufferFree(&context->NtChallengeResponse);
	sspi_SecBufferFree(&context->LmChallengeResponse);
	free(context->ServicePrincipalName.Buffer);
	free(context->Workstation.Buffer);
	free(context);
}

static SECURITY_STATUS SEC_ENTRY ntlm_AcquireCredentialsHandleW(
    SEC_WCHAR* pszPrincipal, SEC_WCHAR* pszPackage, ULONG fCredentialUse, void* pvLogonID,
    void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
    PTimeStamp ptsExpiry)
{
	SSPI_CREDENTIALS* credentials;
	SEC_WINNT_AUTH_IDENTITY* identity;
	SEC_WINPR_NTLM_SETTINGS* settings = NULL;

	if ((fCredentialUse != SECPKG_CRED_OUTBOUND) && (fCredentialUse != SECPKG_CRED_INBOUND) &&
	    (fCredentialUse != SECPKG_CRED_BOTH))
	{
		return SEC_E_INVALID_PARAMETER;
	}

	credentials = sspi_CredentialsNew();

	if (!credentials)
		return SEC_E_INTERNAL_ERROR;

	credentials->fCredentialUse = fCredentialUse;
	credentials->pGetKeyFn = pGetKeyFn;
	credentials->pvGetKeyArgument = pvGetKeyArgument;

	identity = (SEC_WINNT_AUTH_IDENTITY*)pAuthData;

	if (identity)
	{
		sspi_CopyAuthIdentity(&(credentials->identity), identity);
		if (identity->Flags & SEC_WINNT_AUTH_IDENTITY_EXTENDED)
			settings = &((SEC_WINNT_AUTH_IDENTITY_WINPR*)pAuthData)->ntlmSettings;
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

static SECURITY_STATUS SEC_ENTRY ntlm_AcquireCredentialsHandleA(
    SEC_CHAR* pszPrincipal, SEC_CHAR* pszPackage, ULONG fCredentialUse, void* pvLogonID,
    void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
    PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	SEC_WCHAR* principal = NULL;
	SEC_WCHAR* package = NULL;

	if (pszPrincipal)
		ConvertToUnicode(CP_UTF8, 0, pszPrincipal, -1, &principal, 0);
	if (pszPackage)
		ConvertToUnicode(CP_UTF8, 0, pszPackage, -1, &package, 0);

	status =
	    ntlm_AcquireCredentialsHandleW(principal, package, fCredentialUse, pvLogonID, pAuthData,
	                                   pGetKeyFn, pvGetKeyArgument, phCredential, ptsExpiry);

	if (principal)
		free(principal);
	if (package)
		free(package);

	return status;
}

static SECURITY_STATUS SEC_ENTRY ntlm_FreeCredentialsHandle(PCredHandle phCredential)
{
	SSPI_CREDENTIALS* credentials;

	if (!phCredential)
		return SEC_E_INVALID_HANDLE;

	credentials = (SSPI_CREDENTIALS*)sspi_SecureHandleGetLowerPointer(phCredential);

	if (!credentials)
		return SEC_E_INVALID_HANDLE;

	sspi_CredentialsFree(credentials);
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY ntlm_QueryCredentialsAttributesW(PCredHandle phCredential,
                                                                  ULONG ulAttribute, void* pBuffer)
{
	if (ulAttribute == SECPKG_CRED_ATTR_NAMES)
	{
		return SEC_E_OK;
	}

	WLog_ERR(TAG, "[%s]: TODO: Implement", __FUNCTION__);
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY ntlm_QueryCredentialsAttributesA(PCredHandle phCredential,
                                                                  ULONG ulAttribute, void* pBuffer)
{
	return ntlm_QueryCredentialsAttributesW(phCredential, ulAttribute, pBuffer);
}

/**
 * @see http://msdn.microsoft.com/en-us/library/windows/desktop/aa374707
 */
static SECURITY_STATUS SEC_ENTRY
ntlm_AcceptSecurityContext(PCredHandle phCredential, PCtxtHandle phContext, PSecBufferDesc pInput,
                           ULONG fContextReq, ULONG TargetDataRep, PCtxtHandle phNewContext,
                           PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsTimeStamp)
{
	NTLM_CONTEXT* context;
	SECURITY_STATUS status;
	SSPI_CREDENTIALS* credentials;
	PSecBuffer input_buffer;
	PSecBuffer output_buffer;
	context = (NTLM_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

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

		ntlm_SetContextTargetName(context, NULL);
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
				ULONG i;

				for (i = 0; i < pOutput->cBuffers; i++)
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

static SECURITY_STATUS SEC_ENTRY ntlm_ImpersonateSecurityContext(PCtxtHandle phContext)
{
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY ntlm_InitializeSecurityContextW(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_WCHAR* pszTargetName, ULONG fContextReq,
    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	NTLM_CONTEXT* context;
	SECURITY_STATUS status;
	SSPI_CREDENTIALS* credentials;
	PSecBuffer input_buffer = NULL;
	PSecBuffer output_buffer = NULL;
	PSecBuffer channel_bindings = NULL;
	context = (NTLM_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
	{
		context = ntlm_ContextNew();

		if (!context)
			return SEC_E_INSUFFICIENT_MEMORY;

		if (fContextReq & ISC_REQ_CONFIDENTIALITY)
			context->confidentiality = TRUE;

		credentials = (SSPI_CREDENTIALS*)sspi_SecureHandleGetLowerPointer(phCredential);
		context->credentials = credentials;

		if (context->Workstation.Length < 1)
		{
			if (ntlm_SetContextWorkstation(context, NULL) < 0)
			{
				ntlm_ContextFree(context);
				return SEC_E_INTERNAL_ERROR;
			}
		}

		if (ntlm_SetContextServicePrincipalNameW(context, pszTargetName) < 0)
		{
			ntlm_ContextFree(context);
			return SEC_E_INTERNAL_ERROR;
		}

		sspi_SecureHandleSetLowerPointer(phNewContext, context);
		sspi_SecureHandleSetUpperPointer(phNewContext, NTLM_SSP_NAME);
	}

	if ((!pInput) || (ntlm_get_state(context) == NTLM_STATE_AUTHENTICATE))
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
		if (pInput->cBuffers < 1)
			return SEC_E_INVALID_TOKEN;

		input_buffer = sspi_FindSecBuffer(pInput, SECBUFFER_TOKEN);

		if (!input_buffer)
			return SEC_E_INVALID_TOKEN;

		if (input_buffer->cbBuffer < 1)
			return SEC_E_INVALID_TOKEN;

		channel_bindings = sspi_FindSecBuffer(pInput, SECBUFFER_CHANNEL_BINDINGS);

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
static SECURITY_STATUS SEC_ENTRY ntlm_InitializeSecurityContextA(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_CHAR* pszTargetName, ULONG fContextReq,
    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	SEC_WCHAR* pszTargetNameW = NULL;

	if (pszTargetName)
	{
		if (ConvertToUnicode(CP_UTF8, 0, pszTargetName, -1, &pszTargetNameW, 0) <= 0)
			return SEC_E_INTERNAL_ERROR;
	}

	status = ntlm_InitializeSecurityContextW(phCredential, phContext, pszTargetNameW, fContextReq,
	                                         Reserved1, TargetDataRep, pInput, Reserved2,
	                                         phNewContext, pOutput, pfContextAttr, ptsExpiry);
	free(pszTargetNameW);
	return status;
}

/* http://msdn.microsoft.com/en-us/library/windows/desktop/aa375354 */

static SECURITY_STATUS SEC_ENTRY ntlm_DeleteSecurityContext(PCtxtHandle phContext)
{
	NTLM_CONTEXT* context;
	context = (NTLM_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
		return SEC_E_INVALID_HANDLE;

	ntlm_ContextFree(context);
	return SEC_E_OK;
}

SECURITY_STATUS ntlm_computeProofValue(NTLM_CONTEXT* ntlm, SecBuffer* ntproof)
{
	BYTE* blob;
	SecBuffer* target;

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
	BYTE* blob;
	ULONG msgSize;

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

/* http://msdn.microsoft.com/en-us/library/windows/desktop/aa379337/ */

static SECURITY_STATUS SEC_ENTRY ntlm_QueryContextAttributesW(PCtxtHandle phContext,
                                                              ULONG ulAttribute, void* pBuffer)
{
	NTLM_CONTEXT* context;

	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	context = (NTLM_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (ulAttribute == SECPKG_ATTR_SIZES)
	{
		SecPkgContext_Sizes* ContextSizes = (SecPkgContext_Sizes*)pBuffer;
		ContextSizes->cbMaxToken = 2010;
		ContextSizes->cbMaxSignature = 16;    /* the size of expected signature is 16 bytes */
		ContextSizes->cbBlockSize = 0;        /* no padding */
		ContextSizes->cbSecurityTrailer = 16; /* no security trailer appended in NTLM
		                            contrary to Kerberos */
		return SEC_E_OK;
	}
	else if (ulAttribute == SECPKG_ATTR_AUTH_IDENTITY)
	{
		int status;
		char* UserA = NULL;
		char* DomainA = NULL;
		SSPI_CREDENTIALS* credentials;
		SecPkgContext_AuthIdentity* AuthIdentity = (SecPkgContext_AuthIdentity*)pBuffer;
		context->UseSamFileDatabase = FALSE;
		credentials = context->credentials;
		ZeroMemory(AuthIdentity, sizeof(SecPkgContext_AuthIdentity));
		UserA = AuthIdentity->User;

		if (credentials->identity.UserLength > 0)
		{
			WINPR_ASSERT(credentials->identity.UserLength <= INT_MAX);
			status =
			    ConvertFromUnicode(CP_UTF8, 0, (WCHAR*)credentials->identity.User,
			                       (int)credentials->identity.UserLength, &UserA, 256, NULL, NULL);

			if (status <= 0)
				return SEC_E_INTERNAL_ERROR;
		}

		DomainA = AuthIdentity->Domain;

		if (credentials->identity.DomainLength > 0)
		{
			WINPR_ASSERT(credentials->identity.DomainLength <= INT_MAX);
			status = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*)credentials->identity.Domain,
			                            (int)credentials->identity.DomainLength, &DomainA, 256,
			                            NULL, NULL);

			if (status <= 0)
				return SEC_E_INTERNAL_ERROR;
		}

		return SEC_E_OK;
	}
	else if (ulAttribute == SECPKG_ATTR_AUTH_NTLM_NTPROOF_VALUE)
	{
		return ntlm_computeProofValue(context, (SecBuffer*)pBuffer);
	}
	else if (ulAttribute == SECPKG_ATTR_AUTH_NTLM_RANDKEY)
	{
		SecBuffer* randkey;
		randkey = (SecBuffer*)pBuffer;

		if (!sspi_SecBufferAlloc(randkey, 16))
			return (SEC_E_INSUFFICIENT_MEMORY);

		CopyMemory(randkey->pvBuffer, context->EncryptedRandomSessionKey, 16);
		return (SEC_E_OK);
	}
	else if (ulAttribute == SECPKG_ATTR_AUTH_NTLM_MIC)
	{
		SecBuffer* mic;
		NTLM_AUTHENTICATE_MESSAGE* message;
		mic = (SecBuffer*)pBuffer;
		message = &context->AUTHENTICATE_MESSAGE;

		if (!sspi_SecBufferAlloc(mic, 16))
			return (SEC_E_INSUFFICIENT_MEMORY);

		CopyMemory(mic->pvBuffer, message->MessageIntegrityCheck, 16);
		return (SEC_E_OK);
	}
	else if (ulAttribute == SECPKG_ATTR_AUTH_NTLM_MIC_VALUE)
	{
		return ntlm_computeMicValue(context, (SecBuffer*)pBuffer);
	}

	WLog_ERR(TAG, "[%s]: TODO: Implement ulAttribute%08" PRIx32, __FUNCTION__, ulAttribute);
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY ntlm_QueryContextAttributesA(PCtxtHandle phContext,
                                                              ULONG ulAttribute, void* pBuffer)
{
	return ntlm_QueryContextAttributesW(phContext, ulAttribute, pBuffer);
}

static SECURITY_STATUS SEC_ENTRY ntlm_SetContextAttributesW(PCtxtHandle phContext,
                                                            ULONG ulAttribute, void* pBuffer,
                                                            ULONG cbBuffer)
{
	NTLM_CONTEXT* context;

	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INVALID_PARAMETER;

	context = (NTLM_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (ulAttribute == SECPKG_ATTR_AUTH_NTLM_HASH)
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
	else if (ulAttribute == SECPKG_ATTR_AUTH_NTLM_MESSAGE)
	{
		SecPkgContext_AuthNtlmMessage* AuthNtlmMessage = (SecPkgContext_AuthNtlmMessage*)pBuffer;

		if (cbBuffer < sizeof(SecPkgContext_AuthNtlmMessage))
			return SEC_E_INVALID_PARAMETER;

		if (AuthNtlmMessage->type == 1)
		{
			sspi_SecBufferFree(&context->NegotiateMessage);

			if (!sspi_SecBufferAlloc(&context->NegotiateMessage, AuthNtlmMessage->length))
				return SEC_E_INSUFFICIENT_MEMORY;

			CopyMemory(context->NegotiateMessage.pvBuffer, AuthNtlmMessage->buffer,
			           AuthNtlmMessage->length);
		}
		else if (AuthNtlmMessage->type == 2)
		{
			sspi_SecBufferFree(&context->ChallengeMessage);

			if (!sspi_SecBufferAlloc(&context->ChallengeMessage, AuthNtlmMessage->length))
				return SEC_E_INSUFFICIENT_MEMORY;

			CopyMemory(context->ChallengeMessage.pvBuffer, AuthNtlmMessage->buffer,
			           AuthNtlmMessage->length);
		}
		else if (AuthNtlmMessage->type == 3)
		{
			sspi_SecBufferFree(&context->AuthenticateMessage);

			if (!sspi_SecBufferAlloc(&context->AuthenticateMessage, AuthNtlmMessage->length))
				return SEC_E_INSUFFICIENT_MEMORY;

			CopyMemory(context->AuthenticateMessage.pvBuffer, AuthNtlmMessage->buffer,
			           AuthNtlmMessage->length);
		}

		return SEC_E_OK;
	}
	else if (ulAttribute == SECPKG_ATTR_AUTH_NTLM_TIMESTAMP)
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
	else if (ulAttribute == SECPKG_ATTR_AUTH_NTLM_CLIENT_CHALLENGE)
	{
		SecPkgContext_AuthNtlmClientChallenge* AuthNtlmClientChallenge =
		    (SecPkgContext_AuthNtlmClientChallenge*)pBuffer;

		if (cbBuffer < sizeof(SecPkgContext_AuthNtlmClientChallenge))
			return SEC_E_INVALID_PARAMETER;

		CopyMemory(context->ClientChallenge, AuthNtlmClientChallenge->ClientChallenge, 8);
		return SEC_E_OK;
	}
	else if (ulAttribute == SECPKG_ATTR_AUTH_NTLM_SERVER_CHALLENGE)
	{
		SecPkgContext_AuthNtlmServerChallenge* AuthNtlmServerChallenge =
		    (SecPkgContext_AuthNtlmServerChallenge*)pBuffer;

		if (cbBuffer < sizeof(SecPkgContext_AuthNtlmServerChallenge))
			return SEC_E_INVALID_PARAMETER;

		CopyMemory(context->ServerChallenge, AuthNtlmServerChallenge->ServerChallenge, 8);
		return SEC_E_OK;
	}

	WLog_ERR(TAG, "[%s]: TODO: Implement ulAttribute=%08" PRIx32, __FUNCTION__, ulAttribute);
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY ntlm_SetContextAttributesA(PCtxtHandle phContext,
                                                            ULONG ulAttribute, void* pBuffer,
                                                            ULONG cbBuffer)
{
	return ntlm_SetContextAttributesW(phContext, ulAttribute, pBuffer, cbBuffer);
}

static SECURITY_STATUS SEC_ENTRY ntlm_RevertSecurityContext(PCtxtHandle phContext)
{
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY ntlm_EncryptMessage(PCtxtHandle phContext, ULONG fQOP,
                                                     PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
	ULONG index;
	size_t length;
	void* data;
	UINT32 SeqNo;
	UINT32 value;
	BYTE digest[WINPR_MD5_DIGEST_LENGTH] = { 0 };
	BYTE checksum[8] = { 0 };
	BYTE* signature;
	ULONG version = 1;
	WINPR_HMAC_CTX* hmac;
	NTLM_CONTEXT* context;
	PSecBuffer data_buffer = NULL;
	PSecBuffer signature_buffer = NULL;
	SeqNo = MessageSeqNo;
	context = (NTLM_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	for (index = 0; index < pMessage->cBuffers; index++)
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
	length = data_buffer->cbBuffer;
	data = malloc(length);

	if (!data)
		return SEC_E_INSUFFICIENT_MEMORY;

	CopyMemory(data, data_buffer->pvBuffer, length);
	/* Compute the HMAC-MD5 hash of ConcatenationOf(seq_num,data) using the client signing key */
	hmac = winpr_HMAC_New();

	if (hmac &&
	    winpr_HMAC_Init(hmac, WINPR_MD_MD5, context->SendSigningKey, WINPR_MD5_DIGEST_LENGTH))
	{
		Data_Write_UINT32(&value, SeqNo);
		winpr_HMAC_Update(hmac, (void*)&value, 4);
		winpr_HMAC_Update(hmac, (void*)data, length);
		winpr_HMAC_Final(hmac, digest, WINPR_MD5_DIGEST_LENGTH);
		winpr_HMAC_Free(hmac);
	}
	else
	{
		winpr_HMAC_Free(hmac);
		free(data);
		return SEC_E_INSUFFICIENT_MEMORY;
	}

	/* Encrypt message using with RC4, result overwrites original buffer */
	if ((data_buffer->BufferType & SECBUFFER_READONLY) == 0)
	{
		if (context->confidentiality)
			winpr_RC4_Update(context->SendRc4Seal, length, (BYTE*)data,
			                 (BYTE*)data_buffer->pvBuffer);
		else
			CopyMemory(data_buffer->pvBuffer, data, length);
	}

#ifdef WITH_DEBUG_NTLM
	WLog_DBG(TAG, "Data Buffer (length = %d)", length);
	winpr_HexDump(TAG, WLOG_DEBUG, data, length);
	WLog_DBG(TAG, "Encrypted Data Buffer (length = %" PRIu32 ")", data_buffer->cbBuffer);
	winpr_HexDump(TAG, WLOG_DEBUG, data_buffer->pvBuffer, data_buffer->cbBuffer);
#endif
	free(data);
	/* RC4-encrypt first 8 bytes of digest */
	winpr_RC4_Update(context->SendRc4Seal, 8, digest, checksum);
	if ((signature_buffer->BufferType & SECBUFFER_READONLY) == 0)
	{
		signature = (BYTE*)signature_buffer->pvBuffer;
		/* Concatenate version, ciphertext and sequence number to build signature */
		Data_Write_UINT32(signature, version);
		CopyMemory(&signature[4], (void*)checksum, 8);
		Data_Write_UINT32(&signature[12], SeqNo);
	}
	context->SendSeqNum++;
#ifdef WITH_DEBUG_NTLM
	WLog_DBG(TAG, "Signature (length = %" PRIu32 ")", signature_buffer->cbBuffer);
	winpr_HexDump(TAG, WLOG_DEBUG, signature_buffer->pvBuffer, signature_buffer->cbBuffer);
#endif
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY ntlm_DecryptMessage(PCtxtHandle phContext, PSecBufferDesc pMessage,
                                                     ULONG MessageSeqNo, PULONG pfQOP)
{
	ULONG index;
	size_t length;
	void* data;
	UINT32 SeqNo;
	UINT32 value;
	BYTE digest[WINPR_MD5_DIGEST_LENGTH] = { 0 };
	BYTE checksum[8] = { 0 };
	UINT32 version = 1;
	WINPR_HMAC_CTX* hmac;
	NTLM_CONTEXT* context;
	BYTE expected_signature[WINPR_MD5_DIGEST_LENGTH] = { 0 };
	PSecBuffer data_buffer = NULL;
	PSecBuffer signature_buffer = NULL;
	SeqNo = (UINT32)MessageSeqNo;
	context = (NTLM_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	for (index = 0; index < pMessage->cBuffers; index++)
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
	length = data_buffer->cbBuffer;
	data = malloc(length);

	if (!data)
		return SEC_E_INSUFFICIENT_MEMORY;

	CopyMemory(data, data_buffer->pvBuffer, length);

	/* Decrypt message using with RC4, result overwrites original buffer */

	if (context->confidentiality)
		winpr_RC4_Update(context->RecvRc4Seal, length, (BYTE*)data, (BYTE*)data_buffer->pvBuffer);
	else
		CopyMemory(data_buffer->pvBuffer, data, length);

	/* Compute the HMAC-MD5 hash of ConcatenationOf(seq_num,data) using the client signing key */
	hmac = winpr_HMAC_New();

	if (hmac &&
	    winpr_HMAC_Init(hmac, WINPR_MD_MD5, context->RecvSigningKey, WINPR_MD5_DIGEST_LENGTH))
	{
		Data_Write_UINT32(&value, SeqNo);
		winpr_HMAC_Update(hmac, (void*)&value, 4);
		winpr_HMAC_Update(hmac, (void*)data_buffer->pvBuffer, data_buffer->cbBuffer);
		winpr_HMAC_Final(hmac, digest, WINPR_MD5_DIGEST_LENGTH);
		winpr_HMAC_Free(hmac);
	}
	else
	{
		winpr_HMAC_Free(hmac);
		free(data);
		return SEC_E_INSUFFICIENT_MEMORY;
	}

#ifdef WITH_DEBUG_NTLM
	WLog_DBG(TAG, "Encrypted Data Buffer (length = %d)", length);
	winpr_HexDump(TAG, WLOG_DEBUG, data, length);
	WLog_DBG(TAG, "Data Buffer (length = %" PRIu32 ")", data_buffer->cbBuffer);
	winpr_HexDump(TAG, WLOG_DEBUG, data_buffer->pvBuffer, data_buffer->cbBuffer);
#endif
	free(data);
	/* RC4-encrypt first 8 bytes of digest */
	winpr_RC4_Update(context->RecvRc4Seal, 8, digest, checksum);
	/* Concatenate version, ciphertext and sequence number to build signature */
	Data_Write_UINT32(expected_signature, version);
	CopyMemory(&expected_signature[4], (void*)checksum, 8);
	Data_Write_UINT32(&expected_signature[12], SeqNo);
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

static SECURITY_STATUS SEC_ENTRY ntlm_MakeSignature(PCtxtHandle phContext, ULONG fQOP,
                                                    PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
	NTLM_CONTEXT* context;
	PSecBuffer data_buffer = NULL;
	PSecBuffer sig_buffer = NULL;
	WINPR_HMAC_CTX* hmac;
	UINT32 seq_no;
	BYTE digest[WINPR_MD5_DIGEST_LENGTH] = { 0 };
	BYTE checksum[8] = { 0 };
	BYTE* signature;

	context = sspi_SecureHandleGetLowerPointer(phContext);

	for (int i = 0; i < pMessage->cBuffers; i++)
	{
		if (pMessage->pBuffers[i].BufferType == SECBUFFER_DATA)
			data_buffer = &pMessage->pBuffers[i];
		else if (pMessage->pBuffers[i].BufferType == SECBUFFER_TOKEN)
			sig_buffer = &pMessage->pBuffers[i];
	}

	if (!data_buffer || !sig_buffer)
		return SEC_E_INVALID_TOKEN;

	hmac = winpr_HMAC_New();

	if (!winpr_HMAC_Init(hmac, WINPR_MD_MD5, context->SendSigningKey, WINPR_MD5_DIGEST_LENGTH))
		return SEC_E_INTERNAL_ERROR;

	Data_Write_UINT32(&seq_no, MessageSeqNo);
	winpr_HMAC_Update(hmac, (BYTE*)&seq_no, 4);
	winpr_HMAC_Update(hmac, data_buffer->pvBuffer, data_buffer->cbBuffer);
	winpr_HMAC_Final(hmac, digest, WINPR_MD5_DIGEST_LENGTH);
	winpr_HMAC_Free(hmac);

	winpr_RC4_Update(context->SendRc4Seal, 8, digest, checksum);

	signature = sig_buffer->pvBuffer;
	Data_Write_UINT32(signature, 1L);
	CopyMemory(&signature[4], checksum, 8);
	Data_Write_UINT32(&signature[12], seq_no);
	sig_buffer->cbBuffer = 16;

	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY ntlm_VerifySignature(PCtxtHandle phContext,
                                                      PSecBufferDesc pMessage, ULONG MessageSeqNo,
                                                      PULONG pfQOP)
{
	NTLM_CONTEXT* context;
	PSecBuffer data_buffer = NULL;
	PSecBuffer sig_buffer = NULL;
	WINPR_HMAC_CTX* hmac;
	UINT32 seq_no;
	BYTE digest[WINPR_MD5_DIGEST_LENGTH] = { 0 };
	BYTE checksum[8] = { 0 };
	BYTE signature[16] = { 0 };

	context = sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
		return SEC_E_INVALID_HANDLE;

	for (int i = 0; i < pMessage->cBuffers; i++)
	{
		if (pMessage->pBuffers[i].BufferType == SECBUFFER_DATA)
			data_buffer = &pMessage->pBuffers[i];
		else if (pMessage->pBuffers[i].BufferType == SECBUFFER_TOKEN)
			sig_buffer = &pMessage->pBuffers[i];
	}

	if (!data_buffer || !sig_buffer)
		return SEC_E_INVALID_TOKEN;

	hmac = winpr_HMAC_New();

	if (!winpr_HMAC_Init(hmac, WINPR_MD_MD5, context->RecvSigningKey, WINPR_MD5_DIGEST_LENGTH))
		return SEC_E_INTERNAL_ERROR;

	Data_Write_UINT32(&seq_no, MessageSeqNo);
	winpr_HMAC_Update(hmac, (BYTE*)&seq_no, 4);
	winpr_HMAC_Update(hmac, data_buffer->pvBuffer, data_buffer->cbBuffer);
	winpr_HMAC_Final(hmac, digest, WINPR_MD5_DIGEST_LENGTH);
	winpr_HMAC_Free(hmac);

	winpr_RC4_Update(context->RecvRc4Seal, 8, digest, checksum);

	Data_Write_UINT32(signature, 1L);
	CopyMemory(&signature[4], checksum, 8);
	Data_Write_UINT32(&signature[12], seq_no);

	if (memcmp(sig_buffer->pvBuffer, signature, 16) != 0)
		return SEC_E_MESSAGE_ALTERED;

	return SEC_E_OK;
}

const SecurityFunctionTableA NTLM_SecurityFunctionTableA = {
	1,                                /* dwVersion */
	NULL,                             /* EnumerateSecurityPackages */
	ntlm_QueryCredentialsAttributesA, /* QueryCredentialsAttributes */
	ntlm_AcquireCredentialsHandleA,   /* AcquireCredentialsHandle */
	ntlm_FreeCredentialsHandle,       /* FreeCredentialsHandle */
	NULL,                             /* Reserved2 */
	ntlm_InitializeSecurityContextA,  /* InitializeSecurityContext */
	ntlm_AcceptSecurityContext,       /* AcceptSecurityContext */
	NULL,                             /* CompleteAuthToken */
	ntlm_DeleteSecurityContext,       /* DeleteSecurityContext */
	NULL,                             /* ApplyControlToken */
	ntlm_QueryContextAttributesA,     /* QueryContextAttributes */
	ntlm_ImpersonateSecurityContext,  /* ImpersonateSecurityContext */
	ntlm_RevertSecurityContext,       /* RevertSecurityContext */
	ntlm_MakeSignature,               /* MakeSignature */
	ntlm_VerifySignature,             /* VerifySignature */
	NULL,                             /* FreeContextBuffer */
	NULL,                             /* QuerySecurityPackageInfo */
	NULL,                             /* Reserved3 */
	NULL,                             /* Reserved4 */
	NULL,                             /* ExportSecurityContext */
	NULL,                             /* ImportSecurityContext */
	NULL,                             /* AddCredentials */
	NULL,                             /* Reserved8 */
	NULL,                             /* QuerySecurityContextToken */
	ntlm_EncryptMessage,              /* EncryptMessage */
	ntlm_DecryptMessage,              /* DecryptMessage */
	ntlm_SetContextAttributesA,       /* SetContextAttributes */
};

const SecurityFunctionTableW NTLM_SecurityFunctionTableW = {
	1,                                /* dwVersion */
	NULL,                             /* EnumerateSecurityPackages */
	ntlm_QueryCredentialsAttributesW, /* QueryCredentialsAttributes */
	ntlm_AcquireCredentialsHandleW,   /* AcquireCredentialsHandle */
	ntlm_FreeCredentialsHandle,       /* FreeCredentialsHandle */
	NULL,                             /* Reserved2 */
	ntlm_InitializeSecurityContextW,  /* InitializeSecurityContext */
	ntlm_AcceptSecurityContext,       /* AcceptSecurityContext */
	NULL,                             /* CompleteAuthToken */
	ntlm_DeleteSecurityContext,       /* DeleteSecurityContext */
	NULL,                             /* ApplyControlToken */
	ntlm_QueryContextAttributesW,     /* QueryContextAttributes */
	ntlm_ImpersonateSecurityContext,  /* ImpersonateSecurityContext */
	ntlm_RevertSecurityContext,       /* RevertSecurityContext */
	ntlm_MakeSignature,               /* MakeSignature */
	ntlm_VerifySignature,             /* VerifySignature */
	NULL,                             /* FreeContextBuffer */
	NULL,                             /* QuerySecurityPackageInfo */
	NULL,                             /* Reserved3 */
	NULL,                             /* Reserved4 */
	NULL,                             /* ExportSecurityContext */
	NULL,                             /* ImportSecurityContext */
	NULL,                             /* AddCredentials */
	NULL,                             /* Reserved8 */
	NULL,                             /* QuerySecurityContextToken */
	ntlm_EncryptMessage,              /* EncryptMessage */
	ntlm_DecryptMessage,              /* DecryptMessage */
	ntlm_SetContextAttributesA,       /* SetContextAttributes */
};

const SecPkgInfoA NTLM_SecPkgInfoA = {
	0x00082B37,             /* fCapabilities */
	1,                      /* wVersion */
	0x000A,                 /* wRPCID */
	0x00000B48,             /* cbMaxToken */
	"NTLM",                 /* Name */
	"NTLM Security Package" /* Comment */
};

static WCHAR NTLM_SecPkgInfoW_Name[] = { 'N', 'T', 'L', 'M', '\0' };

static WCHAR NTLM_SecPkgInfoW_Comment[] = {
	'N', 'T', 'L', 'M', ' ', 'S', 'e', 'c', 'u', 'r', 'i',
	't', 'y', ' ', 'P', 'a', 'c', 'k', 'a', 'g', 'e', '\0'
};

const SecPkgInfoW NTLM_SecPkgInfoW = {
	0x00082B37,              /* fCapabilities */
	1,                       /* wVersion */
	0x000A,                  /* wRPCID */
	0x00000B48,              /* cbMaxToken */
	NTLM_SecPkgInfoW_Name,   /* Name */
	NTLM_SecPkgInfoW_Comment /* Comment */
};

char* ntlm_negotiate_flags_string(char* buffer, size_t size, UINT32 flags)
{
	int x;
	if (!buffer || (size == 0))
		return buffer;

	_snprintf(buffer, size, "[0x%08" PRIx32 "] ", flags);

	for (x = 0; x < 31; x++)
	{
		const UINT32 mask = 1 << x;
		size_t len = strnlen(buffer, size);
		if (flags & mask)
		{
			const char* str = ntlm_get_negotiate_string(mask);
			const size_t flen = strlen(str);

			if ((len > 0) && (buffer[len - 1] != ' '))
			{
				if (size - len < 1)
					break;
				winpr_str_append("|", buffer, size, NULL);
				len++;
			}

			if (size - len < flen)
				break;
			winpr_str_append(str, buffer, size, NULL);
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

void ntlm_reset_cipher_state(PSecHandle phContext)
{
	NTLM_CONTEXT* context = sspi_SecureHandleGetLowerPointer(phContext);

	if (context)
	{
		winpr_RC4_Free(context->SendRc4Seal);
		winpr_RC4_Free(context->RecvRc4Seal);
		context->SendRc4Seal = winpr_RC4_New(context->RecvSealingKey, 16);
		context->RecvRc4Seal = winpr_RC4_New(context->SendSealingKey, 16);
	}
}

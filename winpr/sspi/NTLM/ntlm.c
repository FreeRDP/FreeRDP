/**
 * WinPR: Windows Portable Runtime
 * NTLM Security Package
 *
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <time.h>
#include <openssl/des.h>
#include <openssl/md4.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/engine.h>

#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/print.h>
#include <winpr/sysinfo.h>

#include "ntlm.h"
#include "../sspi.h"

#include "ntlm_message.h"

char* NTLM_PACKAGE_NAME = "NTLM";

void ntlm_SetContextWorkstation(NTLM_CONTEXT* context, char* Workstation)
{
	DWORD nSize = 0;

	if (!Workstation)
	{
		GetComputerNameExA(ComputerNameNetBIOS, NULL, &nSize);
		Workstation = malloc(nSize);
		GetComputerNameExA(ComputerNameNetBIOS, Workstation, &nSize);
	}

	context->WorkstationLength = strlen(Workstation) * 2;
	context->Workstation = (UINT16*) malloc(context->WorkstationLength);
	MultiByteToWideChar(CP_ACP, 0, Workstation, strlen(Workstation),
			(LPWSTR) context->Workstation, context->WorkstationLength / 2);

	if (nSize > 0)
		free(Workstation);
}

void ntlm_SetContextTargetName(NTLM_CONTEXT* context, char* TargetName)
{
	DWORD nSize = 0;

	if (!TargetName)
	{
		GetComputerNameExA(ComputerNameDnsHostname, NULL, &nSize);
		TargetName = malloc(nSize);
		GetComputerNameExA(ComputerNameDnsHostname, TargetName, &nSize);
		CharUpperA(TargetName);
	}

	context->TargetName.cbBuffer = strlen(TargetName) * 2;
	context->TargetName.pvBuffer = (void*) malloc(context->TargetName.cbBuffer);
	MultiByteToWideChar(CP_ACP, 0, TargetName, strlen(TargetName),
			(LPWSTR) context->TargetName.pvBuffer, context->TargetName.cbBuffer / 2);

	if (nSize > 0)
		free(TargetName);
}

NTLM_CONTEXT* ntlm_ContextNew()
{
	NTLM_CONTEXT* context;

	context = (NTLM_CONTEXT*) malloc(sizeof(NTLM_CONTEXT));
	ZeroMemory(context, sizeof(NTLM_CONTEXT));

	if (context != NULL)
	{
		context->ntlm_v2 = FALSE;
		context->NegotiateFlags = 0;
		context->SendVersionInfo = 0;
		context->LmCompatibilityLevel = 3;
		context->state = NTLM_STATE_INITIAL;
		context->SuppressExtendedProtection = 1;
		context->av_pairs = (AV_PAIRS*) malloc(sizeof(AV_PAIRS));
		ZeroMemory(context->av_pairs, sizeof(AV_PAIRS));
	}

	return context;
}

void ntlm_ContextFree(NTLM_CONTEXT* context)
{
	if (!context)
		return;

	sspi_SecBufferFree(&context->NegotiateMessage);
	sspi_SecBufferFree(&context->ChallengeMessage);
	sspi_SecBufferFree(&context->AuthenticateMessage);
	sspi_SecBufferFree(&context->TargetInfo);
	sspi_SecBufferFree(&context->TargetName);
	sspi_SecBufferFree(&context->NtChallengeResponse);
	sspi_SecBufferFree(&context->LmChallengeResponse);

	free(context->identity.User);
	free(context->identity.Password);
	free(context->identity.Domain);
	free(context->Workstation);
	free(context->av_pairs->Timestamp.value);
	free(context->av_pairs);
	free(context);
}

SECURITY_STATUS SEC_ENTRY ntlm_AcquireCredentialsHandleW(SEC_WCHAR* pszPrincipal, SEC_WCHAR* pszPackage,
		ULONG fCredentialUse, PLUID pvLogonID, void* pAuthData, void* pGetKeyFn,
		void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry)
{
	CREDENTIALS* credentials;
	SEC_WINNT_AUTH_IDENTITY* identity;

	if (fCredentialUse == SECPKG_CRED_OUTBOUND)
	{
		credentials = sspi_CredentialsNew();

		identity = (SEC_WINNT_AUTH_IDENTITY*) pAuthData;
		CopyMemory(&(credentials->identity), identity, sizeof(SEC_WINNT_AUTH_IDENTITY));

		sspi_SecureHandleSetLowerPointer(phCredential, (void*) credentials);
		sspi_SecureHandleSetUpperPointer(phCredential, (void*) NTLM_PACKAGE_NAME);

		return SEC_E_OK;
	}
	else if (fCredentialUse == SECPKG_CRED_INBOUND)
	{
		credentials = sspi_CredentialsNew();

		identity = (SEC_WINNT_AUTH_IDENTITY*) pAuthData;
		CopyMemory(&(credentials->identity), identity, sizeof(SEC_WINNT_AUTH_IDENTITY));

		sspi_SecureHandleSetLowerPointer(phCredential, (void*) credentials);
		sspi_SecureHandleSetUpperPointer(phCredential, (void*) NTLM_PACKAGE_NAME);

		return SEC_E_OK;
	}

	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY ntlm_AcquireCredentialsHandleA(SEC_CHAR* pszPrincipal, SEC_CHAR* pszPackage,
		ULONG fCredentialUse, PLUID pvLogonID, void* pAuthData, void* pGetKeyFn,
		void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry)
{
	CREDENTIALS* credentials;
	SEC_WINNT_AUTH_IDENTITY* identity;

	if (fCredentialUse == SECPKG_CRED_OUTBOUND)
	{
		credentials = sspi_CredentialsNew();

		identity = (SEC_WINNT_AUTH_IDENTITY*) pAuthData;

		if (identity != NULL)
			CopyMemory(&(credentials->identity), identity, sizeof(SEC_WINNT_AUTH_IDENTITY));

		sspi_SecureHandleSetLowerPointer(phCredential, (void*) credentials);
		sspi_SecureHandleSetUpperPointer(phCredential, (void*) NTLM_PACKAGE_NAME);

		return SEC_E_OK;
	}
	else if (fCredentialUse == SECPKG_CRED_INBOUND)
	{
		credentials = sspi_CredentialsNew();

		identity = (SEC_WINNT_AUTH_IDENTITY*) pAuthData;

		if (identity != NULL)
			CopyMemory(&(credentials->identity), identity, sizeof(SEC_WINNT_AUTH_IDENTITY));

		sspi_SecureHandleSetLowerPointer(phCredential, (void*) credentials);
		sspi_SecureHandleSetUpperPointer(phCredential, (void*) NTLM_PACKAGE_NAME);

		return SEC_E_OK;
	}

	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY ntlm_FreeCredentialsHandle(PCredHandle phCredential)
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

SECURITY_STATUS SEC_ENTRY ntlm_QueryCredentialsAttributesW(PCredHandle phCredential, ULONG ulAttribute, void* pBuffer)
{
	if (ulAttribute == SECPKG_CRED_ATTR_NAMES)
	{
		return SEC_E_OK;
	}

	return SEC_E_UNSUPPORTED_FUNCTION;
}

SECURITY_STATUS SEC_ENTRY ntlm_QueryCredentialsAttributesA(PCredHandle phCredential, ULONG ulAttribute, void* pBuffer)
{
	if (ulAttribute == SECPKG_CRED_ATTR_NAMES)
	{
		return SEC_E_OK;
	}

	return SEC_E_UNSUPPORTED_FUNCTION;
}

/**
 * @see http://msdn.microsoft.com/en-us/library/windows/desktop/aa374707
 */
SECURITY_STATUS SEC_ENTRY ntlm_AcceptSecurityContext(PCredHandle phCredential, PCtxtHandle phContext,
		PSecBufferDesc pInput, ULONG fContextReq, ULONG TargetDataRep, PCtxtHandle phNewContext,
		PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsTimeStamp)
{
	NTLM_CONTEXT* context;
	SECURITY_STATUS status;
	CREDENTIALS* credentials;
	PSecBuffer input_buffer;
	PSecBuffer output_buffer;

	context = (NTLM_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
	{
		context = ntlm_ContextNew();

		if (!context)
			return SEC_E_INSUFFICIENT_MEMORY;

		context->server = 1;

		if (fContextReq & ASC_REQ_CONFIDENTIALITY)
			context->confidentiality = 1;

		credentials = (CREDENTIALS*) sspi_SecureHandleGetLowerPointer(phCredential);
		sspi_CopyAuthIdentity(&context->identity, &credentials->identity);

		ntlm_SetContextTargetName(context, NULL);

		sspi_SecureHandleSetLowerPointer(phNewContext, context);
		sspi_SecureHandleSetUpperPointer(phNewContext, (void*) NTLM_PACKAGE_NAME);
	}

	if (context->state == NTLM_STATE_INITIAL)
	{
		context->state = NTLM_STATE_NEGOTIATE;

		if (!pInput)
			return SEC_E_INVALID_TOKEN;

		if (pInput->cBuffers < 1)
			return SEC_E_INVALID_TOKEN;

		input_buffer = &pInput->pBuffers[0];

		if (input_buffer->BufferType != SECBUFFER_TOKEN)
			return SEC_E_INVALID_TOKEN;

		if (input_buffer->cbBuffer < 1)
			return SEC_E_INVALID_TOKEN;

		status = ntlm_read_NegotiateMessage(context, input_buffer);

		if (context->state == NTLM_STATE_CHALLENGE)
		{
			if (!pOutput)
				return SEC_E_INVALID_TOKEN;

			if (pOutput->cBuffers < 1)
				return SEC_E_INVALID_TOKEN;

			output_buffer = &pOutput->pBuffers[0];

			if (output_buffer->BufferType != SECBUFFER_TOKEN)
				return SEC_E_INVALID_TOKEN;

			if (output_buffer->cbBuffer < 1)
				return SEC_E_INSUFFICIENT_MEMORY;

			return ntlm_write_ChallengeMessage(context, output_buffer);
		}

		return SEC_E_OUT_OF_SEQUENCE;
	}
	else if (context->state == NTLM_STATE_AUTHENTICATE)
	{
		if (!pInput)
			return SEC_E_INVALID_TOKEN;

		if (pInput->cBuffers < 1)
			return SEC_E_INVALID_TOKEN;

		input_buffer = &pInput->pBuffers[0];

		if (input_buffer->BufferType != SECBUFFER_TOKEN)
			return SEC_E_INVALID_TOKEN;

		if (input_buffer->cbBuffer < 1)
			return SEC_E_INVALID_TOKEN;

		status = ntlm_read_AuthenticateMessage(context, input_buffer);

		return status;
	}

	return SEC_E_OUT_OF_SEQUENCE;
}

SECURITY_STATUS SEC_ENTRY ntlm_ImpersonateSecurityContext(PCtxtHandle phContext)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY ntlm_InitializeSecurityContextW(PCredHandle phCredential, PCtxtHandle phContext,
		SEC_WCHAR* pszTargetName, ULONG fContextReq, ULONG Reserved1, ULONG TargetDataRep,
		PSecBufferDesc pInput, ULONG Reserved2, PCtxtHandle phNewContext,
		PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	return SEC_E_OK;
}

/**
 * @see http://msdn.microsoft.com/en-us/library/windows/desktop/aa375512%28v=vs.85%29.aspx
 */
SECURITY_STATUS SEC_ENTRY ntlm_InitializeSecurityContextA(PCredHandle phCredential, PCtxtHandle phContext,
		SEC_CHAR* pszTargetName, ULONG fContextReq, ULONG Reserved1, ULONG TargetDataRep,
		PSecBufferDesc pInput, ULONG Reserved2, PCtxtHandle phNewContext,
		PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	NTLM_CONTEXT* context;
	SECURITY_STATUS status;
	CREDENTIALS* credentials;
	PSecBuffer input_buffer;
	PSecBuffer output_buffer;

	context = (NTLM_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
	{
		context = ntlm_ContextNew();
		if (!context)
			return SEC_E_INSUFFICIENT_MEMORY;

		if (fContextReq & ISC_REQ_CONFIDENTIALITY)
			context->confidentiality = 1;

		credentials = (CREDENTIALS*) sspi_SecureHandleGetLowerPointer(phCredential);

		sspi_CopyAuthIdentity(&context->identity, &credentials->identity);
		ntlm_SetContextWorkstation(context, NULL);

		sspi_SecureHandleSetLowerPointer(phNewContext, context);
		sspi_SecureHandleSetUpperPointer(phNewContext, (void*) NTLM_PACKAGE_NAME);
	}

	if ((!pInput) || (context->state == NTLM_STATE_AUTHENTICATE))
	{
		if (!pOutput)
			return SEC_E_INVALID_TOKEN;

		if (pOutput->cBuffers < 1)
			return SEC_E_INVALID_TOKEN;

		output_buffer = &pOutput->pBuffers[0];

		if (output_buffer->BufferType != SECBUFFER_TOKEN)
			return SEC_E_INVALID_TOKEN;

		if (output_buffer->cbBuffer < 1)
			return SEC_E_INVALID_TOKEN;

		if (context->state == NTLM_STATE_INITIAL)
			context->state = NTLM_STATE_NEGOTIATE;

		if (context->state == NTLM_STATE_NEGOTIATE)
			return ntlm_write_NegotiateMessage(context, output_buffer);

		return SEC_E_OUT_OF_SEQUENCE;
	}
	else
	{
		if (pInput->cBuffers < 1)
			return SEC_E_INVALID_TOKEN;

		input_buffer = &pInput->pBuffers[0];

		if (input_buffer->BufferType != SECBUFFER_TOKEN)
			return SEC_E_INVALID_TOKEN;

		if (input_buffer->cbBuffer < 1)
			return SEC_E_INVALID_TOKEN;

		if (context->state == NTLM_STATE_CHALLENGE)
		{
			status = ntlm_read_ChallengeMessage(context, input_buffer);

			if (!pOutput)
				return SEC_E_INVALID_TOKEN;

			if (pOutput->cBuffers < 1)
				return SEC_E_INVALID_TOKEN;

			output_buffer = &pOutput->pBuffers[0];

			if (output_buffer->BufferType != SECBUFFER_TOKEN)
				return SEC_E_INVALID_TOKEN;

			if (output_buffer->cbBuffer < 1)
				return SEC_E_INSUFFICIENT_MEMORY;

			if (context->state == NTLM_STATE_AUTHENTICATE)
				return ntlm_write_AuthenticateMessage(context, output_buffer);
		}

		return SEC_E_OUT_OF_SEQUENCE;
	}

	return SEC_E_OUT_OF_SEQUENCE;
}

/* http://msdn.microsoft.com/en-us/library/windows/desktop/aa375354 */

SECURITY_STATUS SEC_ENTRY ntlm_DeleteSecurityContext(PCtxtHandle phContext)
{
	NTLM_CONTEXT* context;

	context = (NTLM_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
		return SEC_E_INVALID_HANDLE;

	ntlm_ContextFree(context);

	return SEC_E_OK;
}

/* http://msdn.microsoft.com/en-us/library/windows/desktop/aa379337/ */

SECURITY_STATUS SEC_ENTRY ntlm_QueryContextAttributesW(PCtxtHandle phContext, ULONG ulAttribute, void* pBuffer)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY ntlm_QueryContextAttributesA(PCtxtHandle phContext, ULONG ulAttribute, void* pBuffer)
{
	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	if (ulAttribute == SECPKG_ATTR_SIZES)
	{
		SecPkgContext_Sizes* ContextSizes = (SecPkgContext_Sizes*) pBuffer;

		ContextSizes->cbMaxToken = 2010;
		ContextSizes->cbMaxSignature = 16;
		ContextSizes->cbBlockSize = 0;
		ContextSizes->cbSecurityTrailer = 16;

		return SEC_E_OK;
	}

	return SEC_E_UNSUPPORTED_FUNCTION;
}

SECURITY_STATUS SEC_ENTRY ntlm_RevertSecurityContext(PCtxtHandle phContext)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY ntlm_EncryptMessage(PCtxtHandle phContext, ULONG fQOP, PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
	int index;
	int length;
	void* data;
	UINT32 SeqNo;
	HMAC_CTX hmac;
	BYTE digest[16];
	BYTE checksum[8];
	BYTE* signature;
	ULONG version = 1;
	NTLM_CONTEXT* context;
	PSecBuffer data_buffer = NULL;
	PSecBuffer signature_buffer = NULL;

	SeqNo = MessageSeqNo;
	context = (NTLM_CONTEXT*) sspi_SecureHandleGetLowerPointer(phContext);

	for (index = 0; index < (int) pMessage->cBuffers; index++)
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
	CopyMemory(data, data_buffer->pvBuffer, length);

	/* Compute the HMAC-MD5 hash of ConcatenationOf(seq_num,data) using the client signing key */
	HMAC_CTX_init(&hmac);
	HMAC_Init_ex(&hmac, context->SendSigningKey, 16, EVP_md5(), NULL);
	HMAC_Update(&hmac, (void*) &(SeqNo), 4);
	HMAC_Update(&hmac, data, length);
	HMAC_Final(&hmac, digest, NULL);
	HMAC_CTX_cleanup(&hmac);

	/* Encrypt message using with RC4, result overwrites original buffer */

	if (context->confidentiality)
		RC4(&context->SendRc4Seal, length, data, data_buffer->pvBuffer);
	else
		CopyMemory(data_buffer->pvBuffer, data, length);

#ifdef WITH_DEBUG_NTLM
	printf("Data Buffer (length = %d)\n", length);
	winpr_HexDump(data, length);
	printf("\n");

	printf("Encrypted Data Buffer (length = %d)\n", (int) data_buffer->cbBuffer);
	winpr_HexDump(data_buffer->pvBuffer, data_buffer->cbBuffer);
	printf("\n");
#endif

	free(data);

	/* RC4-encrypt first 8 bytes of digest */
	RC4(&context->SendRc4Seal, 8, digest, checksum);

	signature = (BYTE*) signature_buffer->pvBuffer;

	/* Concatenate version, ciphertext and sequence number to build signature */
	CopyMemory(signature, (void*) &version, 4);
	CopyMemory(&signature[4], (void*) checksum, 8);
	CopyMemory(&signature[12], (void*) &(SeqNo), 4);
	context->SendSeqNum++;

#ifdef WITH_DEBUG_NTLM
	printf("Signature (length = %d)\n", (int) signature_buffer->cbBuffer);
	winpr_HexDump(signature_buffer->pvBuffer, signature_buffer->cbBuffer);
	printf("\n");
#endif

	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY ntlm_DecryptMessage(PCtxtHandle phContext, PSecBufferDesc pMessage, ULONG MessageSeqNo, PULONG pfQOP)
{
	int index;
	int length;
	void* data;
	UINT32 SeqNo;
	HMAC_CTX hmac;
	BYTE digest[16];
	BYTE checksum[8];
	UINT32 version = 1;
	NTLM_CONTEXT* context;
	BYTE expected_signature[16];
	PSecBuffer data_buffer = NULL;
	PSecBuffer signature_buffer = NULL;

	SeqNo = (UINT32) MessageSeqNo;
	context = sspi_SecureHandleGetLowerPointer(phContext);

	for (index = 0; index < (int) pMessage->cBuffers; index++)
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
	CopyMemory(data, data_buffer->pvBuffer, length);

	/* Decrypt message using with RC4, result overwrites original buffer */

	if (context->confidentiality)
		RC4(&context->RecvRc4Seal, length, data, data_buffer->pvBuffer);
	else
		CopyMemory(data_buffer->pvBuffer, data, length);

	/* Compute the HMAC-MD5 hash of ConcatenationOf(seq_num,data) using the client signing key */
	HMAC_CTX_init(&hmac);
	HMAC_Init_ex(&hmac, context->RecvSigningKey, 16, EVP_md5(), NULL);
	HMAC_Update(&hmac, (void*) &(SeqNo), 4);
	HMAC_Update(&hmac, data_buffer->pvBuffer, data_buffer->cbBuffer);
	HMAC_Final(&hmac, digest, NULL);
	HMAC_CTX_cleanup(&hmac);

#ifdef WITH_DEBUG_NTLM
	printf("Encrypted Data Buffer (length = %d)\n", length);
	winpr_HexDump(data, length);
	printf("\n");

	printf("Data Buffer (length = %d)\n", (int) data_buffer->cbBuffer);
	winpr_HexDump(data_buffer->pvBuffer, data_buffer->cbBuffer);
	printf("\n");
#endif

	free(data);

	/* RC4-encrypt first 8 bytes of digest */
	RC4(&context->RecvRc4Seal, 8, digest, checksum);

	/* Concatenate version, ciphertext and sequence number to build signature */
	CopyMemory(expected_signature, (void*) &version, 4);
	CopyMemory(&expected_signature[4], (void*) checksum, 8);
	CopyMemory(&expected_signature[12], (void*) &(SeqNo), 4);
	context->RecvSeqNum++;

	if (memcmp(signature_buffer->pvBuffer, expected_signature, 16) != 0)
	{
		/* signature verification failed! */
		printf("signature verification failed, something nasty is going on!\n");

		printf("Expected Signature:\n");
		winpr_HexDump(expected_signature, 16);
		printf("Actual Signature:\n");
		winpr_HexDump(signature_buffer->pvBuffer, 16);

		return SEC_E_MESSAGE_ALTERED;
	}

	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY ntlm_MakeSignature(PCtxtHandle phContext, ULONG fQOP, PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY ntlm_VerifySignature(PCtxtHandle phContext, PSecBufferDesc pMessage, ULONG MessageSeqNo, PULONG pfQOP)
{
	return SEC_E_OK;
}

const SecurityFunctionTableA NTLM_SecurityFunctionTableA =
{
	1, /* dwVersion */
	NULL, /* EnumerateSecurityPackages */
	ntlm_QueryCredentialsAttributesA, /* QueryCredentialsAttributes */
	ntlm_AcquireCredentialsHandleA, /* AcquireCredentialsHandle */
	ntlm_FreeCredentialsHandle, /* FreeCredentialsHandle */
	NULL, /* Reserved2 */
	ntlm_InitializeSecurityContextA, /* InitializeSecurityContext */
	ntlm_AcceptSecurityContext, /* AcceptSecurityContext */
	NULL, /* CompleteAuthToken */
	ntlm_DeleteSecurityContext, /* DeleteSecurityContext */
	NULL, /* ApplyControlToken */
	ntlm_QueryContextAttributesA, /* QueryContextAttributes */
	ntlm_ImpersonateSecurityContext, /* ImpersonateSecurityContext */
	ntlm_RevertSecurityContext, /* RevertSecurityContext */
	ntlm_MakeSignature, /* MakeSignature */
	ntlm_VerifySignature, /* VerifySignature */
	NULL, /* FreeContextBuffer */
	NULL, /* QuerySecurityPackageInfo */
	NULL, /* Reserved3 */
	NULL, /* Reserved4 */
	NULL, /* ExportSecurityContext */
	NULL, /* ImportSecurityContext */
	NULL, /* AddCredentials */
	NULL, /* Reserved8 */
	NULL, /* QuerySecurityContextToken */
	ntlm_EncryptMessage, /* EncryptMessage */
	ntlm_DecryptMessage, /* DecryptMessage */
	NULL, /* SetContextAttributes */
};

const SecurityFunctionTableW NTLM_SecurityFunctionTableW =
{
	1, /* dwVersion */
	NULL, /* EnumerateSecurityPackages */
	ntlm_QueryCredentialsAttributesW, /* QueryCredentialsAttributes */
	ntlm_AcquireCredentialsHandleW, /* AcquireCredentialsHandle */
	ntlm_FreeCredentialsHandle, /* FreeCredentialsHandle */
	NULL, /* Reserved2 */
	ntlm_InitializeSecurityContextW, /* InitializeSecurityContext */
	ntlm_AcceptSecurityContext, /* AcceptSecurityContext */
	NULL, /* CompleteAuthToken */
	ntlm_DeleteSecurityContext, /* DeleteSecurityContext */
	NULL, /* ApplyControlToken */
	ntlm_QueryContextAttributesW, /* QueryContextAttributes */
	ntlm_ImpersonateSecurityContext, /* ImpersonateSecurityContext */
	ntlm_RevertSecurityContext, /* RevertSecurityContext */
	ntlm_MakeSignature, /* MakeSignature */
	ntlm_VerifySignature, /* VerifySignature */
	NULL, /* FreeContextBuffer */
	NULL, /* QuerySecurityPackageInfo */
	NULL, /* Reserved3 */
	NULL, /* Reserved4 */
	NULL, /* ExportSecurityContext */
	NULL, /* ImportSecurityContext */
	NULL, /* AddCredentials */
	NULL, /* Reserved8 */
	NULL, /* QuerySecurityContextToken */
	ntlm_EncryptMessage, /* EncryptMessage */
	ntlm_DecryptMessage, /* DecryptMessage */
	NULL, /* SetContextAttributes */
};

const SecPkgInfoA NTLM_SecPkgInfoA =
{
	0x00082B37, /* fCapabilities */
	1, /* wVersion */
	0x000A, /* wRPCID */
	0x00000B48, /* cbMaxToken */
	"NTLM", /* Name */
	"NTLM Security Package" /* Comment */
};

WCHAR NTLM_SecPkgInfoW_Name[] = { 'N','T','L','M','\0' };

WCHAR NTLM_SecPkgInfoW_Comment[] =
{
	'N','T','L','M',' ',
	'S','e','c','u','r','i','t','y',' ',
	'P','a','c','k','a','g','e','\0'
};

const SecPkgInfoW NTLM_SecPkgInfoW =
{
	0x00082B37, /* fCapabilities */
	1, /* wVersion */
	0x000A, /* wRPCID */
	0x00000B48, /* cbMaxToken */
	NTLM_SecPkgInfoW_Name, /* Name */
	NTLM_SecPkgInfoW_Comment /* Comment */
};

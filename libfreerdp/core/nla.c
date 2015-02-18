/**
 * WinPR: Windows Portable Runtime
 * Network Level Authentication (NLA)
 *
 * Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		 http://www.apache.org/licenses/LICENSE-2.0
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

#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <freerdp/log.h>
#include <freerdp/crypto/tls.h>

#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/print.h>
#include <winpr/tchar.h>
#include <winpr/dsparse.h>
#include <winpr/library.h>
#include <winpr/registry.h>

#include "nla.h"

#define TAG FREERDP_TAG("core.nla")

/**
 * TSRequest ::= SEQUENCE {
 * 	version    [0] INTEGER,
 * 	negoTokens [1] NegoData OPTIONAL,
 * 	authInfo   [2] OCTET STRING OPTIONAL,
 * 	pubKeyAuth [3] OCTET STRING OPTIONAL
 * }
 *
 * NegoData ::= SEQUENCE OF NegoDataItem
 *
 * NegoDataItem ::= SEQUENCE {
 * 	negoToken [0] OCTET STRING
 * }
 *
 * TSCredentials ::= SEQUENCE {
 * 	credType    [0] INTEGER,
 * 	credentials [1] OCTET STRING
 * }
 *
 * TSPasswordCreds ::= SEQUENCE {
 * 	domainName  [0] OCTET STRING,
 * 	userName    [1] OCTET STRING,
 * 	password    [2] OCTET STRING
 * }
 *
 * TSSmartCardCreds ::= SEQUENCE {
 * 	pin        [0] OCTET STRING,
 * 	cspData    [1] TSCspDataDetail,
 * 	userHint   [2] OCTET STRING OPTIONAL,
 * 	domainHint [3] OCTET STRING OPTIONAL
 * }
 *
 * TSCspDataDetail ::= SEQUENCE {
 * 	keySpec       [0] INTEGER,
 * 	cardName      [1] OCTET STRING OPTIONAL,
 * 	readerName    [2] OCTET STRING OPTIONAL,
 * 	containerName [3] OCTET STRING OPTIONAL,
 * 	cspName       [4] OCTET STRING OPTIONAL
 * }
 *
 */

#define NLA_PKG_NAME	NEGOSSP_NAME

#define TERMSRV_SPN_PREFIX	"TERMSRV/"

void nla_send(rdpNla* nla);
int nla_recv(rdpNla* nla);
void nla_buffer_print(rdpNla* nla);
void nla_buffer_free(rdpNla* nla);
SECURITY_STATUS nla_encrypt_public_key_echo(rdpNla* nla);
SECURITY_STATUS nla_decrypt_public_key_echo(rdpNla* nla);
SECURITY_STATUS nla_encrypt_ts_credentials(rdpNla* nla);
SECURITY_STATUS nla_decrypt_ts_credentials(rdpNla* nla);

#define ber_sizeof_sequence_octet_string(length) ber_sizeof_contextual_tag(ber_sizeof_octet_string(length)) + ber_sizeof_octet_string(length)
#define ber_write_sequence_octet_string(stream, context, value, length) ber_write_contextual_tag(stream, context, ber_sizeof_octet_string(length), TRUE) + ber_write_octet_string(stream, value, length)

/**
 * Initialize NTLMSSP authentication module (client).
 * @param credssp
 */

int nla_client_init(rdpNla* nla)
{
	char* spn;
	int length;
	rdpTls* tls = NULL;
	BOOL PromptPassword = FALSE;
	freerdp* instance = nla->instance;
	rdpSettings* settings = nla->settings;

	nla->state = NLA_STATE_INITIAL;

	if (settings->RestrictedAdminModeRequired)
		settings->DisableCredentialsDelegation = TRUE;

	if ((!settings->Password) || (!settings->Username)
			|| (!strlen(settings->Password)) || (!strlen(settings->Username)))
	{
		PromptPassword = TRUE;
	}

#ifndef _WIN32
	if (PromptPassword)
	{
		if (settings->RestrictedAdminModeRequired)
		{
			if ((settings->PasswordHash) && (strlen(settings->PasswordHash) > 0))
				PromptPassword = FALSE;
		}
	}
#endif

	if (PromptPassword)
	{
		if (instance->Authenticate)
		{
			BOOL proceed = instance->Authenticate(instance,
						&settings->Username, &settings->Password, &settings->Domain);

			if (!proceed)
			{
				freerdp_set_last_error(instance->context, FREERDP_ERROR_CONNECT_CANCELLED);
				return 0;
			}
		}
	}

	sspi_SetAuthIdentity(&(nla->identity), settings->Username, settings->Domain, settings->Password);

#ifndef _WIN32
	{
		SEC_WINNT_AUTH_IDENTITY* identity = &(nla->identity);

		if (settings->RestrictedAdminModeRequired)
		{
			if (settings->PasswordHash)
			{
				if (strlen(settings->PasswordHash) == 32)
				{
					if (identity->Password)
						free(identity->Password);

					identity->PasswordLength = ConvertToUnicode(CP_UTF8, 0,
							settings->PasswordHash, -1, &identity->Password, 0) - 1;
					/**
					 * Multiply password hash length by 64 to obtain a length exceeding
					 * the maximum (256) and use it this for hash identification in WinPR.
					 */
					identity->PasswordLength = 32 * 64; /* 2048 */
				}
			}
		}
	}
#endif

	tls = nla->transport->tls;

	if (!tls)
	{
		WLog_ERR(TAG, "Unknown NLA transport layer");
		return -1;
	}

	sspi_SecBufferAlloc(&nla->PublicKey, tls->PublicKeyLength);
	CopyMemory(nla->PublicKey.pvBuffer, tls->PublicKey, tls->PublicKeyLength);
	length = sizeof(TERMSRV_SPN_PREFIX) + strlen(settings->ServerHostname);

	spn = (SEC_CHAR*) malloc(length + 1);

	if (!spn)
		return -1;

	sprintf(spn, "%s%s", TERMSRV_SPN_PREFIX, settings->ServerHostname);

#ifdef UNICODE
	nla->ServicePrincipalName = NULL;
	ConvertToUnicode(CP_UTF8, 0, spn, -1, &nla->ServicePrincipalName, 0);
	free(spn);
#else
	nla->ServicePrincipalName = spn;
#endif

	nla->table = InitSecurityInterfaceEx(SSPI_INTERFACE_WINPR);
	nla->status = nla->table->QuerySecurityPackageInfo(NLA_PKG_NAME, &nla->pPackageInfo);

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "QuerySecurityPackageInfo status: 0x%08X", nla->status);
		return -1;
	}

	nla->cbMaxToken = nla->pPackageInfo->cbMaxToken;
	nla->status = nla->table->AcquireCredentialsHandle(NULL, NLA_PKG_NAME,
			 SECPKG_CRED_OUTBOUND, NULL, &nla->identity, NULL, NULL, &nla->credentials, &nla->expiration);

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "AcquireCredentialsHandle status: 0x%08X", nla->status);
		return -1;
	}

	nla->haveContext = FALSE;
	nla->haveInputBuffer = FALSE;
	nla->havePubKeyAuth = FALSE;
	ZeroMemory(&nla->inputBuffer, sizeof(SecBuffer));
	ZeroMemory(&nla->outputBuffer, sizeof(SecBuffer));
	ZeroMemory(&nla->ContextSizes, sizeof(SecPkgContext_Sizes));

	/*
	 * from tspkg.dll: 0x00000132
	 * ISC_REQ_MUTUAL_AUTH
	 * ISC_REQ_CONFIDENTIALITY
	 * ISC_REQ_USE_SESSION_KEY
	 * ISC_REQ_ALLOCATE_MEMORY
	 */
	nla->fContextReq = ISC_REQ_MUTUAL_AUTH | ISC_REQ_CONFIDENTIALITY | ISC_REQ_USE_SESSION_KEY;

	return 1;
}

int nla_client_begin(rdpNla* nla)
{
	if (nla_client_init(nla) < 1)
		return -1;

	if (nla->state != NLA_STATE_INITIAL)
		return -1;

	nla->outputBufferDesc.ulVersion = SECBUFFER_VERSION;
	nla->outputBufferDesc.cBuffers = 1;
	nla->outputBufferDesc.pBuffers = &nla->outputBuffer;
	nla->outputBuffer.BufferType = SECBUFFER_TOKEN;
	nla->outputBuffer.cbBuffer = nla->cbMaxToken;
	nla->outputBuffer.pvBuffer = malloc(nla->outputBuffer.cbBuffer);

	if (!nla->outputBuffer.pvBuffer)
		return -1;

	nla->status = nla->table->InitializeSecurityContext(&nla->credentials,
			NULL, nla->ServicePrincipalName, nla->fContextReq, 0,
			SECURITY_NATIVE_DREP, NULL, 0, &nla->context,
			&nla->outputBufferDesc, &nla->pfContextAttr, &nla->expiration);

	if ((nla->status == SEC_I_COMPLETE_AND_CONTINUE) || (nla->status == SEC_I_COMPLETE_NEEDED))
	{
		if (nla->table->CompleteAuthToken)
			nla->table->CompleteAuthToken(&nla->context, &nla->outputBufferDesc);

		if (nla->status == SEC_I_COMPLETE_NEEDED)
			nla->status = SEC_E_OK;
		else if (nla->status == SEC_I_COMPLETE_AND_CONTINUE)
			nla->status = SEC_I_CONTINUE_NEEDED;
	}

	if (nla->status != SEC_I_CONTINUE_NEEDED)
		return -1;

	if (nla->outputBuffer.cbBuffer < 1)
		return -1;

	nla->negoToken.pvBuffer = nla->outputBuffer.pvBuffer;
	nla->negoToken.cbBuffer = nla->outputBuffer.cbBuffer;

	WLog_DBG(TAG, "Sending Authentication Token");
	winpr_HexDump(TAG, WLOG_DEBUG, nla->negoToken.pvBuffer, nla->negoToken.cbBuffer);

	nla_send(nla);
	nla_buffer_free(nla);

	nla->state = NLA_STATE_NEGO_TOKEN;

	return 1;
}

int nla_client_recv(rdpNla* nla)
{
	int status = -1;

	if (nla->state == NLA_STATE_NEGO_TOKEN)
	{
		nla->inputBufferDesc.ulVersion = SECBUFFER_VERSION;
		nla->inputBufferDesc.cBuffers = 1;
		nla->inputBufferDesc.pBuffers = &nla->inputBuffer;
		nla->inputBuffer.BufferType = SECBUFFER_TOKEN;
		nla->inputBuffer.pvBuffer = nla->negoToken.pvBuffer;
		nla->inputBuffer.cbBuffer = nla->negoToken.cbBuffer;

		nla->outputBufferDesc.ulVersion = SECBUFFER_VERSION;
		nla->outputBufferDesc.cBuffers = 1;
		nla->outputBufferDesc.pBuffers = &nla->outputBuffer;
		nla->outputBuffer.BufferType = SECBUFFER_TOKEN;
		nla->outputBuffer.cbBuffer = nla->cbMaxToken;
		nla->outputBuffer.pvBuffer = malloc(nla->outputBuffer.cbBuffer);

		if (!nla->outputBuffer.pvBuffer)
			return -1;

		nla->status = nla->table->InitializeSecurityContext(&nla->credentials,
				&nla->context, nla->ServicePrincipalName, nla->fContextReq, 0,
				SECURITY_NATIVE_DREP, &nla->inputBufferDesc,
				0, &nla->context, &nla->outputBufferDesc, &nla->pfContextAttr, &nla->expiration);

		free(nla->inputBuffer.pvBuffer);
		nla->inputBuffer.pvBuffer = NULL;

		if ((nla->status == SEC_I_COMPLETE_AND_CONTINUE) || (nla->status == SEC_I_COMPLETE_NEEDED))
		{
			if (nla->table->CompleteAuthToken)
				nla->table->CompleteAuthToken(&nla->context, &nla->outputBufferDesc);

			if (nla->status == SEC_I_COMPLETE_NEEDED)
				nla->status = SEC_E_OK;
			else if (nla->status == SEC_I_COMPLETE_AND_CONTINUE)
				nla->status = SEC_I_CONTINUE_NEEDED;
		}

		if (nla->status == SEC_E_OK)
		{
			nla->havePubKeyAuth = TRUE;

			if (nla->table->QueryContextAttributes(&nla->context, SECPKG_ATTR_SIZES, &nla->ContextSizes) != SEC_E_OK)
			{
				WLog_ERR(TAG, "QueryContextAttributes SECPKG_ATTR_SIZES failure");
				return -1;
			}

			nla_encrypt_public_key_echo(nla);
		}

		if (nla->outputBuffer.cbBuffer < 1)
			return -1;

		nla->negoToken.pvBuffer = nla->outputBuffer.pvBuffer;
		nla->negoToken.cbBuffer = nla->outputBuffer.cbBuffer;

		WLog_DBG(TAG, "Sending Authentication Token");
		winpr_HexDump(TAG, WLOG_DEBUG, nla->negoToken.pvBuffer, nla->negoToken.cbBuffer);

		nla_send(nla);
		nla_buffer_free(nla);

		nla->state = NLA_STATE_PUB_KEY_AUTH;
		status = 1;
	}
	else if (nla->state == NLA_STATE_PUB_KEY_AUTH)
	{
		/* Verify Server Public Key Echo */
		nla->status = nla_decrypt_public_key_echo(nla);
		nla_buffer_free(nla);

		if (nla->status != SEC_E_OK)
		{
			WLog_ERR(TAG, "Could not verify public key echo!");
			return -1;
		}

		/* Send encrypted credentials */
		nla->status = nla_encrypt_ts_credentials(nla);

		if (nla->status != SEC_E_OK)
		{
			WLog_ERR(TAG, "nla_encrypt_ts_credentials status: 0x%08X", nla->status);
			return -1;
		}

		nla_send(nla);
		nla_buffer_free(nla);

		nla->table->FreeCredentialsHandle(&nla->credentials);
		nla->table->FreeContextBuffer(nla->pPackageInfo);

		nla->state = NLA_STATE_AUTH_INFO;
		status = 1;
	}

	return status;
}

int nla_client_authenticate(rdpNla* nla)
{
	wStream* s;
	int status;

	s = Stream_New(NULL, 4096);

	if (nla_client_begin(nla) < 1)
		return -1;

	while (nla->state < NLA_STATE_AUTH_INFO)
	{
		Stream_SetPosition(s, 0);

		status = transport_read_pdu(nla->transport, s);

		if (status < 0)
		{
			WLog_ERR(TAG, "nla_client_authenticate failure");
			Stream_Free(s, TRUE);
			return -1;
		}

		status = nla_recv_pdu(nla, s);

		if (status < 0)
			return -1;
	}

	Stream_Free(s, TRUE);

	return 1;
}

/**
 * Initialize NTLMSSP authentication module (server).
 * @param credssp
 */

int nla_server_init(rdpNla* nla)
{
	rdpTls* tls = nla->transport->tls;

	sspi_SecBufferAlloc(&nla->PublicKey, tls->PublicKeyLength);
	CopyMemory(nla->PublicKey.pvBuffer, tls->PublicKey, tls->PublicKeyLength);

	if (nla->SspiModule)
	{
		HMODULE hSSPI;
		INIT_SECURITY_INTERFACE pInitSecurityInterface;

		hSSPI = LoadLibrary(nla->SspiModule);

		if (!hSSPI)
		{
			WLog_ERR(TAG, "Failed to load SSPI module: %s", nla->SspiModule);
			return -1;
		}

#ifdef UNICODE
		pInitSecurityInterface = (INIT_SECURITY_INTERFACE) GetProcAddress(hSSPI, "InitSecurityInterfaceW");
#else
		pInitSecurityInterface = (INIT_SECURITY_INTERFACE) GetProcAddress(hSSPI, "InitSecurityInterfaceA");
#endif
		nla->table = pInitSecurityInterface();
	}
	else
	{
		nla->table = InitSecurityInterfaceEx(0);
	}

	nla->status = nla->table->QuerySecurityPackageInfo(NLA_PKG_NAME, &nla->pPackageInfo);

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "QuerySecurityPackageInfo status: 0x%08X", nla->status);
		return -1;
	}

	nla->cbMaxToken = nla->pPackageInfo->cbMaxToken;
	nla->status = nla->table->AcquireCredentialsHandle(NULL, NLA_PKG_NAME,
			 SECPKG_CRED_INBOUND, NULL, NULL, NULL, NULL, &nla->credentials, &nla->expiration);

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "AcquireCredentialsHandle status: 0x%08X", nla->status);
		return -1;
	}

	nla->haveContext = FALSE;
	nla->haveInputBuffer = FALSE;
	nla->havePubKeyAuth = FALSE;
	ZeroMemory(&nla->inputBuffer, sizeof(SecBuffer));
	ZeroMemory(&nla->outputBuffer, sizeof(SecBuffer));
	ZeroMemory(&nla->inputBufferDesc, sizeof(SecBufferDesc));
	ZeroMemory(&nla->outputBufferDesc, sizeof(SecBufferDesc));
	ZeroMemory(&nla->ContextSizes, sizeof(SecPkgContext_Sizes));

	/*
	 * from tspkg.dll: 0x00000112
	 * ASC_REQ_MUTUAL_AUTH
	 * ASC_REQ_CONFIDENTIALITY
	 * ASC_REQ_ALLOCATE_MEMORY
	 */
	nla->fContextReq = 0;
	nla->fContextReq |= ASC_REQ_MUTUAL_AUTH;
	nla->fContextReq |= ASC_REQ_CONFIDENTIALITY;
	nla->fContextReq |= ASC_REQ_CONNECTION;
	nla->fContextReq |= ASC_REQ_USE_SESSION_KEY;
	nla->fContextReq |= ASC_REQ_REPLAY_DETECT;
	nla->fContextReq |= ASC_REQ_SEQUENCE_DETECT;
	nla->fContextReq |= ASC_REQ_EXTENDED_ERROR;

	return 1;
}

/**
 * Authenticate with client using CredSSP (server).
 * @param credssp
 * @return 1 if authentication is successful
 */

int nla_server_authenticate(rdpNla* nla)
{
	if (nla_server_init(nla) < 1)
		return -1;

	while (TRUE)
	{
		/* receive authentication token */
		nla->inputBufferDesc.ulVersion = SECBUFFER_VERSION;
		nla->inputBufferDesc.cBuffers = 1;
		nla->inputBufferDesc.pBuffers = &nla->inputBuffer;
		nla->inputBuffer.BufferType = SECBUFFER_TOKEN;

		if (nla_recv(nla) < 0)
			return -1;

		WLog_DBG(TAG, "Receiving Authentication Token");
		nla_buffer_print(nla);

		nla->inputBuffer.pvBuffer = nla->negoToken.pvBuffer;
		nla->inputBuffer.cbBuffer = nla->negoToken.cbBuffer;

		if (nla->negoToken.cbBuffer < 1)
		{
			WLog_ERR(TAG, "CredSSP: invalid negoToken!");
			return -1;
		}

		nla->outputBufferDesc.ulVersion = SECBUFFER_VERSION;
		nla->outputBufferDesc.cBuffers = 1;
		nla->outputBufferDesc.pBuffers = &nla->outputBuffer;
		nla->outputBuffer.BufferType = SECBUFFER_TOKEN;
		nla->outputBuffer.cbBuffer = nla->cbMaxToken;
		nla->outputBuffer.pvBuffer = malloc(nla->outputBuffer.cbBuffer);

		if (!nla->outputBuffer.pvBuffer)
			return -1;

		nla->status = nla->table->AcceptSecurityContext(&nla->credentials,
				nla-> haveContext? &nla->context: NULL,
				 &nla->inputBufferDesc, nla->fContextReq, SECURITY_NATIVE_DREP, &nla->context,
				 &nla->outputBufferDesc, &nla->pfContextAttr, &nla->expiration);

		nla->negoToken.pvBuffer = nla->outputBuffer.pvBuffer;
		nla->negoToken.cbBuffer = nla->outputBuffer.cbBuffer;

		if ((nla->status == SEC_I_COMPLETE_AND_CONTINUE) || (nla->status == SEC_I_COMPLETE_NEEDED))
		{
			if (nla->table->CompleteAuthToken)
				nla->table->CompleteAuthToken(&nla->context, &nla->outputBufferDesc);

			if (nla->status == SEC_I_COMPLETE_NEEDED)
				nla->status = SEC_E_OK;
			else if (nla->status == SEC_I_COMPLETE_AND_CONTINUE)
				nla->status = SEC_I_CONTINUE_NEEDED;
		}

		if (nla->status == SEC_E_OK)
		{
			nla->havePubKeyAuth = TRUE;

			if (nla->table->QueryContextAttributes(&nla->context, SECPKG_ATTR_SIZES, &nla->ContextSizes) != SEC_E_OK)
			{
				WLog_ERR(TAG, "QueryContextAttributes SECPKG_ATTR_SIZES failure");
				return -1;
			}

			if (nla_decrypt_public_key_echo(nla) != SEC_E_OK)
			{
				WLog_ERR(TAG, "Error: could not verify client's public key echo");
				return -1;
			}

			sspi_SecBufferFree(&nla->negoToken);
			nla->negoToken.pvBuffer = NULL;
			nla->negoToken.cbBuffer = 0;
			nla_encrypt_public_key_echo(nla);
		}

		if ((nla->status != SEC_E_OK) && (nla->status != SEC_I_CONTINUE_NEEDED))
		{
			WLog_ERR(TAG, "AcceptSecurityContext status: 0x%08X", nla->status);
			return -1; /* Access Denied */
		}

		/* send authentication token */

		WLog_DBG(TAG, "Sending Authentication Token");
		nla_buffer_print(nla);

		nla_send(nla);
		nla_buffer_free(nla);

		if (nla->status != SEC_I_CONTINUE_NEEDED)
			break;

		nla->haveContext = TRUE;
	}

	/* Receive encrypted credentials */

	if (nla_recv(nla) < 0)
		return -1;

	if (nla_decrypt_ts_credentials(nla) != SEC_E_OK)
	{
		WLog_ERR(TAG, "Could not decrypt TSCredentials status: 0x%08X", nla->status);
		return -1;
	}

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "AcceptSecurityContext status: 0x%08X", nla->status);
		return -1;
	}

	nla->status = nla->table->ImpersonateSecurityContext(&nla->context);

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "ImpersonateSecurityContext status: 0x%08X", nla->status);
		return -1;
	}
	else
	{
		nla->status = nla->table->RevertSecurityContext(&nla->context);

		if (nla->status != SEC_E_OK)
		{
			WLog_ERR(TAG, "RevertSecurityContext status: 0x%08X", nla->status);
			return -1;
		}
	}

	nla->table->FreeContextBuffer(nla->pPackageInfo);

	return 1;
}

/**
 * Authenticate using CredSSP.
 * @param credssp
 * @return 1 if authentication is successful
 */

int nla_authenticate(rdpNla* nla)
{
	if (nla->server)
		return nla_server_authenticate(nla);
	else
		return nla_client_authenticate(nla);
}

void ap_integer_increment_le(BYTE* number, int size)
{
	int index;

	for (index = 0; index < size; index++)
	{
		if (number[index] < 0xFF)
		{
			number[index]++;
			break;
		}
		else
		{
			number[index] = 0;
			continue;
		}
	}
}

void ap_integer_decrement_le(BYTE* number, int size)
{
	int index;

	for (index = 0; index < size; index++)
	{
		if (number[index] > 0)
		{
			number[index]--;
			break;
		}
		else
		{
			number[index] = 0xFF;
			continue;
		}
	}
}

SECURITY_STATUS nla_encrypt_public_key_echo(rdpNla* nla)
{
	SecBuffer Buffers[2];
	SecBufferDesc Message;
	SECURITY_STATUS status;
	int public_key_length;

	public_key_length = nla->PublicKey.cbBuffer;
	Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
	Buffers[1].BufferType = SECBUFFER_DATA; /* TLS Public Key */
	sspi_SecBufferAlloc(&nla->pubKeyAuth, nla->ContextSizes.cbMaxSignature + public_key_length);
	Buffers[0].cbBuffer = nla->ContextSizes.cbMaxSignature;
	Buffers[0].pvBuffer = nla->pubKeyAuth.pvBuffer;
	Buffers[1].cbBuffer = public_key_length;
	Buffers[1].pvBuffer = ((BYTE*) nla->pubKeyAuth.pvBuffer) + nla->ContextSizes.cbMaxSignature;
	CopyMemory(Buffers[1].pvBuffer, nla->PublicKey.pvBuffer, Buffers[1].cbBuffer);

	if (nla->server)
	{
		/* server echos the public key +1 */
		ap_integer_increment_le((BYTE*) Buffers[1].pvBuffer, Buffers[1].cbBuffer);
	}

	Message.cBuffers = 2;
	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer) &Buffers;
	status = nla->table->EncryptMessage(&nla->context, 0, &Message, nla->sendSeqNum++);

	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "EncryptMessage status: 0x%08X", status);
		return status;
	}

	return status;
}

SECURITY_STATUS nla_decrypt_public_key_echo(rdpNla* nla)
{
	int length;
	BYTE* buffer;
	ULONG pfQOP = 0;
	BYTE* public_key1;
	BYTE* public_key2;
	int public_key_length;
	SecBuffer Buffers[2];
	SecBufferDesc Message;
	SECURITY_STATUS status;

	if ((nla->PublicKey.cbBuffer + nla->ContextSizes.cbMaxSignature) != nla->pubKeyAuth.cbBuffer)
	{
		WLog_ERR(TAG, "unexpected pubKeyAuth buffer size: %d", (int) nla->pubKeyAuth.cbBuffer);
		return SEC_E_INVALID_TOKEN;
	}

	length = nla->pubKeyAuth.cbBuffer;
	buffer = (BYTE*) malloc(length);

	if (!buffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	CopyMemory(buffer, nla->pubKeyAuth.pvBuffer, length);
	public_key_length = nla->PublicKey.cbBuffer;
	Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
	Buffers[1].BufferType = SECBUFFER_DATA; /* Encrypted TLS Public Key */
	Buffers[0].cbBuffer = nla->ContextSizes.cbMaxSignature;
	Buffers[0].pvBuffer = buffer;
	Buffers[1].cbBuffer = length - nla->ContextSizes.cbMaxSignature;
	Buffers[1].pvBuffer = buffer + nla->ContextSizes.cbMaxSignature;
	Message.cBuffers = 2;
	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer) &Buffers;
	status = nla->table->DecryptMessage(&nla->context, &Message, nla->recvSeqNum++, &pfQOP);

	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "DecryptMessage failure: 0x%08X", status);
		return status;
	}

	public_key1 = (BYTE*) nla->PublicKey.pvBuffer;
	public_key2 = (BYTE*) Buffers[1].pvBuffer;

	if (!nla->server)
	{
		/* server echos the public key +1 */
		ap_integer_decrement_le(public_key2, public_key_length);
	}

	if (memcmp(public_key1, public_key2, public_key_length) != 0)
	{
		WLog_ERR(TAG, "Could not verify server's public key echo");
		WLog_ERR(TAG, "Expected (length = %d):", public_key_length);
		winpr_HexDump(TAG, WLOG_ERROR, public_key1, public_key_length);
		WLog_ERR(TAG, "Actual (length = %d):", public_key_length);
		winpr_HexDump(TAG, WLOG_ERROR, public_key2, public_key_length);
		return SEC_E_MESSAGE_ALTERED; /* DO NOT SEND CREDENTIALS! */
	}

	free(buffer);
	return SEC_E_OK;
}

int nla_sizeof_ts_password_creds(rdpNla* nla)
{
	int length = 0;
	length += ber_sizeof_sequence_octet_string(nla->identity.DomainLength * 2);
	length += ber_sizeof_sequence_octet_string(nla->identity.UserLength * 2);
	length += ber_sizeof_sequence_octet_string(nla->identity.PasswordLength * 2);
	return length;
}

void nla_read_ts_password_creds(rdpNla* nla, wStream* s)
{
	int length;

	/* TSPasswordCreds (SEQUENCE) */
	ber_read_sequence_tag(s, &length);

	/* [0] domainName (OCTET STRING) */
	ber_read_contextual_tag(s, 0, &length, TRUE);
	ber_read_octet_string_tag(s, &length);
	nla->identity.DomainLength = (UINT32) length;
	nla->identity.Domain = (UINT16*) malloc(length);
	CopyMemory(nla->identity.Domain, Stream_Pointer(s), nla->identity.DomainLength);
	Stream_Seek(s, nla->identity.DomainLength);
	nla->identity.DomainLength /= 2;

	/* [1] userName (OCTET STRING) */
	ber_read_contextual_tag(s, 1, &length, TRUE);
	ber_read_octet_string_tag(s, &length);
	nla->identity.UserLength = (UINT32) length;
	nla->identity.User = (UINT16*) malloc(length);
	CopyMemory(nla->identity.User, Stream_Pointer(s), nla->identity.UserLength);
	Stream_Seek(s, nla->identity.UserLength);
	nla->identity.UserLength /= 2;

	/* [2] password (OCTET STRING) */
	ber_read_contextual_tag(s, 2, &length, TRUE);
	ber_read_octet_string_tag(s, &length);
	nla->identity.PasswordLength = (UINT32) length;
	nla->identity.Password = (UINT16*) malloc(length);
	CopyMemory(nla->identity.Password, Stream_Pointer(s), nla->identity.PasswordLength);
	Stream_Seek(s, nla->identity.PasswordLength);
	nla->identity.PasswordLength /= 2;
	nla->identity.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
}

int nla_write_ts_password_creds(rdpNla* nla, wStream* s)
{
	int size = 0;
	int innerSize = nla_sizeof_ts_password_creds(nla);
	/* TSPasswordCreds (SEQUENCE) */
	size += ber_write_sequence_tag(s, innerSize);
	/* [0] domainName (OCTET STRING) */
	size += ber_write_sequence_octet_string(s, 0, (BYTE*) nla->identity.Domain, nla->identity.DomainLength * 2);
	/* [1] userName (OCTET STRING) */
	size += ber_write_sequence_octet_string(s, 1, (BYTE*) nla->identity.User, nla->identity.UserLength * 2);
	/* [2] password (OCTET STRING) */
	size += ber_write_sequence_octet_string(s, 2, (BYTE*) nla->identity.Password, nla->identity.PasswordLength * 2);
	return size;
}

int nla_sizeof_ts_credentials(rdpNla* nla)
{
	int size = 0;
	size += ber_sizeof_integer(1);
	size += ber_sizeof_contextual_tag(ber_sizeof_integer(1));
	size += ber_sizeof_sequence_octet_string(ber_sizeof_sequence(nla_sizeof_ts_password_creds(nla)));
	return size;
}

void nla_read_ts_credentials(rdpNla* nla, PSecBuffer ts_credentials)
{
	wStream* s;
	int length;
	int ts_password_creds_length;

	s = Stream_New(ts_credentials->pvBuffer, ts_credentials->cbBuffer);

	/* TSCredentials (SEQUENCE) */
	ber_read_sequence_tag(s, &length);

	/* [0] credType (INTEGER) */
	ber_read_contextual_tag(s, 0, &length, TRUE);
	ber_read_integer(s, NULL);

	/* [1] credentials (OCTET STRING) */
	ber_read_contextual_tag(s, 1, &length, TRUE);
	ber_read_octet_string_tag(s, &ts_password_creds_length);

	nla_read_ts_password_creds(nla, s);

	Stream_Free(s, FALSE);
}

int nla_write_ts_credentials(rdpNla* nla, wStream* s)
{
	int size = 0;
	int passwordSize;
	int innerSize = nla_sizeof_ts_credentials(nla);

	/* TSCredentials (SEQUENCE) */
	size += ber_write_sequence_tag(s, innerSize);

	/* [0] credType (INTEGER) */
	size += ber_write_contextual_tag(s, 0, ber_sizeof_integer(1), TRUE);
	size += ber_write_integer(s, 1);

	/* [1] credentials (OCTET STRING) */
	passwordSize = ber_sizeof_sequence(nla_sizeof_ts_password_creds(nla));
	size += ber_write_contextual_tag(s, 1, ber_sizeof_octet_string(passwordSize), TRUE);
	size += ber_write_octet_string_tag(s, passwordSize);
	size += nla_write_ts_password_creds(nla, s);

	return size;
}

/**
 * Encode TSCredentials structure.
 * @param credssp
 */

void nla_encode_ts_credentials(rdpNla* nla)
{
	wStream* s;
	int length;
	int DomainLength;
	int UserLength;
	int PasswordLength;

	DomainLength = nla->identity.DomainLength;
	UserLength = nla->identity.UserLength;
	PasswordLength = nla->identity.PasswordLength;

	if (nla->settings->DisableCredentialsDelegation)
	{
		nla->identity.DomainLength = 0;
		nla->identity.UserLength = 0;
		nla->identity.PasswordLength = 0;
	}

	length = ber_sizeof_sequence(nla_sizeof_ts_credentials(nla));
	sspi_SecBufferAlloc(&nla->tsCredentials, length);
	s = Stream_New((BYTE*) nla->tsCredentials.pvBuffer, length);
	nla_write_ts_credentials(nla, s);

	if (nla->settings->DisableCredentialsDelegation)
	{
		nla->identity.DomainLength = DomainLength;
		nla->identity.UserLength = UserLength;
		nla->identity.PasswordLength = PasswordLength;
	}

	Stream_Free(s, FALSE);
}

SECURITY_STATUS nla_encrypt_ts_credentials(rdpNla* nla)
{
	SecBuffer Buffers[2];
	SecBufferDesc Message;
	SECURITY_STATUS status;

	nla_encode_ts_credentials(nla);

	Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
	Buffers[1].BufferType = SECBUFFER_DATA; /* TSCredentials */
	sspi_SecBufferAlloc(&nla->authInfo, nla->ContextSizes.cbMaxSignature + nla->tsCredentials.cbBuffer);
	Buffers[0].cbBuffer = nla->ContextSizes.cbMaxSignature;
	Buffers[0].pvBuffer = nla->authInfo.pvBuffer;
	ZeroMemory(Buffers[0].pvBuffer, Buffers[0].cbBuffer);
	Buffers[1].cbBuffer = nla->tsCredentials.cbBuffer;
	Buffers[1].pvBuffer = &((BYTE*) nla->authInfo.pvBuffer)[Buffers[0].cbBuffer];
	CopyMemory(Buffers[1].pvBuffer, nla->tsCredentials.pvBuffer, Buffers[1].cbBuffer);

	Message.cBuffers = 2;
	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer) &Buffers;

	status = nla->table->EncryptMessage(&nla->context, 0, &Message, nla->sendSeqNum++);

	if (status != SEC_E_OK)
		return status;

	return SEC_E_OK;
}

SECURITY_STATUS nla_decrypt_ts_credentials(rdpNla* nla)
{
	int length;
	BYTE* buffer;
	ULONG pfQOP;
	SecBuffer Buffers[2];
	SecBufferDesc Message;
	SECURITY_STATUS status;
	Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
	Buffers[1].BufferType = SECBUFFER_DATA; /* TSCredentials */

	if (nla->authInfo.cbBuffer < 1)
	{
		WLog_ERR(TAG, "nla_decrypt_ts_credentials missing authInfo buffer");
		return SEC_E_INVALID_TOKEN;
	}

	length = nla->authInfo.cbBuffer;
	buffer = (BYTE*) malloc(length);

	if (!buffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	CopyMemory(buffer, nla->authInfo.pvBuffer, length);
	Buffers[0].cbBuffer = nla->ContextSizes.cbMaxSignature;
	Buffers[0].pvBuffer = buffer;
	Buffers[1].cbBuffer = length - nla->ContextSizes.cbMaxSignature;
	Buffers[1].pvBuffer = &buffer[nla->ContextSizes.cbMaxSignature];
	Message.cBuffers = 2;
	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer) &Buffers;
	status = nla->table->DecryptMessage(&nla->context, &Message, nla->recvSeqNum++, &pfQOP);

	if (status != SEC_E_OK)
		return status;

	nla_read_ts_credentials(nla, &Buffers[1]);
	free(buffer);

	return SEC_E_OK;
}

int nla_sizeof_nego_token(int length)
{
	length = ber_sizeof_octet_string(length);
	length += ber_sizeof_contextual_tag(length);
	return length;
}

int nla_sizeof_nego_tokens(int length)
{
	length = nla_sizeof_nego_token(length);
	length += ber_sizeof_sequence_tag(length);
	length += ber_sizeof_sequence_tag(length);
	length += ber_sizeof_contextual_tag(length);
	return length;
}

int nla_sizeof_pub_key_auth(int length)
{
	length = ber_sizeof_octet_string(length);
	length += ber_sizeof_contextual_tag(length);
	return length;
}

int nla_sizeof_auth_info(int length)
{
	length = ber_sizeof_octet_string(length);
	length += ber_sizeof_contextual_tag(length);
	return length;
}

int nla_sizeof_ts_request(int length)
{
	length += ber_sizeof_integer(2);
	length += ber_sizeof_contextual_tag(3);
	return length;
}

/**
 * Send CredSSP message.
 * @param credssp
 */

void nla_send(rdpNla* nla)
{
	wStream* s;
	int length;
	int ts_request_length;
	int nego_tokens_length;
	int pub_key_auth_length;
	int auth_info_length;

	nego_tokens_length = (nla->negoToken.cbBuffer > 0) ? nla_sizeof_nego_tokens(nla->negoToken.cbBuffer) : 0;
	pub_key_auth_length = (nla->pubKeyAuth.cbBuffer > 0) ? nla_sizeof_pub_key_auth(nla->pubKeyAuth.cbBuffer) : 0;
	auth_info_length = (nla->authInfo.cbBuffer > 0) ? nla_sizeof_auth_info(nla->authInfo.cbBuffer) : 0;
	length = nego_tokens_length + pub_key_auth_length + auth_info_length;
	ts_request_length = nla_sizeof_ts_request(length);

	s = Stream_New(NULL, ber_sizeof_sequence(ts_request_length));

	/* TSRequest */
	ber_write_sequence_tag(s, ts_request_length); /* SEQUENCE */
	/* [0] version */
	ber_write_contextual_tag(s, 0, 3, TRUE);
	ber_write_integer(s, 2); /* INTEGER */

	/* [1] negoTokens (NegoData) */
	if (nego_tokens_length > 0)
	{
		length = nego_tokens_length;
		length -= ber_write_contextual_tag(s, 1, ber_sizeof_sequence(ber_sizeof_sequence(ber_sizeof_sequence_octet_string(nla->negoToken.cbBuffer))), TRUE); /* NegoData */
		length -= ber_write_sequence_tag(s, ber_sizeof_sequence(ber_sizeof_sequence_octet_string(nla->negoToken.cbBuffer))); /* SEQUENCE OF NegoDataItem */
		length -= ber_write_sequence_tag(s, ber_sizeof_sequence_octet_string(nla->negoToken.cbBuffer)); /* NegoDataItem */
		length -= ber_write_sequence_octet_string(s, 0, (BYTE*) nla->negoToken.pvBuffer, nla->negoToken.cbBuffer);  /* OCTET STRING */
	}

	/* [2] authInfo (OCTET STRING) */
	if (auth_info_length > 0)
	{
		length = auth_info_length;
		length -= ber_write_sequence_octet_string(s, 2, nla->authInfo.pvBuffer, nla->authInfo.cbBuffer);
	}

	/* [3] pubKeyAuth (OCTET STRING) */
	if (pub_key_auth_length > 0)
	{
		length = pub_key_auth_length;
		length -= ber_write_sequence_octet_string(s, 3, nla->pubKeyAuth.pvBuffer, nla->pubKeyAuth.cbBuffer);
	}

	Stream_SealLength(s);
	transport_write(nla->transport, s);
	Stream_Free(s, TRUE);
}

int nla_decode_ts_request(rdpNla* nla, wStream* s)
{
	int length;
	UINT32 version;

	/* TSRequest */
	if (!ber_read_sequence_tag(s, &length) ||
			!ber_read_contextual_tag(s, 0, &length, TRUE) ||
			!ber_read_integer(s, &version))
	{
		Stream_Free(s, TRUE);
		return -1;
	}

	/* [1] negoTokens (NegoData) */
	if (ber_read_contextual_tag(s, 1, &length, TRUE) != FALSE)
	{
		if (!ber_read_sequence_tag(s, &length) || /* SEQUENCE OF NegoDataItem */
				!ber_read_sequence_tag(s, &length) || /* NegoDataItem */
				!ber_read_contextual_tag(s, 0, &length, TRUE) || /* [0] negoToken */
				!ber_read_octet_string_tag(s, &length) || /* OCTET STRING */
				((int) Stream_GetRemainingLength(s)) < length)
		{
			Stream_Free(s, TRUE);
			return -1;
		}

		sspi_SecBufferAlloc(&nla->negoToken, length);
		Stream_Read(s, nla->negoToken.pvBuffer, length);
		nla->negoToken.cbBuffer = length;
	}

	/* [2] authInfo (OCTET STRING) */
	if (ber_read_contextual_tag(s, 2, &length, TRUE) != FALSE)
	{
		if (!ber_read_octet_string_tag(s, &length) || /* OCTET STRING */
				((int) Stream_GetRemainingLength(s)) < length)
		{
			Stream_Free(s, TRUE);
			return -1;
		}

		sspi_SecBufferAlloc(&nla->authInfo, length);
		Stream_Read(s, nla->authInfo.pvBuffer, length);
		nla->authInfo.cbBuffer = length;
	}

	/* [3] pubKeyAuth (OCTET STRING) */
	if (ber_read_contextual_tag(s, 3, &length, TRUE) != FALSE)
	{
		if (!ber_read_octet_string_tag(s, &length) || /* OCTET STRING */
				((int) Stream_GetRemainingLength(s)) < length)
		{
			Stream_Free(s, TRUE);
			return -1;
		}

		sspi_SecBufferAlloc(&nla->pubKeyAuth, length);
		Stream_Read(s, nla->pubKeyAuth.pvBuffer, length);
		nla->pubKeyAuth.cbBuffer = length;
	}

	return 1;
}

int nla_recv_pdu(rdpNla* nla, wStream* s)
{
	if (nla_decode_ts_request(nla, s) < 1)
		return -1;

	if (nla_client_recv(nla) < 1)
		return -1;

	return 1;
}

int nla_recv(rdpNla* nla)
{
	wStream* s;
	int status;

	s = Stream_New(NULL, 4096);

	status = transport_read_pdu(nla->transport, s);

	if (status < 0)
	{
		WLog_ERR(TAG, "nla_recv() error: %d", status);
		Stream_Free(s, TRUE);
		return -1;
	}

	if (nla_recv_pdu(nla, s) < 1)
		return -1;

	Stream_Free(s, TRUE);
	return 1;
}

void nla_buffer_print(rdpNla* nla)
{
	if (nla->negoToken.cbBuffer > 0)
	{
		WLog_DBG(TAG, "NLA.negoToken (length = %d):", (int) nla->negoToken.cbBuffer);
		winpr_HexDump(TAG, WLOG_DEBUG, nla->negoToken.pvBuffer, nla->negoToken.cbBuffer);
	}

	if (nla->pubKeyAuth.cbBuffer > 0)
	{
		WLog_DBG(TAG, "NLA.pubKeyAuth (length = %d):", (int) nla->pubKeyAuth.cbBuffer);
		winpr_HexDump(TAG, WLOG_DEBUG, nla->pubKeyAuth.pvBuffer, nla->pubKeyAuth.cbBuffer);
	}

	if (nla->authInfo.cbBuffer > 0)
	{
		WLog_DBG(TAG, "NLA.authInfo (length = %d):", (int) nla->authInfo.cbBuffer);
		winpr_HexDump(TAG, WLOG_DEBUG, nla->authInfo.pvBuffer, nla->authInfo.cbBuffer);
	}
}

void nla_buffer_free(rdpNla* nla)
{
	sspi_SecBufferFree(&nla->negoToken);
	sspi_SecBufferFree(&nla->pubKeyAuth);
	sspi_SecBufferFree(&nla->authInfo);
}

LPTSTR nla_make_spn(const char* ServiceClass, const char* hostname)
{
	DWORD status;
	DWORD SpnLength;
	LPTSTR hostnameX = NULL;
	LPTSTR ServiceClassX = NULL;
	LPTSTR ServicePrincipalName = NULL;
#ifdef UNICODE
	ConvertToUnicode(CP_UTF8, 0, hostname, -1, &hostnameX, 0);
	ConvertToUnicode(CP_UTF8, 0, ServiceClass, -1, &ServiceClassX, 0);
#else
	hostnameX = _strdup(hostname);
	ServiceClassX = _strdup(ServiceClass);
#endif

	if (!ServiceClass)
	{
		ServicePrincipalName = (LPTSTR) _tcsdup(hostnameX);
		free(ServiceClassX);
		free(hostnameX);
		return ServicePrincipalName;
	}

	SpnLength = 0;
	status = DsMakeSpn(ServiceClassX, hostnameX, NULL, 0, NULL, &SpnLength, NULL);

	if (status != ERROR_BUFFER_OVERFLOW)
	{
		free(ServiceClassX);
		free(hostnameX);
		return NULL;
	}

	ServicePrincipalName = (LPTSTR) malloc(SpnLength * sizeof(TCHAR));

	if (!ServicePrincipalName)
		return NULL;

	status = DsMakeSpn(ServiceClassX, hostnameX, NULL, 0, NULL, &SpnLength, ServicePrincipalName);

	if (status != ERROR_SUCCESS)
	{
		free(ServicePrincipalName);
		free(ServiceClassX);
		free(hostnameX);
		return NULL;
	}

	free(ServiceClassX);
	free(hostnameX);
	return ServicePrincipalName;
}

/**
 * Create new CredSSP state machine.
 * @param transport
 * @return new CredSSP state machine.
 */

rdpNla* nla_new(freerdp* instance, rdpTransport* transport, rdpSettings* settings)
{
	rdpNla* nla = (rdpNla*) calloc(1, sizeof(rdpNla));

	if (nla)
	{
		HKEY hKey;
		LONG status;
		DWORD dwType;
		DWORD dwSize;
		nla->instance = instance;
		nla->settings = settings;
		nla->server = settings->ServerMode;
		nla->transport = transport;
		nla->sendSeqNum = 0;
		nla->recvSeqNum = 0;
		ZeroMemory(&nla->negoToken, sizeof(SecBuffer));
		ZeroMemory(&nla->pubKeyAuth, sizeof(SecBuffer));
		ZeroMemory(&nla->authInfo, sizeof(SecBuffer));
		SecInvalidateHandle(&nla->context);

		if (nla->server)
		{
			status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\FreeRDP\\Server"),
								  0, KEY_READ | KEY_WOW64_64KEY, &hKey);

			if (status == ERROR_SUCCESS)
			{
				status = RegQueryValueEx(hKey, _T("SspiModule"), NULL, &dwType, NULL, &dwSize);

				if (status == ERROR_SUCCESS)
				{
					nla->SspiModule = (LPTSTR) malloc(dwSize + sizeof(TCHAR));
					status = RegQueryValueEx(hKey, _T("SspiModule"), NULL, &dwType,
											 (BYTE*) nla->SspiModule, &dwSize);

					if (status == ERROR_SUCCESS)
					{
						WLog_INFO(TAG, "Using SSPI Module: %s", nla->SspiModule);
						RegCloseKey(hKey);
					}
				}
			}
		}
	}

	return nla;
}

/**
 * Free CredSSP state machine.
 * @param credssp
 */

void nla_free(rdpNla* nla)
{
	if (!nla)
		return;

	if (nla->table)
		nla->table->DeleteSecurityContext(&nla->context);

	sspi_SecBufferFree(&nla->PublicKey);
	sspi_SecBufferFree(&nla->tsCredentials);

	free(nla->ServicePrincipalName);
	free(nla->identity.User);
	free(nla->identity.Domain);
	free(nla->identity.Password);

	free(nla);
}

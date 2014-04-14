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

#include <freerdp/crypto/tls.h>

#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/print.h>
#include <winpr/tchar.h>
#include <winpr/dsparse.h>
#include <winpr/library.h>
#include <winpr/registry.h>

#include "nla.h"

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

#ifdef WITH_DEBUG_NLA
#define WITH_DEBUG_CREDSSP
#endif

#ifdef WITH_NATIVE_SSPI
#define NLA_PKG_NAME	NTLMSP_NAME
#else
#define NLA_PKG_NAME	NTLMSP_NAME
#endif

#define TERMSRV_SPN_PREFIX	"TERMSRV/"

void credssp_send(rdpCredssp* credssp);
int credssp_recv(rdpCredssp* credssp);
void credssp_buffer_print(rdpCredssp* credssp);
void credssp_buffer_free(rdpCredssp* credssp);
SECURITY_STATUS credssp_encrypt_public_key_echo(rdpCredssp* credssp);
SECURITY_STATUS credssp_decrypt_public_key_echo(rdpCredssp* credssp);
SECURITY_STATUS credssp_encrypt_ts_credentials(rdpCredssp* credssp);
SECURITY_STATUS credssp_decrypt_ts_credentials(rdpCredssp* credssp);

#define ber_sizeof_sequence_octet_string(length) ber_sizeof_contextual_tag(ber_sizeof_octet_string(length)) + ber_sizeof_octet_string(length)
#define ber_write_sequence_octet_string(stream, context, value, length) ber_write_contextual_tag(stream, context, ber_sizeof_octet_string(length), TRUE) + ber_write_octet_string(stream, value, length)

/**
 * Initialize NTLMSSP authentication module (client).
 * @param credssp
 */

int credssp_ntlm_client_init(rdpCredssp* credssp)
{
	char* spn;
	int length;
	BOOL PromptPassword;
	rdpTls* tls = NULL;
	freerdp* instance;
	rdpSettings* settings;

	PromptPassword = FALSE;
	settings = credssp->settings;
	instance = (freerdp*) settings->instance;

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
				connectErrorCode = CANCELEDBYUSER;
				freerdp_set_last_error(instance->context, FREERDP_ERROR_CONNECT_CANCELLED);
				return 0;
			}

		}
	}

	sspi_SetAuthIdentity(&(credssp->identity), settings->Username, settings->Domain, settings->Password);

#ifndef _WIN32
	{
		SEC_WINNT_AUTH_IDENTITY* identity = &(credssp->identity);

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

#ifdef WITH_DEBUG_NLA
	_tprintf(_T("User: %s Domain: %s Password: %s\n"),
		(char*) credssp->identity.User, (char*) credssp->identity.Domain, (char*) credssp->identity.Password);
#endif

	if (credssp->transport->layer == TRANSPORT_LAYER_TLS)
	{
		tls = credssp->transport->TlsIn;
	}
	else if (credssp->transport->layer == TRANSPORT_LAYER_TSG_TLS)
	{
		tls = credssp->transport->TsgTls;
	}
	else
	{
		fprintf(stderr, "Unknown NLA transport layer\n");
		return 0;
	}

	sspi_SecBufferAlloc(&credssp->PublicKey, tls->PublicKeyLength);
	CopyMemory(credssp->PublicKey.pvBuffer, tls->PublicKey, tls->PublicKeyLength);

	length = sizeof(TERMSRV_SPN_PREFIX) + strlen(settings->ServerHostname);

	spn = (SEC_CHAR*) malloc(length + 1);
	sprintf(spn, "%s%s", TERMSRV_SPN_PREFIX, settings->ServerHostname);

#ifdef UNICODE
	credssp->ServicePrincipalName = (LPTSTR) malloc(length * 2 + 2);
	MultiByteToWideChar(CP_UTF8, 0, spn, length,
		(LPWSTR) credssp->ServicePrincipalName, length);
	free(spn);
#else
	credssp->ServicePrincipalName = spn;
#endif

	return 1;
}

/**
 * Initialize NTLMSSP authentication module (server).
 * @param credssp
 */

int credssp_ntlm_server_init(rdpCredssp* credssp)
{
	freerdp* instance;
	rdpSettings* settings = credssp->settings;
	instance = (freerdp*) settings->instance;

	sspi_SecBufferAlloc(&credssp->PublicKey, credssp->transport->TlsIn->PublicKeyLength);
	CopyMemory(credssp->PublicKey.pvBuffer, credssp->transport->TlsIn->PublicKey, credssp->transport->TlsIn->PublicKeyLength);

	return 1;
}

int credssp_client_authenticate(rdpCredssp* credssp)
{
	ULONG cbMaxToken;
	ULONG fContextReq;
	ULONG pfContextAttr;
	SECURITY_STATUS status;
	CredHandle credentials;
	TimeStamp expiration;
	PSecPkgInfo pPackageInfo;
	SecBuffer input_buffer;
	SecBuffer output_buffer;
	SecBufferDesc input_buffer_desc;
	SecBufferDesc output_buffer_desc;
	BOOL have_context;
	BOOL have_input_buffer;
	BOOL have_pub_key_auth;

	sspi_GlobalInit();

	if (credssp_ntlm_client_init(credssp) == 0)
		return 0;

#ifdef WITH_NATIVE_SSPI
	{
		HMODULE hSSPI;
		INIT_SECURITY_INTERFACE InitSecurityInterface;
		PSecurityFunctionTable pSecurityInterface = NULL;

		hSSPI = LoadLibrary(_T("secur32.dll"));

#ifdef UNICODE
		InitSecurityInterface = (INIT_SECURITY_INTERFACE) GetProcAddress(hSSPI, "InitSecurityInterfaceW");
#else
		InitSecurityInterface = (INIT_SECURITY_INTERFACE) GetProcAddress(hSSPI, "InitSecurityInterfaceA");
#endif
		credssp->table = (*InitSecurityInterface)();
	}
#else
	credssp->table = InitSecurityInterface();
#endif

	status = credssp->table->QuerySecurityPackageInfo(NLA_PKG_NAME, &pPackageInfo);

	if (status != SEC_E_OK)
	{
		fprintf(stderr, "QuerySecurityPackageInfo status: 0x%08X\n", status);
		return 0;
	}

	cbMaxToken = pPackageInfo->cbMaxToken;

	status = credssp->table->AcquireCredentialsHandle(NULL, NLA_PKG_NAME,
			SECPKG_CRED_OUTBOUND, NULL, &credssp->identity, NULL, NULL, &credentials, &expiration);

	if (status != SEC_E_OK)
	{
		fprintf(stderr, "AcquireCredentialsHandle status: 0x%08X\n", status);
		return 0;
	}

	have_context = FALSE;
	have_input_buffer = FALSE;
	have_pub_key_auth = FALSE;
	ZeroMemory(&input_buffer, sizeof(SecBuffer));
	ZeroMemory(&output_buffer, sizeof(SecBuffer));
	ZeroMemory(&credssp->ContextSizes, sizeof(SecPkgContext_Sizes));

	/*
	 * from tspkg.dll: 0x00000132
	 * ISC_REQ_MUTUAL_AUTH
	 * ISC_REQ_CONFIDENTIALITY
	 * ISC_REQ_USE_SESSION_KEY
	 * ISC_REQ_ALLOCATE_MEMORY
	 */

	fContextReq = ISC_REQ_MUTUAL_AUTH | ISC_REQ_CONFIDENTIALITY | ISC_REQ_USE_SESSION_KEY;

	while (TRUE)
	{
		output_buffer_desc.ulVersion = SECBUFFER_VERSION;
		output_buffer_desc.cBuffers = 1;
		output_buffer_desc.pBuffers = &output_buffer;
		output_buffer.BufferType = SECBUFFER_TOKEN;
		output_buffer.cbBuffer = cbMaxToken;
		output_buffer.pvBuffer = malloc(output_buffer.cbBuffer);

		status = credssp->table->InitializeSecurityContext(&credentials,
				(have_context) ? &credssp->context : NULL,
				credssp->ServicePrincipalName, fContextReq, 0,
				SECURITY_NATIVE_DREP, (have_input_buffer) ? &input_buffer_desc : NULL,
				0, &credssp->context, &output_buffer_desc, &pfContextAttr, &expiration);

		if (have_input_buffer && (input_buffer.pvBuffer != NULL))
		{
			free(input_buffer.pvBuffer);
			input_buffer.pvBuffer = NULL;
		}

		if ((status == SEC_I_COMPLETE_AND_CONTINUE) || (status == SEC_I_COMPLETE_NEEDED) || (status == SEC_E_OK))
		{
			if (credssp->table->CompleteAuthToken != NULL)
				credssp->table->CompleteAuthToken(&credssp->context, &output_buffer_desc);

			have_pub_key_auth = TRUE;

			if (credssp->table->QueryContextAttributes(&credssp->context, SECPKG_ATTR_SIZES, &credssp->ContextSizes) != SEC_E_OK)
			{
				fprintf(stderr, "QueryContextAttributes SECPKG_ATTR_SIZES failure\n");
				return 0;
			}

			credssp_encrypt_public_key_echo(credssp);

			if (status == SEC_I_COMPLETE_NEEDED)
				status = SEC_E_OK;
			else if (status == SEC_I_COMPLETE_AND_CONTINUE)
				status = SEC_I_CONTINUE_NEEDED;
		}

		/* send authentication token to server */

		if (output_buffer.cbBuffer > 0)
		{
			credssp->negoToken.pvBuffer = output_buffer.pvBuffer;
			credssp->negoToken.cbBuffer = output_buffer.cbBuffer;

#ifdef WITH_DEBUG_CREDSSP
			fprintf(stderr, "Sending Authentication Token\n");
			winpr_HexDump(credssp->negoToken.pvBuffer, credssp->negoToken.cbBuffer);
#endif

			credssp_send(credssp);
			credssp_buffer_free(credssp);
		}

		if (status != SEC_I_CONTINUE_NEEDED)
			break;

		/* receive server response and place in input buffer */

		input_buffer_desc.ulVersion = SECBUFFER_VERSION;
		input_buffer_desc.cBuffers = 1;
		input_buffer_desc.pBuffers = &input_buffer;
		input_buffer.BufferType = SECBUFFER_TOKEN;

		if (credssp_recv(credssp) < 0)
			return -1;

#ifdef WITH_DEBUG_CREDSSP
		fprintf(stderr, "Receiving Authentication Token (%d)\n", (int) credssp->negoToken.cbBuffer);
		winpr_HexDump(credssp->negoToken.pvBuffer, credssp->negoToken.cbBuffer);
#endif

		input_buffer.pvBuffer = credssp->negoToken.pvBuffer;
		input_buffer.cbBuffer = credssp->negoToken.cbBuffer;

		have_input_buffer = TRUE;
		have_context = TRUE;
	}

	/* Encrypted Public Key +1 */
	if (credssp_recv(credssp) < 0)
		return -1;

	/* Verify Server Public Key Echo */

	status = credssp_decrypt_public_key_echo(credssp);
	credssp_buffer_free(credssp);

	if (status != SEC_E_OK)
	{
		fprintf(stderr, "Could not verify public key echo!\n");
		return -1;
	}

	/* Send encrypted credentials */

	status = credssp_encrypt_ts_credentials(credssp);

	if (status != SEC_E_OK)
	{
		fprintf(stderr, "credssp_encrypt_ts_credentials status: 0x%08X\n", status);
		return 0;
	}

	credssp_send(credssp);
	credssp_buffer_free(credssp);

	/* Free resources */

	credssp->table->FreeCredentialsHandle(&credentials);
	credssp->table->FreeContextBuffer(pPackageInfo);

	return 1;
}

/**
 * Authenticate with client using CredSSP (server).
 * @param credssp
 * @return 1 if authentication is successful
 */

int credssp_server_authenticate(rdpCredssp* credssp)
{
	UINT32 cbMaxToken;
	ULONG fContextReq;
	ULONG pfContextAttr;
	SECURITY_STATUS status;
	CredHandle credentials;
	TimeStamp expiration;
	PSecPkgInfo pPackageInfo;
	SecBuffer input_buffer;
	SecBuffer output_buffer;
	SecBufferDesc input_buffer_desc;
	SecBufferDesc output_buffer_desc;
	BOOL have_context;
	BOOL have_input_buffer;
	BOOL have_pub_key_auth;

	sspi_GlobalInit();

	if (credssp_ntlm_server_init(credssp) == 0)
		return 0;

#ifdef WITH_NATIVE_SSPI
	if (!credssp->SspiModule)
		credssp->SspiModule = _tcsdup(_T("secur32.dll"));
#endif

	if (credssp->SspiModule)
	{
		HMODULE hSSPI;
		INIT_SECURITY_INTERFACE pInitSecurityInterface;

		hSSPI = LoadLibrary(credssp->SspiModule);

		if (!hSSPI)
		{
			_tprintf(_T("Failed to load SSPI module: %s\n"), credssp->SspiModule);
			return 0;
		}

#ifdef UNICODE
		pInitSecurityInterface = (INIT_SECURITY_INTERFACE) GetProcAddress(hSSPI, "InitSecurityInterfaceW");
#else
		pInitSecurityInterface = (INIT_SECURITY_INTERFACE) GetProcAddress(hSSPI, "InitSecurityInterfaceA");
#endif

		credssp->table = (*pInitSecurityInterface)();
	}
#ifndef WITH_NATIVE_SSPI
	else
	{
		credssp->table = InitSecurityInterface();
	}
#endif

	status = credssp->table->QuerySecurityPackageInfo(NLA_PKG_NAME, &pPackageInfo);

	if (status != SEC_E_OK)
	{
		fprintf(stderr, "QuerySecurityPackageInfo status: 0x%08X\n", status);
		return 0;
	}

	cbMaxToken = pPackageInfo->cbMaxToken;

	status = credssp->table->AcquireCredentialsHandle(NULL, NLA_PKG_NAME,
			SECPKG_CRED_INBOUND, NULL, NULL, NULL, NULL, &credentials, &expiration);

	if (status != SEC_E_OK)
	{
		fprintf(stderr, "AcquireCredentialsHandle status: 0x%08X\n", status);
		return 0;
	}

	have_context = FALSE;
	have_input_buffer = FALSE;
	have_pub_key_auth = FALSE;
	ZeroMemory(&input_buffer, sizeof(SecBuffer));
	ZeroMemory(&output_buffer, sizeof(SecBuffer));
	ZeroMemory(&input_buffer_desc, sizeof(SecBufferDesc));
	ZeroMemory(&output_buffer_desc, sizeof(SecBufferDesc));
	ZeroMemory(&credssp->ContextSizes, sizeof(SecPkgContext_Sizes));

	/*
	 * from tspkg.dll: 0x00000112
	 * ASC_REQ_MUTUAL_AUTH
	 * ASC_REQ_CONFIDENTIALITY
	 * ASC_REQ_ALLOCATE_MEMORY
	 */

	fContextReq = 0;
	fContextReq |= ASC_REQ_MUTUAL_AUTH;
	fContextReq |= ASC_REQ_CONFIDENTIALITY;

	fContextReq |= ASC_REQ_CONNECTION;
	fContextReq |= ASC_REQ_USE_SESSION_KEY;

	fContextReq |= ASC_REQ_REPLAY_DETECT;
	fContextReq |= ASC_REQ_SEQUENCE_DETECT;

	fContextReq |= ASC_REQ_EXTENDED_ERROR;

	while (TRUE)
	{
		input_buffer_desc.ulVersion = SECBUFFER_VERSION;
		input_buffer_desc.cBuffers = 1;
		input_buffer_desc.pBuffers = &input_buffer;
		input_buffer.BufferType = SECBUFFER_TOKEN;

		/* receive authentication token */

		input_buffer_desc.ulVersion = SECBUFFER_VERSION;
		input_buffer_desc.cBuffers = 1;
		input_buffer_desc.pBuffers = &input_buffer;
		input_buffer.BufferType = SECBUFFER_TOKEN;

		if (credssp_recv(credssp) < 0)
			return -1;

#ifdef WITH_DEBUG_CREDSSP
		fprintf(stderr, "Receiving Authentication Token\n");
		credssp_buffer_print(credssp);
#endif

		input_buffer.pvBuffer = credssp->negoToken.pvBuffer;
		input_buffer.cbBuffer = credssp->negoToken.cbBuffer;

		if (credssp->negoToken.cbBuffer < 1)
		{
			fprintf(stderr, "CredSSP: invalid negoToken!\n");
			return -1;
		}

		output_buffer_desc.ulVersion = SECBUFFER_VERSION;
		output_buffer_desc.cBuffers = 1;
		output_buffer_desc.pBuffers = &output_buffer;
		output_buffer.BufferType = SECBUFFER_TOKEN;
		output_buffer.cbBuffer = cbMaxToken;
		output_buffer.pvBuffer = malloc(output_buffer.cbBuffer);

		status = credssp->table->AcceptSecurityContext(&credentials,
			have_context? &credssp->context: NULL,
			&input_buffer_desc, fContextReq, SECURITY_NATIVE_DREP, &credssp->context,
			&output_buffer_desc, &pfContextAttr, &expiration);

		credssp->negoToken.pvBuffer = output_buffer.pvBuffer;
		credssp->negoToken.cbBuffer = output_buffer.cbBuffer;

		if ((status == SEC_I_COMPLETE_AND_CONTINUE) || (status == SEC_I_COMPLETE_NEEDED))
		{
			if (credssp->table->CompleteAuthToken != NULL)
				credssp->table->CompleteAuthToken(&credssp->context, &output_buffer_desc);

			if (status == SEC_I_COMPLETE_NEEDED)
				status = SEC_E_OK;
			else if (status == SEC_I_COMPLETE_AND_CONTINUE)
				status = SEC_I_CONTINUE_NEEDED;
		}

		if (status == SEC_E_OK)
		{
			have_pub_key_auth = TRUE;

			if (credssp->table->QueryContextAttributes(&credssp->context, SECPKG_ATTR_SIZES, &credssp->ContextSizes) != SEC_E_OK)
			{
				fprintf(stderr, "QueryContextAttributes SECPKG_ATTR_SIZES failure\n");
				return 0;
			}

			if (credssp_decrypt_public_key_echo(credssp) != SEC_E_OK)
			{
				fprintf(stderr, "Error: could not verify client's public key echo\n");
				return -1;
			}

			sspi_SecBufferFree(&credssp->negoToken);
			credssp->negoToken.pvBuffer = NULL;
			credssp->negoToken.cbBuffer = 0;

			credssp_encrypt_public_key_echo(credssp);
		}

		if ((status != SEC_E_OK) && (status != SEC_I_CONTINUE_NEEDED))
		{
			fprintf(stderr, "AcceptSecurityContext status: 0x%08X\n", status);
			return -1; /* Access Denied */
		}

		/* send authentication token */

#ifdef WITH_DEBUG_CREDSSP
		fprintf(stderr, "Sending Authentication Token\n");
		credssp_buffer_print(credssp);
#endif

		credssp_send(credssp);
		credssp_buffer_free(credssp);

		if (status != SEC_I_CONTINUE_NEEDED)
			break;

		have_context = TRUE;
	}

	/* Receive encrypted credentials */

	if (credssp_recv(credssp) < 0)
		return -1;

	if (credssp_decrypt_ts_credentials(credssp) != SEC_E_OK)
	{
		fprintf(stderr, "Could not decrypt TSCredentials status: 0x%08X\n", status);
		return 0;
	}

	if (status != SEC_E_OK)
	{
		fprintf(stderr, "AcceptSecurityContext status: 0x%08X\n", status);
		return 0;
	}

	status = credssp->table->ImpersonateSecurityContext(&credssp->context);

	if (status != SEC_E_OK)
	{
		fprintf(stderr, "ImpersonateSecurityContext status: 0x%08X\n", status);
		return 0;
	}
	else
	{
		status = credssp->table->RevertSecurityContext(&credssp->context);

		if (status != SEC_E_OK)
		{
			fprintf(stderr, "RevertSecurityContext status: 0x%08X\n", status);
			return 0;
		}
	}

	credssp->table->FreeContextBuffer(pPackageInfo);

	return 1;
}

/**
 * Authenticate using CredSSP.
 * @param credssp
 * @return 1 if authentication is successful
 */

int credssp_authenticate(rdpCredssp* credssp)
{
	if (credssp->server)
		return credssp_server_authenticate(credssp);
	else
		return credssp_client_authenticate(credssp);
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

SECURITY_STATUS credssp_encrypt_public_key_echo(rdpCredssp* credssp)
{
	SecBuffer Buffers[2];
	SecBufferDesc Message;
	SECURITY_STATUS status;
	int public_key_length;

	public_key_length = credssp->PublicKey.cbBuffer;

	Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
	Buffers[1].BufferType = SECBUFFER_DATA; /* TLS Public Key */

	sspi_SecBufferAlloc(&credssp->pubKeyAuth, credssp->ContextSizes.cbMaxSignature + public_key_length);

	Buffers[0].cbBuffer = credssp->ContextSizes.cbMaxSignature;
	Buffers[0].pvBuffer = credssp->pubKeyAuth.pvBuffer;

	Buffers[1].cbBuffer = public_key_length;
	Buffers[1].pvBuffer = ((BYTE*) credssp->pubKeyAuth.pvBuffer) + credssp->ContextSizes.cbMaxSignature;
	CopyMemory(Buffers[1].pvBuffer, credssp->PublicKey.pvBuffer, Buffers[1].cbBuffer);

	if (credssp->server)
	{
		/* server echos the public key +1 */
		ap_integer_increment_le((BYTE*) Buffers[1].pvBuffer, Buffers[1].cbBuffer);
	}

	Message.cBuffers = 2;
	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer) &Buffers;

	status = credssp->table->EncryptMessage(&credssp->context, 0, &Message, credssp->send_seq_num++);

	if (status != SEC_E_OK)
	{
		fprintf(stderr, "EncryptMessage status: 0x%08X\n", status);
		return status;
	}

	return status;
}

SECURITY_STATUS credssp_decrypt_public_key_echo(rdpCredssp* credssp)
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

	if (credssp->PublicKey.cbBuffer + credssp->ContextSizes.cbMaxSignature != credssp->pubKeyAuth.cbBuffer)
	{
		fprintf(stderr, "unexpected pubKeyAuth buffer size:%d\n", (int) credssp->pubKeyAuth.cbBuffer);
		return SEC_E_INVALID_TOKEN;
	}

	length = credssp->pubKeyAuth.cbBuffer;
	buffer = (BYTE*) malloc(length);
	CopyMemory(buffer, credssp->pubKeyAuth.pvBuffer, length);

	public_key_length = credssp->PublicKey.cbBuffer;

	Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
	Buffers[1].BufferType = SECBUFFER_DATA; /* Encrypted TLS Public Key */

	Buffers[0].cbBuffer = credssp->ContextSizes.cbMaxSignature;
	Buffers[0].pvBuffer = buffer;

	Buffers[1].cbBuffer = length - credssp->ContextSizes.cbMaxSignature;
	Buffers[1].pvBuffer = buffer + credssp->ContextSizes.cbMaxSignature;

	Message.cBuffers = 2;
	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer) &Buffers;

	status = credssp->table->DecryptMessage(&credssp->context, &Message, credssp->recv_seq_num++, &pfQOP);

	if (status != SEC_E_OK)
	{
		fprintf(stderr, "DecryptMessage failure: 0x%08X\n", status);
		return status;
	}

	public_key1 = (BYTE*) credssp->PublicKey.pvBuffer;
	public_key2 = (BYTE*) Buffers[1].pvBuffer;

	if (!credssp->server)
	{
		/* server echos the public key +1 */
		ap_integer_decrement_le(public_key2, public_key_length);
	}

	if (memcmp(public_key1, public_key2, public_key_length) != 0)
	{
		fprintf(stderr, "Could not verify server's public key echo\n");

		fprintf(stderr, "Expected (length = %d):\n", public_key_length);
		winpr_HexDump(public_key1, public_key_length);

		fprintf(stderr, "Actual (length = %d):\n", public_key_length);
		winpr_HexDump(public_key2, public_key_length);

		return SEC_E_MESSAGE_ALTERED; /* DO NOT SEND CREDENTIALS! */
	}

	free(buffer);

	return SEC_E_OK;
}

int credssp_sizeof_ts_password_creds(rdpCredssp* credssp)
{
	int length = 0;

	length += ber_sizeof_sequence_octet_string(credssp->identity.DomainLength * 2);
	length += ber_sizeof_sequence_octet_string(credssp->identity.UserLength * 2);
	length += ber_sizeof_sequence_octet_string(credssp->identity.PasswordLength * 2);

	return length;
}

void credssp_read_ts_password_creds(rdpCredssp* credssp, wStream* s)
{
	int length;

	/* TSPasswordCreds (SEQUENCE) */
	ber_read_sequence_tag(s, &length);

	/* [0] domainName (OCTET STRING) */
	ber_read_contextual_tag(s, 0, &length, TRUE);
	ber_read_octet_string_tag(s, &length);
	credssp->identity.DomainLength = (UINT32) length;
	credssp->identity.Domain = (UINT16*) malloc(length);
	CopyMemory(credssp->identity.Domain, Stream_Pointer(s), credssp->identity.DomainLength);
	Stream_Seek(s, credssp->identity.DomainLength);
	credssp->identity.DomainLength /= 2;

	/* [1] userName (OCTET STRING) */
	ber_read_contextual_tag(s, 1, &length, TRUE);
	ber_read_octet_string_tag(s, &length);
	credssp->identity.UserLength = (UINT32) length;
	credssp->identity.User = (UINT16*) malloc(length);
	CopyMemory(credssp->identity.User, Stream_Pointer(s), credssp->identity.UserLength);
	Stream_Seek(s, credssp->identity.UserLength);
	credssp->identity.UserLength /= 2;

	/* [2] password (OCTET STRING) */
	ber_read_contextual_tag(s, 2, &length, TRUE);
	ber_read_octet_string_tag(s, &length);
	credssp->identity.PasswordLength = (UINT32) length;
	credssp->identity.Password = (UINT16*) malloc(length);
	CopyMemory(credssp->identity.Password, Stream_Pointer(s), credssp->identity.PasswordLength);
	Stream_Seek(s, credssp->identity.PasswordLength);
	credssp->identity.PasswordLength /= 2;

	credssp->identity.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
}

int credssp_write_ts_password_creds(rdpCredssp* credssp, wStream* s)
{
	int size = 0;
	int innerSize = credssp_sizeof_ts_password_creds(credssp);

	/* TSPasswordCreds (SEQUENCE) */

	size += ber_write_sequence_tag(s, innerSize);

	/* [0] domainName (OCTET STRING) */
	size += ber_write_sequence_octet_string(s, 0, (BYTE*) credssp->identity.Domain, credssp->identity.DomainLength * 2);

	/* [1] userName (OCTET STRING) */
	size += ber_write_sequence_octet_string(s, 1, (BYTE*) credssp->identity.User, credssp->identity.UserLength * 2);

	/* [2] password (OCTET STRING) */
	size += ber_write_sequence_octet_string(s, 2, (BYTE*) credssp->identity.Password, credssp->identity.PasswordLength * 2);

	return size;
}

int credssp_sizeof_ts_credentials(rdpCredssp* credssp)
{
	int size = 0;

	size += ber_sizeof_integer(1);
	size += ber_sizeof_contextual_tag(ber_sizeof_integer(1));
	size += ber_sizeof_sequence_octet_string(ber_sizeof_sequence(credssp_sizeof_ts_password_creds(credssp)));

	return size;
}

void credssp_read_ts_credentials(rdpCredssp* credssp, PSecBuffer ts_credentials)
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

	credssp_read_ts_password_creds(credssp, s);

	Stream_Free(s, FALSE);
}

int credssp_write_ts_credentials(rdpCredssp* credssp, wStream* s)
{
	int size = 0;
	int innerSize = credssp_sizeof_ts_credentials(credssp);
	int passwordSize;

	/* TSCredentials (SEQUENCE) */
	size += ber_write_sequence_tag(s, innerSize);

	/* [0] credType (INTEGER) */
	size += ber_write_contextual_tag(s, 0, ber_sizeof_integer(1), TRUE);
	size += ber_write_integer(s, 1);

	/* [1] credentials (OCTET STRING) */

	passwordSize = ber_sizeof_sequence(credssp_sizeof_ts_password_creds(credssp));

	size += ber_write_contextual_tag(s, 1, ber_sizeof_octet_string(passwordSize), TRUE);
	size += ber_write_octet_string_tag(s, passwordSize);
	size += credssp_write_ts_password_creds(credssp, s);

	return size;
}

/**
 * Encode TSCredentials structure.
 * @param credssp
 */

void credssp_encode_ts_credentials(rdpCredssp* credssp)
{
	wStream* s;
	int length;
	int DomainLength;
	int UserLength;
	int PasswordLength;

	DomainLength = credssp->identity.DomainLength;
	UserLength = credssp->identity.UserLength;
	PasswordLength = credssp->identity.PasswordLength;

	if (credssp->settings->DisableCredentialsDelegation)
	{
		credssp->identity.DomainLength = 0;
		credssp->identity.UserLength = 0;
		credssp->identity.PasswordLength = 0;
	}

	length = ber_sizeof_sequence(credssp_sizeof_ts_credentials(credssp));
	sspi_SecBufferAlloc(&credssp->ts_credentials, length);

	s = Stream_New((BYTE*) credssp->ts_credentials.pvBuffer, length);
	credssp_write_ts_credentials(credssp, s);

	if (credssp->settings->DisableCredentialsDelegation)
	{
		credssp->identity.DomainLength = DomainLength;
		credssp->identity.UserLength = UserLength;
		credssp->identity.PasswordLength = PasswordLength;
	}

	Stream_Free(s, FALSE);
}

SECURITY_STATUS credssp_encrypt_ts_credentials(rdpCredssp* credssp)
{
	SecBuffer Buffers[2];
	SecBufferDesc Message;
	SECURITY_STATUS status;

	credssp_encode_ts_credentials(credssp);

	Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
	Buffers[1].BufferType = SECBUFFER_DATA; /* TSCredentials */

	sspi_SecBufferAlloc(&credssp->authInfo, credssp->ContextSizes.cbMaxSignature + credssp->ts_credentials.cbBuffer);

	Buffers[0].cbBuffer = credssp->ContextSizes.cbMaxSignature;
	Buffers[0].pvBuffer = credssp->authInfo.pvBuffer;
	ZeroMemory(Buffers[0].pvBuffer, Buffers[0].cbBuffer);

	Buffers[1].cbBuffer = credssp->ts_credentials.cbBuffer;
	Buffers[1].pvBuffer = &((BYTE*) credssp->authInfo.pvBuffer)[Buffers[0].cbBuffer];
	CopyMemory(Buffers[1].pvBuffer, credssp->ts_credentials.pvBuffer, Buffers[1].cbBuffer);

	Message.cBuffers = 2;
	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer) &Buffers;

	status = credssp->table->EncryptMessage(&credssp->context, 0, &Message, credssp->send_seq_num++);

	if (status != SEC_E_OK)
		return status;

	return SEC_E_OK;
}

SECURITY_STATUS credssp_decrypt_ts_credentials(rdpCredssp* credssp)
{
	int length;
	BYTE* buffer;
	ULONG pfQOP;
	SecBuffer Buffers[2];
	SecBufferDesc Message;
	SECURITY_STATUS status;

	Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
	Buffers[1].BufferType = SECBUFFER_DATA; /* TSCredentials */

	if (credssp->authInfo.cbBuffer < 1)
	{
		fprintf(stderr, "credssp_decrypt_ts_credentials missing authInfo buffer\n");
		return SEC_E_INVALID_TOKEN;
	}

	length = credssp->authInfo.cbBuffer;
	buffer = (BYTE*) malloc(length);
	CopyMemory(buffer, credssp->authInfo.pvBuffer, length);

	Buffers[0].cbBuffer = credssp->ContextSizes.cbMaxSignature;
	Buffers[0].pvBuffer = buffer;

	Buffers[1].cbBuffer = length - credssp->ContextSizes.cbMaxSignature;
	Buffers[1].pvBuffer = &buffer[credssp->ContextSizes.cbMaxSignature];

	Message.cBuffers = 2;
	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer) &Buffers;

	status = credssp->table->DecryptMessage(&credssp->context, &Message, credssp->recv_seq_num++, &pfQOP);

	if (status != SEC_E_OK)
		return status;

	credssp_read_ts_credentials(credssp, &Buffers[1]);

	free(buffer);

	return SEC_E_OK;
}

int credssp_sizeof_nego_token(int length)
{
	length = ber_sizeof_octet_string(length);
	length += ber_sizeof_contextual_tag(length);
	return length;
}

int credssp_sizeof_nego_tokens(int length)
{
	length = credssp_sizeof_nego_token(length);
	length += ber_sizeof_sequence_tag(length);
	length += ber_sizeof_sequence_tag(length);
	length += ber_sizeof_contextual_tag(length);
	return length;
}

int credssp_sizeof_pub_key_auth(int length)
{
	length = ber_sizeof_octet_string(length);
	length += ber_sizeof_contextual_tag(length);
	return length;
}

int credssp_sizeof_auth_info(int length)
{
	length = ber_sizeof_octet_string(length);
	length += ber_sizeof_contextual_tag(length);
	return length;
}

int credssp_sizeof_ts_request(int length)
{
	length += ber_sizeof_integer(2);
	length += ber_sizeof_contextual_tag(3);
	return length;
}

/**
 * Send CredSSP message.
 * @param credssp
 */

void credssp_send(rdpCredssp* credssp)
{
	wStream* s;
	int length;
	int ts_request_length;
	int nego_tokens_length;
	int pub_key_auth_length;
	int auth_info_length;

	nego_tokens_length = (credssp->negoToken.cbBuffer > 0) ? credssp_sizeof_nego_tokens(credssp->negoToken.cbBuffer) : 0;
	pub_key_auth_length = (credssp->pubKeyAuth.cbBuffer > 0) ? credssp_sizeof_pub_key_auth(credssp->pubKeyAuth.cbBuffer) : 0;
	auth_info_length = (credssp->authInfo.cbBuffer > 0) ? credssp_sizeof_auth_info(credssp->authInfo.cbBuffer) : 0;

	length = nego_tokens_length + pub_key_auth_length + auth_info_length;

	ts_request_length = credssp_sizeof_ts_request(length);

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

		length -= ber_write_contextual_tag(s, 1, ber_sizeof_sequence(ber_sizeof_sequence(ber_sizeof_sequence_octet_string(credssp->negoToken.cbBuffer))), TRUE); /* NegoData */
		length -= ber_write_sequence_tag(s, ber_sizeof_sequence(ber_sizeof_sequence_octet_string(credssp->negoToken.cbBuffer))); /* SEQUENCE OF NegoDataItem */
		length -= ber_write_sequence_tag(s, ber_sizeof_sequence_octet_string(credssp->negoToken.cbBuffer)); /* NegoDataItem */
		length -= ber_write_sequence_octet_string(s, 0, (BYTE*) credssp->negoToken.pvBuffer, credssp->negoToken.cbBuffer); /* OCTET STRING */

		// assert length == 0
	}

	/* [2] authInfo (OCTET STRING) */
	if (auth_info_length > 0)
	{
		length = auth_info_length;
		length -= ber_write_sequence_octet_string(s, 2, credssp->authInfo.pvBuffer, credssp->authInfo.cbBuffer);

		// assert length == 0
	}

	/* [3] pubKeyAuth (OCTET STRING) */
	if (pub_key_auth_length > 0)
	{
		length = pub_key_auth_length;
		length -= ber_write_sequence_octet_string(s, 3, credssp->pubKeyAuth.pvBuffer, credssp->pubKeyAuth.cbBuffer);

		// assert length == 0
	}

	Stream_SealLength(s);

	transport_write(credssp->transport, s);

	Stream_Free(s, TRUE);
}

/**
 * Receive CredSSP message.
 * @param credssp
 * @return
 */

int credssp_recv(rdpCredssp* credssp)
{
	wStream* s;
	int length;
	int status;
	UINT32 version;

	s = Stream_New(NULL, 4096);

	status = transport_read(credssp->transport, s);
	Stream_Length(s) = status;

	if (status < 0)
	{
		fprintf(stderr, "credssp_recv() error: %d\n", status);
		Stream_Free(s, TRUE);
		return -1;
	}

	/* TSRequest */
	if(!ber_read_sequence_tag(s, &length) ||
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
		sspi_SecBufferAlloc(&credssp->negoToken, length);
		Stream_Read(s, credssp->negoToken.pvBuffer, length);
		credssp->negoToken.cbBuffer = length;
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
		sspi_SecBufferAlloc(&credssp->authInfo, length);
		Stream_Read(s, credssp->authInfo.pvBuffer, length);
		credssp->authInfo.cbBuffer = length;
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
		sspi_SecBufferAlloc(&credssp->pubKeyAuth, length);
		Stream_Read(s, credssp->pubKeyAuth.pvBuffer, length);
		credssp->pubKeyAuth.cbBuffer = length;
	}

	Stream_Free(s, TRUE);

	return 0;
}

void credssp_buffer_print(rdpCredssp* credssp)
{
	if (credssp->negoToken.cbBuffer > 0)
	{
		fprintf(stderr, "CredSSP.negoToken (length = %d):\n", (int) credssp->negoToken.cbBuffer);
		winpr_HexDump(credssp->negoToken.pvBuffer, credssp->negoToken.cbBuffer);
	}

	if (credssp->pubKeyAuth.cbBuffer > 0)
	{
		fprintf(stderr, "CredSSP.pubKeyAuth (length = %d):\n", (int) credssp->pubKeyAuth.cbBuffer);
		winpr_HexDump(credssp->pubKeyAuth.pvBuffer, credssp->pubKeyAuth.cbBuffer);
	}

	if (credssp->authInfo.cbBuffer > 0)
	{
		fprintf(stderr, "CredSSP.authInfo (length = %d):\n", (int) credssp->authInfo.cbBuffer);
		winpr_HexDump(credssp->authInfo.pvBuffer, credssp->authInfo.cbBuffer);
	}
}

void credssp_buffer_free(rdpCredssp* credssp)
{
	sspi_SecBufferFree(&credssp->negoToken);
	sspi_SecBufferFree(&credssp->pubKeyAuth);
	sspi_SecBufferFree(&credssp->authInfo);
}

LPTSTR credssp_make_spn(const char* ServiceClass, const char* hostname)
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

rdpCredssp* credssp_new(freerdp* instance, rdpTransport* transport, rdpSettings* settings)
{
	rdpCredssp* credssp;

	credssp = (rdpCredssp*) malloc(sizeof(rdpCredssp));

	if (credssp)
	{
		HKEY hKey;
		LONG status;
		DWORD dwType;
		DWORD dwSize;

		ZeroMemory(credssp, sizeof(rdpCredssp));

		credssp->instance = instance;
		credssp->settings = settings;
		credssp->server = settings->ServerMode;
		credssp->transport = transport;
		credssp->send_seq_num = 0;
		credssp->recv_seq_num = 0;
		ZeroMemory(&credssp->negoToken, sizeof(SecBuffer));
		ZeroMemory(&credssp->pubKeyAuth, sizeof(SecBuffer));
		ZeroMemory(&credssp->authInfo, sizeof(SecBuffer));
		SecInvalidateHandle(&credssp->context);

		if (credssp->server)
		{
			status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\FreeRDP\\Server"),
					0, KEY_READ | KEY_WOW64_64KEY, &hKey);

			if (status == ERROR_SUCCESS)
			{
				status = RegQueryValueEx(hKey, _T("SspiModule"), NULL, &dwType, NULL, &dwSize);

				if (status == ERROR_SUCCESS)
				{
					credssp->SspiModule = (LPTSTR) malloc(dwSize + sizeof(TCHAR));

					status = RegQueryValueEx(hKey, _T("SspiModule"), NULL, &dwType,
							(BYTE*) credssp->SspiModule, &dwSize);

					if (status == ERROR_SUCCESS)
					{
						_tprintf(_T("Using SSPI Module: %s\n"), credssp->SspiModule);
						RegCloseKey(hKey);
					}
				}
			}
		}
	}

	return credssp;
}

/**
 * Free CredSSP state machine.
 * @param credssp
 */

void credssp_free(rdpCredssp* credssp)
{
	if (credssp)
	{
		if (credssp->table)
			credssp->table->DeleteSecurityContext(&credssp->context);

		sspi_SecBufferFree(&credssp->PublicKey);
		sspi_SecBufferFree(&credssp->ts_credentials);

		free(credssp->ServicePrincipalName);

		free(credssp->identity.User);
		free(credssp->identity.Domain);
		free(credssp->identity.Password);
		free(credssp);
	}
}

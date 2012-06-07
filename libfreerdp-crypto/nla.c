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

#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <freerdp/crypto/nla.h>
#include <freerdp/crypto/tls.h>

#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/print.h>

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

#define TERMSRV_SPN_PREFIX	"TERMSRV/"

void credssp_send(rdpCredssp* credssp);
int credssp_recv(rdpCredssp* credssp);
void credssp_buffer_print(rdpCredssp* credssp);
void credssp_buffer_free(rdpCredssp* credssp);
SECURITY_STATUS credssp_verify_public_key_echo(rdpCredssp* credssp);
void credssp_encode_ts_credentials(rdpCredssp* credssp);
SECURITY_STATUS credssp_encrypt_ts_credentials(rdpCredssp* credssp);
SECURITY_STATUS credssp_decrypt_ts_credentials(rdpCredssp* credssp);

/**
 * Initialize NTLMSSP authentication module (client).
 * @param credssp
 */

int credssp_ntlm_client_init(rdpCredssp* credssp)
{
	char* spn;
	int length;
	freerdp* instance;
	rdpSettings* settings;
	
	settings = credssp->settings;
	instance = (freerdp*) settings->instance;

	if ((settings->password == NULL) || (settings->username == NULL))
	{
		if (instance->Authenticate)
		{
			boolean proceed = instance->Authenticate(instance,
					&settings->username, &settings->password, &settings->domain);
			if (!proceed)
				return 0;
		}
	}

	sspi_SetAuthIdentity(&(credssp->identity), settings->username, settings->domain, settings->password);

	sspi_SecBufferAlloc(&credssp->PublicKey, credssp->tls->public_key.length);
	CopyMemory(credssp->PublicKey.pvBuffer, credssp->tls->public_key.data, credssp->tls->public_key.length);

	length = sizeof(TERMSRV_SPN_PREFIX) + strlen(settings->hostname);

	spn = (SEC_CHAR*) malloc(length + 1);
	sprintf(spn, "%s%s", TERMSRV_SPN_PREFIX, settings->hostname);

#ifdef UNICODE
	credssp->ServicePrincipalName = (LPCTSTR) malloc(length * 2 + 2);
	MultiByteToWideChar(CP_ACP, 0, spn, length,
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

	sspi_SecBufferAlloc(&credssp->PublicKey, credssp->tls->public_key.length);
	CopyMemory(credssp->PublicKey.pvBuffer, credssp->tls->public_key.data, credssp->tls->public_key.length);

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
	PSecBuffer p_buffer;
	SecBuffer input_buffer;
	SecBuffer output_buffer;
	SecBufferDesc input_buffer_desc;
	SecBufferDesc output_buffer_desc;
	boolean have_context;
	boolean have_input_buffer;
	boolean have_pub_key_auth;

	sspi_GlobalInit();

	if (credssp_ntlm_client_init(credssp) == 0)
		return 0;

#ifdef WITH_NATIVE_SSPI
	{
		HMODULE hSSPI;
		INIT_SECURITY_INTERFACE InitSecurityInterface;
		PSecurityFunctionTable pSecurityInterface = NULL;

		hSSPI = LoadLibrary(_T("security.dll"));

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

	status = credssp->table->QuerySecurityPackageInfo(NTLMSP_NAME, &pPackageInfo);

	if (status != SEC_E_OK)
	{
		printf("QuerySecurityPackageInfo status: 0x%08X\n", status);
		return 0;
	}

	cbMaxToken = pPackageInfo->cbMaxToken;

	status = credssp->table->AcquireCredentialsHandle(NULL, NTLMSP_NAME,
			SECPKG_CRED_OUTBOUND, NULL, &credssp->identity, NULL, NULL, &credentials, &expiration);

	if (status != SEC_E_OK)
	{
		printf("AcquireCredentialsHandle status: 0x%08X\n", status);
		return 0;
	}

	have_context = false;
	have_input_buffer = false;
	have_pub_key_auth = false;
	memset(&input_buffer, 0, sizeof(SecBuffer));
	memset(&output_buffer, 0, sizeof(SecBuffer));
	memset(&credssp->ContextSizes, 0, sizeof(SecPkgContext_Sizes));

	fContextReq = ISC_REQ_CONFIDENTIALITY | ISC_REQ_INTEGRITY | ISC_REQ_IDENTIFY |
			ISC_REQ_EXTENDED_ERROR | ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT;

	while (true)
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

		freerdp_hexdump(credssp->identity.User, credssp->identity.UserLength);

		if (input_buffer.pvBuffer != NULL)
		{
			free(input_buffer.pvBuffer);
			input_buffer.pvBuffer = NULL;
		}

		if ((status == SEC_I_COMPLETE_AND_CONTINUE) || (status == SEC_I_COMPLETE_NEEDED) || (status == SEC_E_OK))
		{
			if (credssp->table->CompleteAuthToken != NULL)
				credssp->table->CompleteAuthToken(&credssp->context, &output_buffer_desc);

			have_pub_key_auth = true;

			if (credssp->table->QueryContextAttributes(&credssp->context, SECPKG_ATTR_SIZES, &credssp->ContextSizes) != SEC_E_OK)
			{
				printf("QueryContextAttributes SECPKG_ATTR_SIZES failure\n");
				return 0;
			}

			if (have_pub_key_auth)
			{
				BYTE* p;
				SecBuffer Buffers[2];
				SecBufferDesc Message;
				SECURITY_STATUS encrypt_status;

				Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
				Buffers[1].BufferType = SECBUFFER_DATA; /* TLS Public Key */

				Buffers[0].cbBuffer = credssp->ContextSizes.cbMaxSignature;
				Buffers[0].pvBuffer = xzalloc(Buffers[0].cbBuffer);

				Buffers[1].cbBuffer = credssp->PublicKey.cbBuffer;
				Buffers[1].pvBuffer = malloc(Buffers[1].cbBuffer);
				CopyMemory(Buffers[1].pvBuffer, credssp->PublicKey.pvBuffer, Buffers[1].cbBuffer);

				Message.cBuffers = 2;
				Message.ulVersion = SECBUFFER_VERSION;
				Message.pBuffers = (PSecBuffer) &Buffers;

				sspi_SecBufferAlloc(&credssp->pubKeyAuth, Buffers[0].cbBuffer + Buffers[1].cbBuffer);

				encrypt_status = credssp->table->EncryptMessage(&credssp->context, 0, &Message, 0);

				if (encrypt_status != SEC_E_OK)
				{
					printf("EncryptMessage status: 0x%08X\n", encrypt_status);
					return 0;
				}

#ifdef WITH_DEBUG_CREDSSP
				printf("CredSSP.Signature: (%d)\n", (int) Buffers[0].cbBuffer);
				winpr_HexDump((BYTE*) Buffers[0].pvBuffer, Buffers[0].cbBuffer);
				printf("CredSSP.PublicKey: (%d)\n", (int) credssp->PublicKey.cbBuffer);
				winpr_HexDump((BYTE*) credssp->PublicKey.pvBuffer, credssp->PublicKey.cbBuffer);
				printf("CredSSP.PublicKey (encrypted) (%d):\n", (int) Buffers[1].cbBuffer);
				winpr_HexDump((BYTE*) Buffers[1].pvBuffer, Buffers[1].cbBuffer);
#endif

				p = (BYTE*) credssp->pubKeyAuth.pvBuffer;
				CopyMemory(p, Buffers[0].pvBuffer, Buffers[0].cbBuffer); /* Message Signature */
				CopyMemory(&p[Buffers[0].cbBuffer], Buffers[1].pvBuffer, Buffers[1].cbBuffer); /* Encrypted Public Key */
				free(Buffers[0].pvBuffer);
				free(Buffers[1].pvBuffer);
			}

			if (status == SEC_I_COMPLETE_NEEDED)
				status = SEC_E_OK;
			else if (status == SEC_I_COMPLETE_AND_CONTINUE)
				status = SEC_I_CONTINUE_NEEDED;
		}

		/* send authentication token to server */

		if (output_buffer.cbBuffer > 0)
		{
			p_buffer = &output_buffer_desc.pBuffers[0];

			credssp->negoToken.pvBuffer = p_buffer->pvBuffer;
			credssp->negoToken.cbBuffer = p_buffer->cbBuffer;

#ifdef WITH_DEBUG_CREDSSP
			printf("Sending Authentication Token\n");
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
		printf("Receiving Authentication Token\n");
		winpr_HexDump(credssp->negoToken.pvBuffer, credssp->negoToken.cbBuffer);
#endif

		p_buffer = &input_buffer_desc.pBuffers[0];
		p_buffer->pvBuffer = credssp->negoToken.pvBuffer;
		p_buffer->cbBuffer = credssp->negoToken.cbBuffer;

		have_input_buffer = true;
		have_context = true;
	}

	/* Encrypted Public Key +1 */
	if (credssp_recv(credssp) < 0)
		return -1;

	/* Verify Server Public Key Echo */

	status = credssp_verify_public_key_echo(credssp);
	credssp_buffer_free(credssp);

	if (status != SEC_E_OK)
		return 0;

	/* Send encrypted credentials */

	status = credssp_encrypt_ts_credentials(credssp);

	if (status != SEC_E_OK)
	{
		printf("credssp_encrypt_ts_credentials status: 0x%08X\n", status);
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
	PSecBuffer p_buffer;
	SecBuffer input_buffer;
	SecBuffer output_buffer;
	SecBufferDesc input_buffer_desc;
	SecBufferDesc output_buffer_desc;
	boolean have_context;
	boolean have_input_buffer;
	boolean have_pub_key_auth;

	sspi_GlobalInit();

	if (credssp_ntlm_server_init(credssp) == 0)
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

	status = credssp->table->QuerySecurityPackageInfo(NTLMSP_NAME, &pPackageInfo);

	if (status != SEC_E_OK)
	{
		printf("QuerySecurityPackageInfo status: 0x%08X\n", status);
		return 0;
	}

	cbMaxToken = pPackageInfo->cbMaxToken;

	status = credssp->table->AcquireCredentialsHandle(NULL, NTLMSP_NAME,
			SECPKG_CRED_INBOUND, NULL, &credssp->identity, NULL, NULL, &credentials, &expiration);

	if (status != SEC_E_OK)
	{
		printf("AcquireCredentialsHandle status: 0x%08X\n", status);
		return 0;
	}

	have_context = false;
	have_input_buffer = false;
	have_pub_key_auth = false;
	memset(&input_buffer, 0, sizeof(SecBuffer));
	memset(&output_buffer, 0, sizeof(SecBuffer));
	memset(&credssp->ContextSizes, 0, sizeof(SecPkgContext_Sizes));

	fContextReq = ASC_REQ_REPLAY_DETECT | ASC_REQ_SEQUENCE_DETECT |
			ASC_REQ_CONFIDENTIALITY | ASC_REQ_DELEGATE;

	while (true)
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
		printf("Receiving Authentication Token\n");
		credssp_buffer_print(credssp);
#endif

		p_buffer = &input_buffer_desc.pBuffers[0];
		p_buffer->pvBuffer = credssp->negoToken.pvBuffer;
		p_buffer->cbBuffer = credssp->negoToken.cbBuffer;

		output_buffer_desc.ulVersion = SECBUFFER_VERSION;
		output_buffer_desc.cBuffers = 1;
		output_buffer_desc.pBuffers = &output_buffer;
		output_buffer.BufferType = SECBUFFER_TOKEN;
		output_buffer.cbBuffer = cbMaxToken;
		output_buffer.pvBuffer = malloc(output_buffer.cbBuffer);

		status = credssp->table->AcceptSecurityContext(&credentials,
			have_context? &credssp->context: NULL,
			&input_buffer_desc, fContextReq, SECURITY_NETWORK_DREP, &credssp->context,
			&output_buffer_desc, &pfContextAttr, &expiration);

		if (input_buffer.pvBuffer != NULL)
		{
			free(input_buffer.pvBuffer);
			input_buffer.pvBuffer = NULL;
		}

		p_buffer = &output_buffer_desc.pBuffers[0];
		credssp->negoToken.pvBuffer = p_buffer->pvBuffer;
		credssp->negoToken.cbBuffer = p_buffer->cbBuffer;

		if ((status == SEC_I_COMPLETE_AND_CONTINUE) || (status == SEC_I_COMPLETE_NEEDED))
		{
			if (credssp->table->CompleteAuthToken != NULL)
				credssp->table->CompleteAuthToken(&credssp->context, &output_buffer_desc);

			have_pub_key_auth = true;

			if (credssp->table->QueryContextAttributes(&credssp->context, SECPKG_ATTR_SIZES, &credssp->ContextSizes) != SEC_E_OK)
			{
				printf("QueryContextAttributes SECPKG_ATTR_SIZES failure\n");
				return 0;
			}

			credssp_verify_public_key_echo(credssp);

			sspi_SecBufferFree(&credssp->negoToken);
			credssp->negoToken.pvBuffer = NULL;
			credssp->negoToken.cbBuffer = 0;

			if (have_pub_key_auth)
			{
				BYTE* p;
				SecBuffer Buffers[2];
				SecBufferDesc Message;

				Buffers[0].BufferType = SECBUFFER_DATA; /* TLS Public Key */
				Buffers[1].BufferType = SECBUFFER_TOKEN; /* Signature */

				Buffers[0].cbBuffer = credssp->PublicKey.cbBuffer;
				Buffers[0].pvBuffer = malloc(Buffers[0].cbBuffer);
				CopyMemory(Buffers[0].pvBuffer, credssp->PublicKey.pvBuffer, Buffers[0].cbBuffer);

				Buffers[1].cbBuffer = credssp->ContextSizes.cbMaxSignature;
				Buffers[1].pvBuffer = xzalloc(Buffers[1].cbBuffer);

				Message.cBuffers = 2;
				Message.ulVersion = SECBUFFER_VERSION;
				Message.pBuffers = (PSecBuffer) &Buffers;

				p = (BYTE*) Buffers[0].pvBuffer;
				p[0]++; /* Public Key +1 */

				sspi_SecBufferAlloc(&credssp->pubKeyAuth, Buffers[0].cbBuffer + Buffers[1].cbBuffer);

				credssp->table->EncryptMessage(&credssp->context, 0, &Message, 0);

				p = (BYTE*) credssp->pubKeyAuth.pvBuffer;
				CopyMemory(p, Buffers[1].pvBuffer, Buffers[1].cbBuffer); /* Message Signature */
				CopyMemory(&p[Buffers[1].cbBuffer], Buffers[0].pvBuffer, Buffers[0].cbBuffer); /* Encrypted Public Key */
			}

			if (status == SEC_I_COMPLETE_NEEDED)
				status = SEC_E_OK;
			else if (status == SEC_I_COMPLETE_AND_CONTINUE)
				status = SEC_I_CONTINUE_NEEDED;
		}

		/* send authentication token */

#ifdef WITH_DEBUG_CREDSSP
		printf("Sending Authentication Token\n");
		credssp_buffer_print(credssp);
#endif

		credssp_send(credssp);
		credssp_buffer_free(credssp);

		if (status != SEC_I_CONTINUE_NEEDED)
			break;

		have_context = true;
	}

	/* Receive encrypted credentials */

	if (credssp_recv(credssp) < 0)
		return -1;

	credssp_decrypt_ts_credentials(credssp);

	if (status != SEC_E_OK)
	{
		printf("AcceptSecurityContext status: 0x%08X\n", status);
		return 0;
	}

	status = credssp->table->ImpersonateSecurityContext(&credssp->context);

	if (status != SEC_E_OK)
	{
		printf("ImpersonateSecurityContext status: 0x%08X\n", status);
		return 0;
	}
	else
	{
		status = credssp->table->RevertSecurityContext(&credssp->context);

		if (status != SEC_E_OK)
		{
			printf("RevertSecurityContext status: 0x%08X\n", status);
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

SECURITY_STATUS credssp_verify_public_key_echo(rdpCredssp* credssp)
{
	int length;
	ULONG pfQOP;
	BYTE* public_key1;
	BYTE* public_key2;
	BYTE* pub_key_auth;
	int public_key_length;
	SecBuffer Buffers[2];
	SecBufferDesc Message;
	SECURITY_STATUS status;

	length = credssp->pubKeyAuth.cbBuffer;
	pub_key_auth = (BYTE*) credssp->pubKeyAuth.pvBuffer;
	public_key_length = credssp->PublicKey.cbBuffer;

	Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
	Buffers[1].BufferType = SECBUFFER_DATA; /* Encrypted TLS Public Key */

	Buffers[0].cbBuffer = credssp->ContextSizes.cbMaxSignature;
	Buffers[0].pvBuffer = malloc(Buffers[0].cbBuffer);
	CopyMemory(Buffers[0].pvBuffer, pub_key_auth, Buffers[0].cbBuffer);

	Buffers[1].cbBuffer = length - Buffers[0].cbBuffer;
	Buffers[1].pvBuffer = malloc(Buffers[1].cbBuffer);
	CopyMemory(Buffers[1].pvBuffer, &pub_key_auth[Buffers[0].cbBuffer], Buffers[1].cbBuffer);

	Message.cBuffers = 2;
	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer) &Buffers;

	status = credssp->table->DecryptMessage(&credssp->context, &Message, 0, &pfQOP);

	if (status != SEC_E_OK)
	{
		printf("DecryptMessage failure: 0x%08X\n", status);
		return status;
	}

	public_key1 = (BYTE*) credssp->PublicKey.pvBuffer;
	public_key2 = (BYTE*) Buffers[1].pvBuffer;

	if (!credssp->server)
		public_key2[0]--; /* server echos the public key +1 */

	if (memcmp(public_key1, public_key2, public_key_length) != 0)
	{
		printf("Could not verify server's public key echo\n");

		printf("Expected (length = %d):\n", public_key_length);
		winpr_HexDump(public_key1, public_key_length);

		printf("Actual (length = %d):\n", public_key_length);
		winpr_HexDump(public_key2, public_key_length);

		return SEC_E_MESSAGE_ALTERED; /* DO NOT SEND CREDENTIALS! */
	}

	public_key2[0]++;

	free(Buffers[0].pvBuffer);
	free(Buffers[1].pvBuffer);

	return SEC_E_OK;
}

int credssp_skip_ts_password_creds(rdpCredssp* credssp)
{
	int length;
	int ts_password_creds_length = 0;

	length = ber_skip_octet_string(credssp->identity.DomainLength * 2);
	length += ber_skip_contextual_tag(length);
	ts_password_creds_length += length;

	length = ber_skip_octet_string(credssp->identity.UserLength * 2);
	length += ber_skip_contextual_tag(length);
	ts_password_creds_length += length;

	length = ber_skip_octet_string(credssp->identity.PasswordLength * 2);
	length += ber_skip_contextual_tag(length);
	ts_password_creds_length += length;

	length = ber_skip_sequence(ts_password_creds_length);

	return length;
}

void credssp_read_ts_password_creds(rdpCredssp* credssp, STREAM* s)
{
	int length;

	/* TSPasswordCreds (SEQUENCE) */
	ber_read_sequence_tag(s, &length);

	/* [0] domainName (OCTET STRING) */
	ber_read_contextual_tag(s, 0, &length, true);
	ber_read_octet_string_tag(s, &length);
	credssp->identity.DomainLength = (UINT32) length;
	credssp->identity.Domain = (UINT16*) malloc(length);
	CopyMemory(credssp->identity.Domain, s->p, credssp->identity.DomainLength);
	stream_seek(s, credssp->identity.DomainLength);

	/* [1] userName (OCTET STRING) */
	ber_read_contextual_tag(s, 1, &length, true);
	ber_read_octet_string_tag(s, &length);
	credssp->identity.UserLength = (UINT32) length;
	credssp->identity.User = (UINT16*) malloc(length);
	CopyMemory(credssp->identity.User, s->p, credssp->identity.UserLength);
	stream_seek(s, credssp->identity.UserLength);

	/* [2] password (OCTET STRING) */
	ber_read_contextual_tag(s, 2, &length, true);
	ber_read_octet_string_tag(s, &length);
	credssp->identity.PasswordLength = (UINT32) length;
	credssp->identity.Password = (UINT16*) malloc(length);
	CopyMemory(credssp->identity.Password, s->p, credssp->identity.PasswordLength);
	stream_seek(s, credssp->identity.PasswordLength);
}

void credssp_write_ts_password_creds(rdpCredssp* credssp, STREAM* s)
{
	int length;

	length = credssp_skip_ts_password_creds(credssp);

	/* TSPasswordCreds (SEQUENCE) */
	length = ber_get_content_length(length);
	ber_write_sequence_tag(s, length);

	/* [0] domainName (OCTET STRING) */
	ber_write_contextual_tag(s, 0, credssp->identity.DomainLength * 2 + 2, true);
	ber_write_octet_string(s, (BYTE*) credssp->identity.Domain, credssp->identity.DomainLength * 2);

	/* [1] userName (OCTET STRING) */
	ber_write_contextual_tag(s, 1, credssp->identity.UserLength * 2 + 2, true);
	ber_write_octet_string(s, (BYTE*) credssp->identity.User, credssp->identity.UserLength * 2);

	/* [2] password (OCTET STRING) */
	ber_write_contextual_tag(s, 2, credssp->identity.PasswordLength * 2 + 2, true);
	ber_write_octet_string(s, (BYTE*) credssp->identity.Password, credssp->identity.PasswordLength * 2);
}

int credssp_skip_ts_credentials(rdpCredssp* credssp)
{
	int length;
	int ts_password_creds_length;
	int ts_credentials_length = 0;

	length = ber_skip_integer(0);
	length += ber_skip_contextual_tag(length);
	ts_credentials_length += length;

	ts_password_creds_length = credssp_skip_ts_password_creds(credssp);
	length = ber_skip_octet_string(ts_password_creds_length);
	length += ber_skip_contextual_tag(length);
	ts_credentials_length += length;

	length = ber_skip_sequence(ts_credentials_length);

	return length;
}

void credssp_read_ts_credentials(rdpCredssp* credssp, PSecBuffer ts_credentials)
{
	STREAM* s;
	int length;
	int ts_password_creds_length;

	s = stream_new(0);
	stream_attach(s, ts_credentials->pvBuffer, ts_credentials->cbBuffer);

	/* TSCredentials (SEQUENCE) */
	ber_read_sequence_tag(s, &length);

	/* [0] credType (INTEGER) */
	ber_read_contextual_tag(s, 0, &length, true);
	ber_read_integer(s, NULL);

	/* [1] credentials (OCTET STRING) */
	ber_read_contextual_tag(s, 1, &length, true);
	ber_read_octet_string_tag(s, &ts_password_creds_length);

	credssp_read_ts_password_creds(credssp, s);

	stream_detach(s);
	stream_free(s);
}

void credssp_write_ts_credentials(rdpCredssp* credssp, STREAM* s)
{
	int length;
	int ts_password_creds_length;

	length = credssp_skip_ts_credentials(credssp);
	ts_password_creds_length = credssp_skip_ts_password_creds(credssp);

	/* TSCredentials (SEQUENCE) */
	length = ber_get_content_length(length);
	length -= ber_write_sequence_tag(s, length);

	/* [0] credType (INTEGER) */
	length -= ber_write_contextual_tag(s, 0, 3, true);
	length -= ber_write_integer(s, 1);

	/* [1] credentials (OCTET STRING) */
	length -= 1;
	length -= ber_write_contextual_tag(s, 1, length, true);
	length -= ber_write_octet_string_tag(s, ts_password_creds_length);

	credssp_write_ts_password_creds(credssp, s);
}

/**
 * Encode TSCredentials structure.
 * @param credssp
 */

void credssp_encode_ts_credentials(rdpCredssp* credssp)
{
	STREAM* s;
	int length;

	s = stream_new(0);
	length = credssp_skip_ts_credentials(credssp);
	sspi_SecBufferAlloc(&credssp->ts_credentials, length);
	stream_attach(s, credssp->ts_credentials.pvBuffer, length);

	credssp_write_ts_credentials(credssp, s);
	stream_detach(s);
	stream_free(s);
}

SECURITY_STATUS credssp_encrypt_ts_credentials(rdpCredssp* credssp)
{
	BYTE* p;
	SecBuffer Buffers[2];
	SecBufferDesc Message;
	SECURITY_STATUS status;

	credssp_encode_ts_credentials(credssp);

	Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
	Buffers[1].BufferType = SECBUFFER_DATA; /* TSCredentials */

	Buffers[0].cbBuffer = 16;
	Buffers[0].pvBuffer = xzalloc(Buffers[0].cbBuffer);

	Buffers[1].cbBuffer = credssp->ts_credentials.cbBuffer;
	Buffers[1].pvBuffer = malloc(Buffers[1].cbBuffer);
	CopyMemory(Buffers[1].pvBuffer, credssp->ts_credentials.pvBuffer, Buffers[1].cbBuffer);

	Message.cBuffers = 2;
	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer) &Buffers;

	sspi_SecBufferAlloc(&credssp->authInfo, Buffers[0].cbBuffer + Buffers[1].cbBuffer);

	status = credssp->table->EncryptMessage(&credssp->context, 0, &Message, 1);

	if (status != SEC_E_OK)
		return status;

	p = (BYTE*) credssp->authInfo.pvBuffer;
	CopyMemory(p, Buffers[0].pvBuffer, Buffers[0].cbBuffer); /* Message Signature */
	CopyMemory(&p[Buffers[0].cbBuffer], Buffers[1].pvBuffer, Buffers[1].cbBuffer); /* Encrypted TSCredentials */

	free(Buffers[0].pvBuffer);
	free(Buffers[1].pvBuffer);

	return SEC_E_OK;
}

SECURITY_STATUS credssp_decrypt_ts_credentials(rdpCredssp* credssp)
{
	BYTE* p;
	SecBuffer Buffers[2];
	SecBufferDesc Message;
	SECURITY_STATUS status;

	Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
	Buffers[1].BufferType = SECBUFFER_DATA; /* TSCredentials */

	Buffers[0].cbBuffer = 16;
	Buffers[0].pvBuffer = malloc(Buffers[0].cbBuffer);
	CopyMemory(Buffers[0].pvBuffer, credssp->authInfo.pvBuffer, Buffers[0].cbBuffer);

	Buffers[1].cbBuffer = credssp->authInfo.cbBuffer - Buffers[0].cbBuffer;
	Buffers[1].pvBuffer = malloc(Buffers[1].cbBuffer);
	p = (BYTE*) credssp->authInfo.pvBuffer;
	CopyMemory(Buffers[1].pvBuffer, &p[Buffers[0].cbBuffer], Buffers[1].cbBuffer);

	Message.cBuffers = 2;
	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer) &Buffers;

	sspi_SecBufferAlloc(&credssp->authInfo, Buffers[0].cbBuffer + Buffers[1].cbBuffer);

	status = credssp->table->DecryptMessage(&credssp->context, &Message, 1, 0);

	if (status != SEC_E_OK)
		return status;

	credssp_read_ts_credentials(credssp, &Buffers[1]);

	free(Buffers[0].pvBuffer);
	free(Buffers[1].pvBuffer);

	return SEC_E_OK;
}

int credssp_skip_nego_token(int length)
{
	length = ber_skip_octet_string(length);
	length += ber_skip_contextual_tag(length);
	return length;
}

int credssp_skip_nego_tokens(int length)
{
	length = credssp_skip_nego_token(length);
	length += ber_skip_sequence_tag(length);
	length += ber_skip_sequence_tag(length);
	length += ber_skip_contextual_tag(length);
	return length;
}

int credssp_skip_pub_key_auth(int length)
{
	length = ber_skip_octet_string(length);
	length += ber_skip_contextual_tag(length);
	return length;
}

int credssp_skip_auth_info(int length)
{
	length = ber_skip_octet_string(length);
	length += ber_skip_contextual_tag(length);
	return length;
}

int credssp_skip_ts_request(int length)
{
	length += ber_skip_integer(2);
	length += ber_skip_contextual_tag(3);
	length += ber_skip_sequence_tag(length);
	return length;
}

/**
 * Send CredSSP message.
 * @param credssp
 */

void credssp_send(rdpCredssp* credssp)
{
	STREAM* s;
	int length;
	int ts_request_length;
	int nego_tokens_length;
	int pub_key_auth_length;
	int auth_info_length;

	nego_tokens_length = (credssp->negoToken.cbBuffer > 0) ? credssp_skip_nego_tokens(credssp->negoToken.cbBuffer) : 0;
	pub_key_auth_length = (credssp->pubKeyAuth.cbBuffer > 0) ? credssp_skip_pub_key_auth(credssp->pubKeyAuth.cbBuffer) : 0;
	auth_info_length = (credssp->authInfo.cbBuffer > 0) ? credssp_skip_auth_info(credssp->authInfo.cbBuffer) : 0;

	length = nego_tokens_length + pub_key_auth_length + auth_info_length;
	ts_request_length = credssp_skip_ts_request(length);

	s = stream_new(ts_request_length);

	/* TSRequest */
	length = ber_get_content_length(ts_request_length);
	ber_write_sequence_tag(s, length); /* SEQUENCE */
	ber_write_contextual_tag(s, 0, 3, true); /* [0] version */
	ber_write_integer(s, 2); /* INTEGER */

	/* [1] negoTokens (NegoData) */
	if (nego_tokens_length > 0)
	{
		length = ber_get_content_length(nego_tokens_length);
		length -= ber_write_contextual_tag(s, 1, length, true); /* NegoData */
		length -= ber_write_sequence_tag(s, length); /* SEQUENCE OF NegoDataItem */
		length -= ber_write_sequence_tag(s, length); /* NegoDataItem */
		length -= ber_write_contextual_tag(s, 0, length, true); /* [0] negoToken */
		ber_write_octet_string(s, credssp->negoToken.pvBuffer, length); /* OCTET STRING */
	}

	/* [2] authInfo (OCTET STRING) */
	if (auth_info_length > 0)
	{
		length = ber_get_content_length(auth_info_length);
		length -= ber_write_contextual_tag(s, 2, length, true);
		ber_write_octet_string(s, credssp->authInfo.pvBuffer, credssp->authInfo.cbBuffer);
	}

	/* [3] pubKeyAuth (OCTET STRING) */
	if (pub_key_auth_length > 0)
	{
		length = ber_get_content_length(pub_key_auth_length);
		length -= ber_write_contextual_tag(s, 3, length, true);
		ber_write_octet_string(s, credssp->pubKeyAuth.pvBuffer, length);
	}

	printf("Sending Token (%d)\n", stream_get_length(s));
	freerdp_hexdump(s->data, stream_get_length(s));

	tls_write(credssp->tls, s->data, stream_get_length(s));
	stream_free(s);
}

/**
 * Receive CredSSP message.
 * @param credssp
 * @return
 */

int credssp_recv(rdpCredssp* credssp)
{
	STREAM* s;
	int length;
	int status;
	UINT32 version;

	s = stream_new(2048);
	status = tls_read(credssp->tls, s->data, stream_get_left(s));
	s->size = status;

	if (status < 0)
	{
		stream_free(s);
		return -1;
	}

	printf("Receiving Token (%d)\n", s->size);
	freerdp_hexdump(s->data, s->size);

	/* TSRequest */
	ber_read_sequence_tag(s, &length);
	ber_read_contextual_tag(s, 0, &length, true);
	ber_read_integer(s, &version);

	/* [1] negoTokens (NegoData) */
	if (ber_read_contextual_tag(s, 1, &length, true) != false)
	{
		ber_read_sequence_tag(s, &length); /* SEQUENCE OF NegoDataItem */
		ber_read_sequence_tag(s, &length); /* NegoDataItem */
		ber_read_contextual_tag(s, 0, &length, true); /* [0] negoToken */
		ber_read_octet_string_tag(s, &length); /* OCTET STRING */
		sspi_SecBufferAlloc(&credssp->negoToken, length);
		stream_read(s, credssp->negoToken.pvBuffer, length);
		credssp->negoToken.cbBuffer = length;
	}

	/* [2] authInfo (OCTET STRING) */
	if (ber_read_contextual_tag(s, 2, &length, true) != false)
	{
		ber_read_octet_string_tag(s, &length); /* OCTET STRING */
		sspi_SecBufferAlloc(&credssp->authInfo, length);
		stream_read(s, credssp->authInfo.pvBuffer, length);
		credssp->authInfo.cbBuffer = length;
	}

	/* [3] pubKeyAuth (OCTET STRING) */
	if (ber_read_contextual_tag(s, 3, &length, true) != false)
	{
		ber_read_octet_string_tag(s, &length); /* OCTET STRING */
		sspi_SecBufferAlloc(&credssp->pubKeyAuth, length);
		stream_read(s, credssp->pubKeyAuth.pvBuffer, length);
		credssp->pubKeyAuth.cbBuffer = length;
	}

	stream_free(s);

	return 0;
}

void credssp_buffer_print(rdpCredssp* credssp)
{
	if (credssp->negoToken.cbBuffer > 0)
	{
		printf("CredSSP.negoToken (length = %d):\n", (int) credssp->negoToken.cbBuffer);
		winpr_HexDump(credssp->negoToken.pvBuffer, credssp->negoToken.cbBuffer);
	}

	if (credssp->pubKeyAuth.cbBuffer > 0)
	{
		printf("CredSSP.pubKeyAuth (length = %d):\n", (int) credssp->pubKeyAuth.cbBuffer);
		winpr_HexDump(credssp->pubKeyAuth.pvBuffer, credssp->pubKeyAuth.cbBuffer);
	}

	if (credssp->authInfo.cbBuffer > 0)
	{
		printf("CredSSP.authInfo (length = %d):\n", (int) credssp->authInfo.cbBuffer);
		winpr_HexDump(credssp->authInfo.pvBuffer, credssp->authInfo.cbBuffer);
	}
}

void credssp_buffer_free(rdpCredssp* credssp)
{
	sspi_SecBufferFree(&credssp->negoToken);
	sspi_SecBufferFree(&credssp->pubKeyAuth);
	sspi_SecBufferFree(&credssp->authInfo);
}

/**
 * Create new CredSSP state machine.
 * @param transport
 * @return new CredSSP state machine.
 */

rdpCredssp* credssp_new(freerdp* instance, rdpTls* tls, rdpSettings* settings)
{
	rdpCredssp* credssp;

	credssp = (rdpCredssp*) xzalloc(sizeof(rdpCredssp));

	if (credssp != NULL)
	{
		credssp->instance = instance;
		credssp->settings = settings;
		credssp->server = settings->server_mode;
		credssp->tls = tls;
		credssp->send_seq_num = 0;
		credssp->recv_seq_num = 0;
		memset(&credssp->negoToken, 0, sizeof(SecBuffer));
		memset(&credssp->pubKeyAuth, 0, sizeof(SecBuffer));
		memset(&credssp->authInfo, 0, sizeof(SecBuffer));
	}

	return credssp;
}

/**
 * Free CredSSP state machine.
 * @param credssp
 */

void credssp_free(rdpCredssp* credssp)
{
	if (credssp != NULL)
	{
		if (credssp->table)
			credssp->table->DeleteSecurityContext(&credssp->context);

		sspi_SecBufferFree(&credssp->PublicKey);
		sspi_SecBufferFree(&credssp->ts_credentials);

		free(credssp->identity.User);
		free(credssp->identity.Domain);
		free(credssp->identity.Password);
		free(credssp);
	}
}

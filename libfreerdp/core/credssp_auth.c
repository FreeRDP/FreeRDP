/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2022 David Fort <contact@hardening-consulting.com>
 * Copyright 2022 Isaac Klein <fifthdegree@protonmail.com>
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

#include <ctype.h>

#include <freerdp/config.h>
#include "settings.h"
#include <freerdp/build-config.h>
#include <freerdp/peer.h>

#include <winpr/crt.h>
#include <winpr/wtypes.h>
#include <winpr/assert.h>
#include <winpr/library.h>
#include <winpr/registry.h>
#include <winpr/sspi.h>

#include <freerdp/log.h>

#include "utils.h"
#include "credssp_auth.h"

#define TAG FREERDP_TAG("core.auth")

#define SERVER_KEY "Software\\" FREERDP_VENDOR_STRING "\\" FREERDP_PRODUCT_STRING "\\Server"

enum AUTH_STATE
{
	AUTH_STATE_INITIAL,
	AUTH_STATE_CREDS,
	AUTH_STATE_IN_PROGRESS,
	AUTH_STATE_FINAL
};

struct rdp_credssp_auth
{
	const rdpContext* rdp_ctx;
	SecurityFunctionTable* table;
	SecPkgInfo* info;
	SEC_WINNT_AUTH_IDENTITY identity;
	SEC_WINPR_NTLM_SETTINGS ntlmSettings;
	SEC_WINPR_KERBEROS_SETTINGS kerberosSettings;
	CredHandle credentials;
	BOOL server;
	SecPkgContext_Bindings* bindings;
	TCHAR* spn;
	WCHAR* package_list;
	CtxtHandle context;
	SecBuffer input_buffer;
	SecBuffer output_buffer;
	ULONG flags;
	SecPkgContext_Sizes sizes;
	SECURITY_STATUS sspi_error;
	enum AUTH_STATE state;
	char* pkgNameA;
};

static const char* credssp_auth_state_string(const rdpCredsspAuth* auth)
{
	WINPR_ASSERT(auth);
	switch (auth->state)
	{
		case AUTH_STATE_INITIAL:
			return "AUTH_STATE_INITIAL";
		case AUTH_STATE_CREDS:
			return "AUTH_STATE_CREDS";
		case AUTH_STATE_IN_PROGRESS:
			return "AUTH_STATE_IN_PROGRESS";
		case AUTH_STATE_FINAL:
			return "AUTH_STATE_FINAL";
		default:
			return "AUTH_STATE_UNKNOWN";
	}
}
static BOOL parseKerberosDeltat(const char* value, INT32* dest, const char* message);
static BOOL credssp_auth_setup_identity(rdpCredsspAuth* auth);
static SecurityFunctionTable* auth_resolve_sspi_table(const rdpSettings* settings);

static BOOL credssp_auth_update_name_cache(rdpCredsspAuth* auth, TCHAR* name)
{
	WINPR_ASSERT(auth);

	free(auth->pkgNameA);
	auth->pkgNameA = NULL;
	if (name)
#if defined(UNICODE)
		auth->pkgNameA = ConvertWCharToUtf8Alloc(name, NULL);
#else
		auth->pkgNameA = _strdup(name);
#endif
	return TRUE;
}
rdpCredsspAuth* credssp_auth_new(const rdpContext* rdp_ctx)
{
	rdpCredsspAuth* auth = calloc(1, sizeof(rdpCredsspAuth));

	if (auth)
		auth->rdp_ctx = rdp_ctx;

	return auth;
}

BOOL credssp_auth_init(rdpCredsspAuth* auth, TCHAR* pkg_name, SecPkgContext_Bindings* bindings)
{
	WINPR_ASSERT(auth);
	WINPR_ASSERT(auth->rdp_ctx);

	const rdpSettings* settings = auth->rdp_ctx->settings;
	WINPR_ASSERT(settings);

	if (!credssp_auth_update_name_cache(auth, pkg_name))
		return FALSE;

	auth->table = auth_resolve_sspi_table(settings);
	if (!auth->table)
	{
		WLog_ERR(TAG, "Unable to initialize sspi table");
		return FALSE;
	}

	/* Package name will be stored in the info structure */
	WINPR_ASSERT(auth->table->QuerySecurityPackageInfo);
	const SECURITY_STATUS status = auth->table->QuerySecurityPackageInfo(pkg_name, &auth->info);
	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "QuerySecurityPackageInfo (%s) failed with %s [0x%08X]",
		         credssp_auth_pkg_name(auth), GetSecurityStatusString(status), status);
		return FALSE;
	}

	if (!credssp_auth_update_name_cache(auth, auth->info->Name))
		return FALSE;

	WLog_DBG(TAG, "Using package: %s (cbMaxToken: %u bytes)", credssp_auth_pkg_name(auth),
	         auth->info->cbMaxToken);

	/* Setup common identity settings */
	if (!credssp_auth_setup_identity(auth))
		return FALSE;

	auth->bindings = bindings;

	return TRUE;
}

static BOOL credssp_auth_setup_auth_data(rdpCredsspAuth* auth,
                                         const SEC_WINNT_AUTH_IDENTITY* identity,
                                         SEC_WINNT_AUTH_IDENTITY_WINPR* pAuthData)
{
	SEC_WINNT_AUTH_IDENTITY_EXW* identityEx = NULL;

	WINPR_ASSERT(pAuthData);
	ZeroMemory(pAuthData, sizeof(SEC_WINNT_AUTH_IDENTITY_WINPR));

	identityEx = &pAuthData->identity;
	identityEx->Version = SEC_WINNT_AUTH_IDENTITY_VERSION;
	identityEx->Length = sizeof(SEC_WINNT_AUTH_IDENTITY_EX);
	identityEx->User = identity->User;
	identityEx->UserLength = identity->UserLength;
	identityEx->Domain = identity->Domain;
	identityEx->DomainLength = identity->DomainLength;
	identityEx->Password = identity->Password;
	identityEx->PasswordLength = identity->PasswordLength;
	identityEx->Flags = identity->Flags;
	identityEx->Flags |= SEC_WINNT_AUTH_IDENTITY_UNICODE;
	identityEx->Flags |= SEC_WINNT_AUTH_IDENTITY_EXTENDED;

	if (auth->package_list)
	{
		identityEx->PackageList = (UINT16*)auth->package_list;
		identityEx->PackageListLength = _wcslen(auth->package_list);
	}

	pAuthData->ntlmSettings = &auth->ntlmSettings;
	pAuthData->kerberosSettings = &auth->kerberosSettings;

	return TRUE;
}

static BOOL credssp_auth_client_init_cred_attributes(rdpCredsspAuth* auth)
{
	WINPR_ASSERT(auth);

	if (!utils_str_is_empty(auth->kerberosSettings.kdcUrl))
	{
		SECURITY_STATUS status = ERROR_INTERNAL_ERROR;
		SecPkgCredentials_KdcProxySettingsW* secAttr = NULL;
		SSIZE_T str_size = 0;
		ULONG buffer_size = 0;

		str_size = ConvertUtf8ToWChar(auth->kerberosSettings.kdcUrl, NULL, 0);
		if (str_size <= 0)
			return FALSE;
		str_size++;

		buffer_size = sizeof(SecPkgCredentials_KdcProxySettingsW) + str_size * sizeof(WCHAR);
		secAttr = calloc(1, buffer_size);
		if (!secAttr)
			return FALSE;

		secAttr->Version = KDC_PROXY_SETTINGS_V1;
		secAttr->ProxyServerLength = str_size * sizeof(WCHAR);
		secAttr->ProxyServerOffset = sizeof(SecPkgCredentials_KdcProxySettingsW);

		if (ConvertUtf8ToWChar(auth->kerberosSettings.kdcUrl, (WCHAR*)(secAttr + 1), str_size) <= 0)
		{
			free(secAttr);
			return FALSE;
		}

#ifdef UNICODE
		if (auth->table->SetCredentialsAttributesW)
			status = auth->table->SetCredentialsAttributesW(&auth->credentials,
			                                                SECPKG_CRED_ATTR_KDC_PROXY_SETTINGS,
			                                                (void*)secAttr, buffer_size);
		else
			status = SEC_E_UNSUPPORTED_FUNCTION;
#else
		if (auth->table->SetCredentialsAttributesA)
			status = auth->table->SetCredentialsAttributesA(&auth->credentials,
			                                                SECPKG_CRED_ATTR_KDC_PROXY_SETTINGS,
			                                                (void*)secAttr, buffer_size);
		else
			status = SEC_E_UNSUPPORTED_FUNCTION;
#endif

		if (status != SEC_E_OK)
		{
			WLog_WARN(TAG, "Explicit Kerberos KDC URL (%s) injection is not supported",
			          auth->kerberosSettings.kdcUrl);
		}

		free(secAttr);
	}

	return TRUE;
}

BOOL credssp_auth_setup_client(rdpCredsspAuth* auth, const char* target_service,
                               const char* target_hostname, const SEC_WINNT_AUTH_IDENTITY* identity,
                               const char* pkinit)
{
	void* pAuthData = NULL;
	SEC_WINNT_AUTH_IDENTITY_WINPR winprAuthData = { 0 };

	WINPR_ASSERT(auth);
	WINPR_ASSERT(auth->table);
	WINPR_ASSERT(auth->info);

	WINPR_ASSERT(auth->state == AUTH_STATE_INITIAL);

	/* Construct the service principal name */
	if (!credssp_auth_set_spn(auth, target_service, target_hostname))
		return FALSE;

	if (identity)
	{
		credssp_auth_setup_auth_data(auth, identity, &winprAuthData);

		if (pkinit)
		{
			auth->kerberosSettings.pkinitX509Identity = _strdup(pkinit);
			if (!auth->kerberosSettings.pkinitX509Identity)
			{
				WLog_ERR(TAG, "unable to copy pkinitArgs");
				return FALSE;
			}
		}

		pAuthData = (void*)&winprAuthData;
	}

	WINPR_ASSERT(auth->table->AcquireCredentialsHandle);
	const SECURITY_STATUS status =
	    auth->table->AcquireCredentialsHandle(NULL, auth->info->Name, SECPKG_CRED_OUTBOUND, NULL,
	                                          pAuthData, NULL, NULL, &auth->credentials, NULL);

	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "AcquireCredentialsHandleA failed with %s [0x%08X]",
		         GetSecurityStatusString(status), status);
		return FALSE;
	}

	if (!credssp_auth_client_init_cred_attributes(auth))
	{
		WLog_ERR(TAG, "Fatal error setting credential attributes");
		return FALSE;
	}

	auth->state = AUTH_STATE_CREDS;
	WLog_DBG(TAG, "Acquired client credentials");

	return TRUE;
}

BOOL credssp_auth_setup_server(rdpCredsspAuth* auth)
{
	void* pAuthData = NULL;
	SEC_WINNT_AUTH_IDENTITY_WINPR winprAuthData = { 0 };

	WINPR_ASSERT(auth);
	WINPR_ASSERT(auth->table);

	WINPR_ASSERT(auth->state == AUTH_STATE_INITIAL);

	if (auth->ntlmSettings.samFile || auth->ntlmSettings.hashCallback ||
	    auth->kerberosSettings.keytab)
	{
		credssp_auth_setup_auth_data(auth, &auth->identity, &winprAuthData);
		pAuthData = &winprAuthData;
	}

	WINPR_ASSERT(auth->table->AcquireCredentialsHandle);
	const SECURITY_STATUS status =
	    auth->table->AcquireCredentialsHandle(NULL, auth->info->Name, SECPKG_CRED_INBOUND, NULL,
	                                          pAuthData, NULL, NULL, &auth->credentials, NULL);
	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "AcquireCredentialsHandleA failed with %s [0x%08X]",
		         GetSecurityStatusString(status), status);
		return FALSE;
	}

	auth->state = AUTH_STATE_CREDS;
	WLog_DBG(TAG, "Acquired server credentials");

	auth->server = TRUE;

	return TRUE;
}

void credssp_auth_set_flags(rdpCredsspAuth* auth, ULONG flags)
{
	WINPR_ASSERT(auth);
	auth->flags = flags;
}

/**
 *                                        SSPI Client Ceremony
 *
 *                                           --------------
 *                                          ( Client Begin )
 *                                           --------------
 *                                                 |
 *                                                 |
 *                                                \|/
 *                                      -----------+--------------
 *                                     | AcquireCredentialsHandle |
 *                                      --------------------------
 *                                                 |
 *                                                 |
 *                                                \|/
 *                                    -------------+--------------
 *                 +---------------> / InitializeSecurityContext /
 *                 |                 ----------------------------
 *                 |                               |
 *                 |                               |
 *                 |                              \|/
 *     ---------------------------        ---------+-------------            ----------------------
 *    / Receive blob from server /      < Received security blob? > --Yes-> / Send blob to server /
 *    -------------+-------------         -----------------------           ----------------------
 *                /|\                              |                                |
 *                 |                               No                               |
 *                Yes                             \|/                               |
 *                 |                   ------------+-----------                     |
 *                 +---------------- < Received Continue Needed > <-----------------+
 *                                     ------------------------
 *                                                 |
 *                                                 No
 *                                                \|/
 *                                           ------+-------
 *                                          (  Client End  )
 *                                           --------------
 */

int credssp_auth_authenticate(rdpCredsspAuth* auth)
{
	SECURITY_STATUS status = ERROR_INTERNAL_ERROR;
	SecBuffer input_buffers[2] = { 0 };
	SecBufferDesc input_buffer_desc = { SECBUFFER_VERSION, 1, input_buffers };
	CtxtHandle* context = NULL;

	WINPR_ASSERT(auth);
	WINPR_ASSERT(auth->table);

	SecBufferDesc output_buffer_desc = { SECBUFFER_VERSION, 1, &auth->output_buffer };

	switch (auth->state)
	{
		case AUTH_STATE_CREDS:
		case AUTH_STATE_IN_PROGRESS:
			break;
		case AUTH_STATE_INITIAL:
		case AUTH_STATE_FINAL:
			WLog_ERR(TAG, "context in invalid state!");
			return -1;
	}

	/* input buffer will be null on first call,
	 * context MUST be NULL on first call */
	context = &auth->context;
	if (!auth->context.dwLower && !auth->context.dwUpper)
		context = NULL;

	input_buffers[0] = auth->input_buffer;

	if (auth->bindings)
	{
		input_buffer_desc.cBuffers = 2;

		input_buffers[1].BufferType = SECBUFFER_CHANNEL_BINDINGS;
		input_buffers[1].cbBuffer = auth->bindings->BindingsLength;
		input_buffers[1].pvBuffer = auth->bindings->Bindings;
	}

	/* Free previous output buffer (presumably no longer needed) */
	sspi_SecBufferFree(&auth->output_buffer);
	auth->output_buffer.BufferType = SECBUFFER_TOKEN;
	if (!sspi_SecBufferAlloc(&auth->output_buffer, auth->info->cbMaxToken))
		return -1;

	if (auth->server)
	{
		WINPR_ASSERT(auth->table->AcceptSecurityContext);
		status = auth->table->AcceptSecurityContext(
		    &auth->credentials, context, &input_buffer_desc, auth->flags, SECURITY_NATIVE_DREP,
		    &auth->context, &output_buffer_desc, &auth->flags, NULL);
	}
	else
	{
		WINPR_ASSERT(auth->table->InitializeSecurityContext);
		status = auth->table->InitializeSecurityContext(
		    &auth->credentials, context, auth->spn, auth->flags, 0, SECURITY_NATIVE_DREP,
		    &input_buffer_desc, 0, &auth->context, &output_buffer_desc, &auth->flags, NULL);
	}

	if (status == SEC_E_OK)
	{
		WLog_DBG(TAG, "Authentication complete (output token size: %" PRIu32 " bytes)",
		         auth->output_buffer.cbBuffer);
		auth->state = AUTH_STATE_FINAL;

		/* Not terrible if this fails, although encryption functions may run into issues down the
		 * line, still, authentication succeeded */
		WINPR_ASSERT(auth->table->QueryContextAttributes);
		status =
		    auth->table->QueryContextAttributes(&auth->context, SECPKG_ATTR_SIZES, &auth->sizes);
		WLog_DBG(TAG, "QueryContextAttributes returned %s [0x%08" PRIx32 "]",
		         GetSecurityStatusString(status), status);
		WLog_DBG(TAG, "Context sizes: cbMaxSignature=%d, cbSecurityTrailer=%d",
		         auth->sizes.cbMaxSignature, auth->sizes.cbSecurityTrailer);

		return 1;
	}
	else if (status == SEC_I_CONTINUE_NEEDED)
	{
		WLog_DBG(TAG, "Authentication in progress... (output token size: %" PRIu32 ")",
		         auth->output_buffer.cbBuffer);
		auth->state = AUTH_STATE_IN_PROGRESS;
		return 0;
	}
	else
	{
		WLog_ERR(TAG, "%s failed with %s [0x%08X]",
		         auth->server ? "AcceptSecurityContext" : "InitializeSecurityContext",
		         GetSecurityStatusString(status), status);
		auth->sspi_error = status;
		return -1;
	}
}

/* Plaintext is not modified; Output buffer MUST be freed if encryption succeeds */
BOOL credssp_auth_encrypt(rdpCredsspAuth* auth, const SecBuffer* plaintext, SecBuffer* ciphertext,
                          size_t* signature_length, ULONG sequence)
{
	SECURITY_STATUS status = ERROR_INTERNAL_ERROR;
	SecBuffer buffers[2] = { 0 };
	SecBufferDesc buffer_desc = { SECBUFFER_VERSION, 2, buffers };
	BYTE* buf = NULL;

	WINPR_ASSERT(auth && auth->table);
	WINPR_ASSERT(plaintext);
	WINPR_ASSERT(ciphertext);

	switch (auth->state)
	{
		case AUTH_STATE_INITIAL:
			WLog_ERR(TAG, "Invalid state %s", credssp_auth_state_string(auth));
			return FALSE;
		default:
			break;
	}

	/* Allocate consecutive memory for ciphertext and signature */
	buf = calloc(1, plaintext->cbBuffer + auth->sizes.cbSecurityTrailer);
	if (!buf)
		return FALSE;

	buffers[0].BufferType = SECBUFFER_TOKEN;
	buffers[0].cbBuffer = auth->sizes.cbSecurityTrailer;
	buffers[0].pvBuffer = buf;

	buffers[1].BufferType = SECBUFFER_DATA;
	if (plaintext->BufferType & SECBUFFER_READONLY)
		buffers[1].BufferType |= SECBUFFER_READONLY;
	buffers[1].pvBuffer = buf + auth->sizes.cbSecurityTrailer;
	buffers[1].cbBuffer = plaintext->cbBuffer;
	CopyMemory(buffers[1].pvBuffer, plaintext->pvBuffer, plaintext->cbBuffer);

	WINPR_ASSERT(auth->table->EncryptMessage);
	status = auth->table->EncryptMessage(&auth->context, 0, &buffer_desc, sequence);
	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "EncryptMessage failed with %s [0x%08X]", GetSecurityStatusString(status),
		         status);
		free(buf);
		return FALSE;
	}

	if (buffers[0].cbBuffer < auth->sizes.cbSecurityTrailer)
	{
		/* The signature is smaller than cbSecurityTrailer, so shrink the excess in between */
		MoveMemory(((BYTE*)buffers[0].pvBuffer) + buffers[0].cbBuffer, buffers[1].pvBuffer,
		           buffers[1].cbBuffer);
		// use reported signature size as new cbSecurityTrailer value for DecryptMessage
		auth->sizes.cbSecurityTrailer = buffers[0].cbBuffer;
	}

	ciphertext->cbBuffer = buffers[0].cbBuffer + buffers[1].cbBuffer;
	ciphertext->pvBuffer = buf;

	if (signature_length)
		*signature_length = buffers[0].cbBuffer;

	return TRUE;
}

/* Output buffer MUST be freed if decryption succeeds */
BOOL credssp_auth_decrypt(rdpCredsspAuth* auth, const SecBuffer* ciphertext, SecBuffer* plaintext,
                          ULONG sequence)
{
	SecBuffer buffers[2];
	SecBufferDesc buffer_desc = { SECBUFFER_VERSION, 2, buffers };
	ULONG fqop = 0;

	WINPR_ASSERT(auth && auth->table);
	WINPR_ASSERT(ciphertext);
	WINPR_ASSERT(plaintext);

	switch (auth->state)
	{
		case AUTH_STATE_INITIAL:
			WLog_ERR(TAG, "Invalid state %s", credssp_auth_state_string(auth));
			return FALSE;
		default:
			break;
	}

	/* Sanity check: ciphertext must at least have a signature */
	if (ciphertext->cbBuffer < auth->sizes.cbSecurityTrailer)
	{
		WLog_ERR(TAG, "Encrypted message buffer too small");
		return FALSE;
	}

	/* Split the input into signature and encrypted data; we assume the signature length is equal to
	 * cbSecurityTrailer */
	buffers[0].BufferType = SECBUFFER_TOKEN;
	buffers[0].pvBuffer = ciphertext->pvBuffer;
	buffers[0].cbBuffer = auth->sizes.cbSecurityTrailer;

	buffers[1].BufferType = SECBUFFER_DATA;
	if (!sspi_SecBufferAlloc(&buffers[1], ciphertext->cbBuffer - auth->sizes.cbSecurityTrailer))
		return FALSE;
	CopyMemory(buffers[1].pvBuffer, (BYTE*)ciphertext->pvBuffer + auth->sizes.cbSecurityTrailer,
	           buffers[1].cbBuffer);

	WINPR_ASSERT(auth->table->DecryptMessage);
	const SECURITY_STATUS status =
	    auth->table->DecryptMessage(&auth->context, &buffer_desc, sequence, &fqop);
	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "DecryptMessage failed with %s [0x%08X]", GetSecurityStatusString(status),
		         status);
		sspi_SecBufferFree(&buffers[1]);
		return FALSE;
	}

	*plaintext = buffers[1];

	return TRUE;
}

BOOL credssp_auth_impersonate(rdpCredsspAuth* auth)
{
	WINPR_ASSERT(auth && auth->table);

	WINPR_ASSERT(auth->table->ImpersonateSecurityContext);
	const SECURITY_STATUS status = auth->table->ImpersonateSecurityContext(&auth->context);

	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "ImpersonateSecurityContext failed with %s [0x%08X]",
		         GetSecurityStatusString(status), status);
		return FALSE;
	}

	return TRUE;
}

BOOL credssp_auth_revert_to_self(rdpCredsspAuth* auth)
{
	WINPR_ASSERT(auth && auth->table);

	WINPR_ASSERT(auth->table->RevertSecurityContext);
	const SECURITY_STATUS status = auth->table->RevertSecurityContext(&auth->context);

	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "RevertSecurityContext failed with %s [0x%08X]",
		         GetSecurityStatusString(status), status);
		return FALSE;
	}

	return TRUE;
}

void credssp_auth_take_input_buffer(rdpCredsspAuth* auth, SecBuffer* buffer)
{
	WINPR_ASSERT(auth);
	WINPR_ASSERT(buffer);

	sspi_SecBufferFree(&auth->input_buffer);

	auth->input_buffer = *buffer;
	auth->input_buffer.BufferType = SECBUFFER_TOKEN;

	/* Invalidate original, rdpCredsspAuth now has ownership of the buffer */
	SecBuffer empty = { 0 };
	*buffer = empty;
}

const SecBuffer* credssp_auth_get_output_buffer(rdpCredsspAuth* auth)
{
	WINPR_ASSERT(auth);
	return &auth->output_buffer;
}

BOOL credssp_auth_have_output_token(rdpCredsspAuth* auth)
{
	WINPR_ASSERT(auth);
	return auth->output_buffer.cbBuffer ? TRUE : FALSE;
}

BOOL credssp_auth_is_complete(rdpCredsspAuth* auth)
{
	WINPR_ASSERT(auth);
	return auth->state == AUTH_STATE_FINAL;
}

size_t credssp_auth_trailer_size(rdpCredsspAuth* auth)
{
	WINPR_ASSERT(auth);
	return auth->sizes.cbSecurityTrailer;
}

const char* credssp_auth_pkg_name(rdpCredsspAuth* auth)
{
	WINPR_ASSERT(auth && auth->info);
	return auth->pkgNameA;
}

UINT32 credssp_auth_sspi_error(rdpCredsspAuth* auth)
{
	WINPR_ASSERT(auth);
	return (UINT32)auth->sspi_error;
}

void credssp_auth_tableAndContext(rdpCredsspAuth* auth, SecurityFunctionTable** ptable,
                                  CtxtHandle* pcontext)
{
	WINPR_ASSERT(auth);
	WINPR_ASSERT(ptable);
	WINPR_ASSERT(pcontext);

	*ptable = auth->table;
	*pcontext = auth->context;
}

void credssp_auth_free(rdpCredsspAuth* auth)
{
	SEC_WINPR_KERBEROS_SETTINGS* krb_settings = NULL;
	SEC_WINPR_NTLM_SETTINGS* ntlm_settings = NULL;

	if (!auth)
		return;

	if (auth->table)
	{
		switch (auth->state)
		{
			case AUTH_STATE_IN_PROGRESS:
			case AUTH_STATE_FINAL:
				WINPR_ASSERT(auth->table->DeleteSecurityContext);
				auth->table->DeleteSecurityContext(&auth->context);
				/* fallthrouth */
				WINPR_FALLTHROUGH
			case AUTH_STATE_CREDS:
				WINPR_ASSERT(auth->table->FreeCredentialsHandle);
				auth->table->FreeCredentialsHandle(&auth->credentials);
				break;
			case AUTH_STATE_INITIAL:
			default:
				break;
		}

		if (auth->info)
		{
			WINPR_ASSERT(auth->table->FreeContextBuffer);
			auth->table->FreeContextBuffer(auth->info);
		}
	}

	sspi_FreeAuthIdentity(&auth->identity);

	krb_settings = &auth->kerberosSettings;
	ntlm_settings = &auth->ntlmSettings;

	free(krb_settings->kdcUrl);
	free(krb_settings->cache);
	free(krb_settings->keytab);
	free(krb_settings->armorCache);
	free(krb_settings->pkinitX509Anchors);
	free(krb_settings->pkinitX509Identity);
	free(ntlm_settings->samFile);

	free(auth->package_list);
	free(auth->spn);
	sspi_SecBufferFree(&auth->input_buffer);
	sspi_SecBufferFree(&auth->output_buffer);
	credssp_auth_update_name_cache(auth, NULL);
	free(auth);
}

static void auth_get_sspi_module_from_reg(char** sspi_module)
{
	HKEY hKey = NULL;
	DWORD dwType = 0;
	DWORD dwSize = 0;

	WINPR_ASSERT(sspi_module);
	*sspi_module = NULL;

	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, SERVER_KEY, 0, KEY_READ | KEY_WOW64_64KEY, &hKey) !=
	    ERROR_SUCCESS)
		return;

	if (RegQueryValueExA(hKey, "SspiModule", NULL, &dwType, NULL, &dwSize) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return;
	}

	char* module = (LPSTR)calloc(dwSize + sizeof(CHAR), sizeof(char));
	if (!module)
	{
		RegCloseKey(hKey);
		return;
	}

	if (RegQueryValueExA(hKey, "SspiModule", NULL, &dwType, (BYTE*)module, &dwSize) !=
	    ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		free(module);
		return;
	}

	RegCloseKey(hKey);
	*sspi_module = module;
}

static SecurityFunctionTable* auth_resolve_sspi_table(const rdpSettings* settings)
{
	char* sspi_module = NULL;

	WINPR_ASSERT(settings);

	if (settings->ServerMode)
		auth_get_sspi_module_from_reg(&sspi_module);

	if (sspi_module || settings->SspiModule)
	{
		const char* module_name = sspi_module ? sspi_module : settings->SspiModule;
#ifdef UNICODE
		const char* proc_name = "InitSecurityInterfaceW";
#else
		const char* proc_name = "InitSecurityInterfaceA";
#endif /* UNICODE */

		HMODULE hSSPI = LoadLibraryX(module_name);

		if (!hSSPI)
		{
			WLog_ERR(TAG, "Failed to load SSPI module: %s", module_name);
			free(sspi_module);
			return FALSE;
		}

		WLog_INFO(TAG, "Using SSPI Module: %s", module_name);

		INIT_SECURITY_INTERFACE InitSecurityInterface_ptr =
		    GetProcAddressAs(hSSPI, proc_name, INIT_SECURITY_INTERFACE);
		if (!InitSecurityInterface_ptr)
		{
			WLog_ERR(TAG, "Failed to load SSPI module: %s, no function %s", module_name, proc_name);
			free(sspi_module);
			return FALSE;
		}
		free(sspi_module);
		return InitSecurityInterface_ptr();
	}

	return InitSecurityInterfaceEx(0);
}

static BOOL credssp_auth_setup_identity(rdpCredsspAuth* auth)
{
	const rdpSettings* settings = NULL;
	SEC_WINPR_KERBEROS_SETTINGS* krb_settings = NULL;
	SEC_WINPR_NTLM_SETTINGS* ntlm_settings = NULL;
	freerdp_peer* peer = NULL;

	WINPR_ASSERT(auth);
	WINPR_ASSERT(auth->rdp_ctx);

	peer = auth->rdp_ctx->peer;
	settings = auth->rdp_ctx->settings;
	WINPR_ASSERT(settings);

	krb_settings = &auth->kerberosSettings;
	ntlm_settings = &auth->ntlmSettings;

	if (settings->KerberosLifeTime)
		parseKerberosDeltat(settings->KerberosLifeTime, &krb_settings->lifeTime, "lifetime");
	if (settings->KerberosStartTime)
		parseKerberosDeltat(settings->KerberosStartTime, &krb_settings->startTime, "starttime");
	if (settings->KerberosRenewableLifeTime)
		parseKerberosDeltat(settings->KerberosRenewableLifeTime, &krb_settings->renewLifeTime,
		                    "renewLifeTime");

	if (settings->KerberosKdcUrl)
	{
		krb_settings->kdcUrl = _strdup(settings->KerberosKdcUrl);
		if (!krb_settings->kdcUrl)
		{
			WLog_ERR(TAG, "unable to copy kdcUrl");
			return FALSE;
		}
	}

	if (settings->KerberosCache)
	{
		krb_settings->cache = _strdup(settings->KerberosCache);
		if (!krb_settings->cache)
		{
			WLog_ERR(TAG, "unable to copy cache name");
			return FALSE;
		}
	}

	if (settings->KerberosKeytab)
	{
		krb_settings->keytab = _strdup(settings->KerberosKeytab);
		if (!krb_settings->keytab)
		{
			WLog_ERR(TAG, "unable to copy keytab name");
			return FALSE;
		}
	}

	if (settings->KerberosArmor)
	{
		krb_settings->armorCache = _strdup(settings->KerberosArmor);
		if (!krb_settings->armorCache)
		{
			WLog_ERR(TAG, "unable to copy armorCache");
			return FALSE;
		}
	}

	if (settings->PkinitAnchors)
	{
		krb_settings->pkinitX509Anchors = _strdup(settings->PkinitAnchors);
		if (!krb_settings->pkinitX509Anchors)
		{
			WLog_ERR(TAG, "unable to copy pkinitX509Anchors");
			return FALSE;
		}
	}

	if (settings->NtlmSamFile)
	{
		ntlm_settings->samFile = _strdup(settings->NtlmSamFile);
		if (!ntlm_settings->samFile)
		{
			WLog_ERR(TAG, "unable to copy samFile");
			return FALSE;
		}
	}

	if (peer && peer->SspiNtlmHashCallback)
	{
		ntlm_settings->hashCallback = peer->SspiNtlmHashCallback;
		ntlm_settings->hashCallbackArg = peer;
	}

	if (settings->AuthenticationPackageList)
	{
		auth->package_list = ConvertUtf8ToWCharAlloc(settings->AuthenticationPackageList, NULL);
		if (!auth->package_list)
			return FALSE;
	}

	auth->identity.Flags |= SEC_WINNT_AUTH_IDENTITY_UNICODE;
	auth->identity.Flags |= SEC_WINNT_AUTH_IDENTITY_EXTENDED;

	return TRUE;
}

BOOL credssp_auth_set_spn(rdpCredsspAuth* auth, const char* service, const char* hostname)
{
	size_t length = 0;
	char* spn = NULL;

	WINPR_ASSERT(auth);

	if (!hostname)
		return FALSE;

	if (!service)
		spn = _strdup(hostname);
	else
	{
		length = strlen(service) + strlen(hostname) + 2;
		spn = calloc(length + 1, sizeof(char));
		if (!spn)
			return FALSE;

		(void)sprintf_s(spn, length, "%s/%s", service, hostname);
	}
	if (!spn)
		return FALSE;

#if defined(UNICODE)
	auth->spn = ConvertUtf8ToWCharAlloc(spn, NULL);
	free(spn);
#else
	auth->spn = spn;
#endif

	return TRUE;
}

static const char* parseInt(const char* v, INT32* r)
{
	*r = 0;

	/* check that we have at least a digit */
	if (!*v || !((*v >= '0') && (*v <= '9')))
		return NULL;

	for (; *v && (*v >= '0') && (*v <= '9'); v++)
	{
		*r = (*r * 10) + (*v - '0');
	}

	return v;
}

static BOOL parseKerberosDeltat(const char* value, INT32* dest, const char* message)
{
	INT32 v = 0;
	const char* ptr = NULL;

	WINPR_ASSERT(value);
	WINPR_ASSERT(dest);
	WINPR_ASSERT(message);

	/* determine the format :
	 *   h:m[:s] (3:00:02) 			deltat in hours/minutes
	 *   <n>d<n>h<n>m<n>s 1d4h	    deltat in day/hours/minutes/seconds
	 *   <n>						deltat in seconds
	 */
	ptr = strchr(value, ':');
	if (ptr)
	{
		/* format like h:m[:s] */
		*dest = 0;
		value = parseInt(value, &v);
		if (!value || *value != ':')
		{
			WLog_ERR(TAG, "Invalid value for %s", message);
			return FALSE;
		}

		*dest = v * 3600;

		value = parseInt(value + 1, &v);
		if (!value || (*value != 0 && *value != ':') || (v > 60))
		{
			WLog_ERR(TAG, "Invalid value for %s", message);
			return FALSE;
		}
		*dest += v * 60;

		if (*value == ':')
		{
			/* have optional seconds */
			value = parseInt(value + 1, &v);
			if (!value || (*value != 0) || (v > 60))
			{
				WLog_ERR(TAG, "Invalid value for %s", message);
				return FALSE;
			}
			*dest += v;
		}
		return TRUE;
	}

	/* <n> or <n>d<n>h<n>m<n>s format */
	value = parseInt(value, &v);
	if (!value)
	{
		WLog_ERR(TAG, "Invalid value for %s", message);
		return FALSE;
	}

	if (!*value || isspace(*value))
	{
		/* interpret that as a value in seconds */
		*dest = v;
		return TRUE;
	}

	*dest = 0;
	do
	{
		INT32 factor = 0;
		INT32 maxValue = 0;

		switch (*value)
		{
			case 'd':
				factor = 3600 * 24;
				maxValue = 0;
				break;
			case 'h':
				factor = 3600;
				maxValue = 0;
				break;
			case 'm':
				factor = 60;
				maxValue = 60;
				break;
			case 's':
				factor = 1;
				maxValue = 60;
				break;
			default:
				WLog_ERR(TAG, "invalid value for unit %c when parsing %s", *value, message);
				return FALSE;
		}

		if ((maxValue > 0) && (v > maxValue))
		{
			WLog_ERR(TAG, "invalid value for unit %c when parsing %s", *value, message);
			return FALSE;
		}

		*dest += (v * factor);
		value++;
		if (!*value)
			return TRUE;

		value = parseInt(value, &v);
		if (!value || !*value)
		{
			WLog_ERR(TAG, "Invalid value for %s", message);
			return FALSE;
		}

	} while (TRUE);

	return TRUE;
}

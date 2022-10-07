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
#include <freerdp/build-config.h>
#include <freerdp/settings.h>
#include <freerdp/peer.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/library.h>
#include <winpr/registry.h>

#include <freerdp/log.h>

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
	SecurityFunctionTableA* table;
	SecPkgInfoA* info;
	SEC_WINNT_AUTH_IDENTITY_WINPR identity;
	CredHandle credentials;
	BOOL server;
	SecPkgContext_Bindings* bindings;
	char* spn;
	CtxtHandle context;
	SecBuffer input_buffer;
	SecBuffer output_buffer;
	ULONG flags;
	SecPkgContext_Sizes sizes;
	enum AUTH_STATE state;
};

static BOOL parseKerberosDeltat(const char* value, INT32* dest, const char* message);
static BOOL auth_setup_identity(rdpCredsspAuth* auth, SEC_WINNT_AUTH_IDENTITY_WINPR* identity);
static SecurityFunctionTableA* auth_resolve_sspi_table(const rdpSettings* settings);

rdpCredsspAuth* credssp_auth_new(const rdpContext* rdp_ctx)
{
	rdpCredsspAuth* auth = calloc(1, sizeof(rdpCredsspAuth));

	if (auth)
		auth->rdp_ctx = rdp_ctx;

	return auth;
}

BOOL credssp_auth_init(rdpCredsspAuth* auth, char* pkg_name, SecPkgContext_Bindings* bindings)
{
	SECURITY_STATUS status;

	WINPR_ASSERT(auth);
	WINPR_ASSERT(auth->rdp_ctx);

	const rdpSettings* settings = auth->rdp_ctx->settings;
	WINPR_ASSERT(settings);

	auth->table = auth_resolve_sspi_table(settings);
	if (!auth->table)
	{
		WLog_ERR(TAG, "Unable to initialize sspi table");
		return FALSE;
	}

	/* Package name will be stored in the info structure */
	status = auth->table->QuerySecurityPackageInfoA(pkg_name, &auth->info);
	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "QuerySecurityPackageInfoA (%s) failed with %s [0x%08X]", pkg_name,
		         GetSecurityStatusString(status), status);
		return FALSE;
	}

	WLog_DBG(TAG, "Using package: %s (cbMaxToken: %u bytes)", pkg_name, auth->info->cbMaxToken);

	/* Setup common identity settings */
	if (!auth_setup_identity(auth, &auth->identity))
		return FALSE;

	auth->bindings = bindings;

	return TRUE;
}

BOOL credssp_auth_setup_client(rdpCredsspAuth* auth, const char* target_service,
                               const char* target_hostname, const SEC_WINNT_AUTH_IDENTITY* identity,
                               const char* pkinit)
{
	SECURITY_STATUS status;
	void* identityPtr = NULL;

	WINPR_ASSERT(auth);
	WINPR_ASSERT(auth->table);
	WINPR_ASSERT(auth->info);

	/* Construct the service principal name */
	if (!credssp_auth_set_spn(auth, target_service, target_hostname))
		return FALSE;

	if (identity)
	{
		if (sspi_CopyAuthIdentity(&auth->identity.identity, identity) < 0)
			return FALSE;
		auth->identity.identity.Flags |= SEC_WINNT_AUTH_IDENTITY_EXTENDED;

		if (pkinit)
		{
			auth->identity.kerberosSettings.pkinitX509Identity = _strdup(pkinit);
			if (!auth->identity.kerberosSettings.pkinitX509Identity)
			{
				WLog_ERR(TAG, "unable to copy pkinitArgs");
				return FALSE;
			}
		}

		identityPtr = &auth->identity;
	}

	status =
	    auth->table->AcquireCredentialsHandleA(NULL, auth->info->Name, SECPKG_CRED_OUTBOUND, NULL,
	                                           identityPtr, NULL, NULL, &auth->credentials, NULL);

	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "AcquireCredentialsHandleA failed with %s [0x%08X]",
		         GetSecurityStatusString(status), status);
		return FALSE;
	}

	if (auth->identity.kerberosSettings.kdcUrl)
	{
		SecPkgCredentials_KdcUrlA attr = { auth->identity.kerberosSettings.kdcUrl };

		if (auth->table->SetCredentialsAttributesA)
			status = auth->table->SetCredentialsAttributesA(
			    &auth->credentials, SECPKG_CRED_ATTR_KDC_URL, &attr, sizeof(attr));
		else
			status = SEC_E_UNSUPPORTED_FUNCTION;

		if (status != SEC_E_OK)
			WLog_WARN(TAG, "Explicit Kerberos KDC URL (%s) injection is not supported",
			          attr.KdcUrl);
	}

	auth->state = AUTH_STATE_CREDS;
	WLog_DBG(TAG, "Acquired client credentials");

	return TRUE;
}

BOOL credssp_auth_setup_server(rdpCredsspAuth* auth)
{
	SECURITY_STATUS status;
	void* auth_data = NULL;

	WINPR_ASSERT(auth);
	WINPR_ASSERT(auth->table);

	if (auth->identity.ntlmSettings.samFile || auth->identity.ntlmSettings.hashCallback ||
	    auth->identity.kerberosSettings.keytab)
		auth_data = &auth->identity;

	status =
	    auth->table->AcquireCredentialsHandleA(NULL, auth->info->Name, SECPKG_CRED_INBOUND, NULL,
	                                           auth_data, NULL, NULL, &auth->credentials, NULL);
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
	SECURITY_STATUS status;
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
			WLog_ERR(TAG, "%s: context in invalid state!", __FUNCTION__);
			return -1;
	}

	/* Context and input buffer will be null on first call */
	context = &auth->context;
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
		status = auth->table->AcceptSecurityContext(
		    &auth->credentials, context, &input_buffer_desc, auth->flags, SECURITY_NATIVE_DREP,
		    &auth->context, &output_buffer_desc, &auth->flags, NULL);
	else
		status = auth->table->InitializeSecurityContextA(
		    &auth->credentials, context, auth->spn, auth->flags, 0, SECURITY_NATIVE_DREP,
		    &input_buffer_desc, 0, &auth->context, &output_buffer_desc, &auth->flags, NULL);

	if (status == SEC_E_OK)
	{
		WLog_DBG(TAG, "Authentication complete (output token size: %lu bytes)",
		         auth->output_buffer.cbBuffer);
		auth->state = AUTH_STATE_FINAL;

		/* Not terrible if this fails, although encryption functions may run into issues down the
		 * line, still, authentication succeeded */
		auth->table->QueryContextAttributesA(&auth->context, SECPKG_ATTR_SIZES, &auth->sizes);
		WLog_DBG(TAG, "Context sizes: cbMaxSignature=%d, cbSecurityTrailer=%d",
		         auth->sizes.cbMaxSignature, auth->sizes.cbSecurityTrailer);

		return 1;
	}
	else if (status == SEC_I_CONTINUE_NEEDED)
	{
		WLog_DBG(TAG, "Authentication in progress... (output token size: %lu)",
		         auth->output_buffer.cbBuffer);
		auth->state = AUTH_STATE_IN_PROGRESS;
		return 0;
	}
	else
	{
		WLog_ERR(TAG, "%s failed with %s [0x%08X]",
		         auth->server ? "AcceptSecurityContext" : "InitializeSecurityContextA",
		         GetSecurityStatusString(status), status);
		return -1;
	}
}

/* Plaintext is not modified; Output buffer MUST be freed if encryption succeeds */
BOOL credssp_auth_encrypt(rdpCredsspAuth* auth, const SecBuffer* plaintext, SecBuffer* ciphertext,
                          size_t* signature_length, ULONG sequence)
{
	SECURITY_STATUS status;
	SecBuffer buffers[2];
	SecBufferDesc buffer_desc = { SECBUFFER_VERSION, 2, buffers };
	BYTE* buf = NULL;

	WINPR_ASSERT(auth && auth->table);
	WINPR_ASSERT(plaintext);
	WINPR_ASSERT(ciphertext);

	/* Allocate consecutive memory for ciphertext and signature. We assume the signature size is
	 * equal to cbSecurityTrailer */
	buf = calloc(1, plaintext->cbBuffer + auth->sizes.cbSecurityTrailer);
	if (!buf)
		return FALSE;

	buffers[0].BufferType = SECBUFFER_TOKEN;
	buffers[0].cbBuffer = auth->sizes.cbSecurityTrailer;
	buffers[0].pvBuffer = buf;

	buffers[1].BufferType = SECBUFFER_DATA;
	buffers[1].pvBuffer = buf + auth->sizes.cbSecurityTrailer;
	buffers[1].cbBuffer = plaintext->cbBuffer;
	CopyMemory(buffers[1].pvBuffer, plaintext->pvBuffer, plaintext->cbBuffer);

	status = auth->table->EncryptMessage(&auth->context, 0, &buffer_desc, sequence);
	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "EncryptMessage failed with %s [0x%08X]", GetSecurityStatusString(status),
		         status);
		free(buf);
		return FALSE;
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
	SECURITY_STATUS status;
	SecBuffer buffers[2];
	SecBufferDesc buffer_desc = { SECBUFFER_VERSION, 2, buffers };
	ULONG fqop;

	WINPR_ASSERT(auth && auth->table);
	WINPR_ASSERT(ciphertext);
	WINPR_ASSERT(plaintext);

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

	status = auth->table->DecryptMessage(&auth->context, &buffer_desc, sequence, &fqop);
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
	SECURITY_STATUS status;

	WINPR_ASSERT(auth && auth->table);

	status = auth->table->ImpersonateSecurityContext(&auth->context);

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
	SECURITY_STATUS status;

	WINPR_ASSERT(auth && auth->table);

	status = auth->table->RevertSecurityContext(&auth->context);

	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "RevertSecurityContext failed with %s [0x%08X]",
		         GetSecurityStatusString(status), status);
		return FALSE;
	}

	return TRUE;
}

void credssp_auth_set_input_buffer(rdpCredsspAuth* auth, SecBuffer* buffer)
{
	WINPR_ASSERT(auth);
	WINPR_ASSERT(buffer);

	auth->input_buffer = *buffer;
	auth->input_buffer.BufferType = SECBUFFER_TOKEN;
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
	return auth->info->Name;
}

void credssp_auth_free(rdpCredsspAuth* auth)
{
	SEC_WINPR_KERBEROS_SETTINGS* krb_settings;
	SEC_WINPR_NTLM_SETTINGS* ntlm_settings;

	if (!auth)
		return;

	if (auth->table)
	{
		switch (auth->state)
		{
			case AUTH_STATE_IN_PROGRESS:
			case AUTH_STATE_FINAL:
				auth->table->DeleteSecurityContext(&auth->context);
				/* FALLTHROUGH */
			case AUTH_STATE_CREDS:
				auth->table->FreeCredentialsHandle(&auth->credentials);
				break;
			case AUTH_STATE_INITIAL:
			default:
				break;
		}

		if (auth->info)
			auth->table->FreeContextBuffer(auth->info);
	}

	sspi_FreeAuthIdentity(&auth->identity.identity);

	krb_settings = &auth->identity.kerberosSettings;
	ntlm_settings = &auth->identity.ntlmSettings;

	free(krb_settings->kdcUrl);
	free(krb_settings->cache);
	free(krb_settings->keytab);
	free(krb_settings->armorCache);
	free(krb_settings->pkinitX509Anchors);
	free(krb_settings->pkinitX509Identity);
	free(ntlm_settings->samFile);

	free(auth->spn);
	sspi_SecBufferFree(&auth->output_buffer);

	free(auth);
}

static void auth_get_sspi_module_from_reg(char** sspi_module)
{
	HKEY hKey;
	DWORD dwType, dwSize;

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

	*sspi_module = (LPSTR)malloc(dwSize + sizeof(CHAR));
	if (!(*sspi_module))
	{
		RegCloseKey(hKey);
		return;
	}

	if (RegQueryValueExA(hKey, "SspiModule", NULL, &dwType, (BYTE*)(*sspi_module), &dwSize) !=
	    ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		free(*sspi_module);
		return;
	}

	RegCloseKey(hKey);
}

static SecurityFunctionTableA* auth_resolve_sspi_table(const rdpSettings* settings)
{
	char* sspi_module = NULL;

	WINPR_ASSERT(settings);

	if (settings->ServerMode)
		auth_get_sspi_module_from_reg(&sspi_module);

	if (sspi_module || settings->SspiModule)
	{
		INIT_SECURITY_INTERFACE_A InitSecurityInterface_ptr;
		const char* module_name = sspi_module ? sspi_module : settings->SspiModule;

		HMODULE hSSPI = LoadLibraryX(module_name);

		if (!hSSPI)
		{
			WLog_ERR(TAG, "Failed to load SSPI module: %s", module_name);
			return FALSE;
		}

		WLog_INFO(TAG, "Using SSPI Module: %s", module_name);

		InitSecurityInterface_ptr = GetProcAddress(hSSPI, "InitSecurityInterfaceA");

		free(sspi_module);
		return InitSecurityInterface_ptr();
	}

	return InitSecurityInterfaceExA(0);
}

static BOOL auth_setup_identity(rdpCredsspAuth* auth, SEC_WINNT_AUTH_IDENTITY_WINPR* identity)
{
	const rdpSettings* settings;
	SEC_WINPR_KERBEROS_SETTINGS* krb_settings;
	SEC_WINPR_NTLM_SETTINGS* ntlm_settings;
	freerdp_peer* peer;

	WINPR_ASSERT(auth);
	WINPR_ASSERT(auth->rdp_ctx);

	peer = auth->rdp_ctx->peer;
	settings = auth->rdp_ctx->settings;
	WINPR_ASSERT(settings);

	WINPR_ASSERT(identity);
	krb_settings = &identity->kerberosSettings;
	ntlm_settings = &identity->ntlmSettings;

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

	identity->identity.Flags |= SEC_WINNT_AUTH_IDENTITY_EXTENDED;

	return TRUE;
}

BOOL credssp_auth_set_spn(rdpCredsspAuth* auth, const char* service, const char* hostname)
{
	size_t length;

	if (!hostname)
		return FALSE;

	if (!service)
	{
		if (!(auth->spn = _strdup(hostname)))
			return FALSE;
		return TRUE;
	}

	length = strlen(service) + strlen(hostname) + 2;
	auth->spn = malloc(length);
	if (!auth->spn)
		return FALSE;

	sprintf_s(auth->spn, length, "%s/%s", service, hostname);

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
	INT32 v;
	const char* ptr;

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

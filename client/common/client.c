/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Client Common
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2025 Siemens
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

#include <winpr/cast.h>

#include <freerdp/config.h>

#include <string.h>
#include <errno.h>
#include <math.h>
#include <limits.h>
#include <float.h>

#include <freerdp/client.h>

#include <freerdp/freerdp.h>
#include <freerdp/addin.h>
#include <freerdp/assistance.h>
#include <freerdp/client/file.h>
#include <freerdp/utils/passphrase.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/client/channels.h>
#include <freerdp/utils/smartcardlogon.h>

#if defined(CHANNEL_AINPUT_CLIENT)
#include <freerdp/client/ainput.h>
#include <freerdp/channels/ainput.h>
#endif

#if defined(CHANNEL_VIDEO_CLIENT)
#include <freerdp/client/video.h>
#include <freerdp/channels/video.h>
#endif

#if defined(CHANNEL_RDPGFX_CLIENT)
#include <freerdp/client/rdpgfx.h>
#include <freerdp/channels/rdpgfx.h>
#include <freerdp/gdi/gfx.h>
#endif

#if defined(CHANNEL_GEOMETRY_CLIENT)
#include <freerdp/client/geometry.h>
#include <freerdp/channels/geometry.h>
#endif

#if defined(CHANNEL_GEOMETRY_CLIENT) || defined(CHANNEL_VIDEO_CLIENT)
#include <freerdp/gdi/video.h>
#endif

#ifdef WITH_AAD
#include <freerdp/utils/http.h>
#include <freerdp/utils/aad.h>
#endif

#ifdef WITH_SSO_MIB
#include "sso_mib_tokens.h"
#endif

#include <freerdp/log.h>
#define TAG CLIENT_TAG("common")

static void set_default_callbacks(freerdp* instance)
{
	WINPR_ASSERT(instance);
	instance->AuthenticateEx = client_cli_authenticate_ex;
	instance->ChooseSmartcard = client_cli_choose_smartcard;
	instance->VerifyCertificateEx = client_cli_verify_certificate_ex;
	instance->VerifyChangedCertificateEx = client_cli_verify_changed_certificate_ex;
	instance->PresentGatewayMessage = client_cli_present_gateway_message;
	instance->LogonErrorInfo = client_cli_logon_error_info;
	instance->GetAccessToken = client_cli_get_access_token;
	instance->RetryDialog = client_common_retry_dialog;
}

static BOOL freerdp_client_common_new(freerdp* instance, rdpContext* context)
{
	RDP_CLIENT_ENTRY_POINTS* pEntryPoints = NULL;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(context);

	instance->LoadChannels = freerdp_client_load_channels;
	set_default_callbacks(instance);

	pEntryPoints = instance->pClientEntryPoints;
	WINPR_ASSERT(pEntryPoints);
	return IFCALLRESULT(TRUE, pEntryPoints->ClientNew, instance, context);
}

static void freerdp_client_common_free(freerdp* instance, rdpContext* context)
{
	RDP_CLIENT_ENTRY_POINTS* pEntryPoints = NULL;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(context);

	pEntryPoints = instance->pClientEntryPoints;
	WINPR_ASSERT(pEntryPoints);
	IFCALL(pEntryPoints->ClientFree, instance, context);
}

/* Common API */

rdpContext* freerdp_client_context_new(const RDP_CLIENT_ENTRY_POINTS* pEntryPoints)
{
	freerdp* instance = NULL;
	rdpContext* context = NULL;

	if (!pEntryPoints)
		return NULL;

	IFCALL(pEntryPoints->GlobalInit);
	instance = freerdp_new();

	if (!instance)
		return NULL;

	instance->ContextSize = pEntryPoints->ContextSize;
	instance->ContextNew = freerdp_client_common_new;
	instance->ContextFree = freerdp_client_common_free;
	instance->pClientEntryPoints = (RDP_CLIENT_ENTRY_POINTS*)malloc(pEntryPoints->Size);

	if (!instance->pClientEntryPoints)
		goto out_fail;

	CopyMemory(instance->pClientEntryPoints, pEntryPoints, pEntryPoints->Size);

	if (!freerdp_context_new_ex(instance, pEntryPoints->settings))
		goto out_fail2;

	context = instance->context;
	context->instance = instance;

#if defined(WITH_CLIENT_CHANNELS)
	if (freerdp_register_addin_provider(freerdp_channels_load_static_addin_entry, 0) !=
	    CHANNEL_RC_OK)
		goto out_fail2;
#endif

	return context;
out_fail2:
	free(instance->pClientEntryPoints);
out_fail:
	freerdp_free(instance);
	return NULL;
}

void freerdp_client_context_free(rdpContext* context)
{
	freerdp* instance = NULL;

	if (!context)
		return;

	instance = context->instance;

	if (instance)
	{
		RDP_CLIENT_ENTRY_POINTS* pEntryPoints = instance->pClientEntryPoints;
		freerdp_context_free(instance);

		if (pEntryPoints)
			IFCALL(pEntryPoints->GlobalUninit);

		free(instance->pClientEntryPoints);
		freerdp_free(instance);
	}
}

int freerdp_client_start(rdpContext* context)
{
	RDP_CLIENT_ENTRY_POINTS* pEntryPoints = NULL;

	if (!context || !context->instance || !context->instance->pClientEntryPoints)
		return ERROR_BAD_ARGUMENTS;

	if (freerdp_settings_get_bool(context->settings, FreeRDP_UseCommonStdioCallbacks))
		set_default_callbacks(context->instance);

#ifdef WITH_SSO_MIB
	rdpClientContext* client_context = (rdpClientContext*)context;
	client_context->mibClientWrapper = sso_mib_new(context);
	if (!client_context->mibClientWrapper)
		return ERROR_INTERNAL_ERROR;
#endif

	pEntryPoints = context->instance->pClientEntryPoints;
	return IFCALLRESULT(CHANNEL_RC_OK, pEntryPoints->ClientStart, context);
}

int freerdp_client_stop(rdpContext* context)
{
	RDP_CLIENT_ENTRY_POINTS* pEntryPoints = NULL;

	if (!context || !context->instance || !context->instance->pClientEntryPoints)
		return ERROR_BAD_ARGUMENTS;

	pEntryPoints = context->instance->pClientEntryPoints;
	const int rc = IFCALLRESULT(CHANNEL_RC_OK, pEntryPoints->ClientStop, context);

#ifdef WITH_SSO_MIB
	rdpClientContext* client_context = (rdpClientContext*)context;
	sso_mib_free(client_context->mibClientWrapper);
	client_context->mibClientWrapper = NULL;
#endif // WITH_SSO_MIB
	return rc;
}

freerdp* freerdp_client_get_instance(rdpContext* context)
{
	if (!context || !context->instance)
		return NULL;

	return context->instance;
}

HANDLE freerdp_client_get_thread(rdpContext* context)
{
	if (!context)
		return NULL;

	return ((rdpClientContext*)context)->thread;
}

static BOOL freerdp_client_settings_post_process(rdpSettings* settings)
{
	/* Moved GatewayUseSameCredentials logic outside of cmdline.c, so
	 * that the rdp file also triggers this functionality */
	if (freerdp_settings_get_bool(settings, FreeRDP_GatewayEnabled))
	{
		if (freerdp_settings_get_bool(settings, FreeRDP_GatewayUseSameCredentials))
		{
			const char* Username = freerdp_settings_get_string(settings, FreeRDP_Username);
			const char* Domain = freerdp_settings_get_string(settings, FreeRDP_Domain);
			if (Username)
			{
				if (!freerdp_settings_set_string(settings, FreeRDP_GatewayUsername, Username))
					goto out_error;
			}

			if (Domain)
			{
				if (!freerdp_settings_set_string(settings, FreeRDP_GatewayDomain, Domain))
					goto out_error;
			}

			if (freerdp_settings_get_string(settings, FreeRDP_Password))
			{
				if (!freerdp_settings_set_string(
				        settings, FreeRDP_GatewayPassword,
				        freerdp_settings_get_string(settings, FreeRDP_Password)))
					goto out_error;
			}
		}
	}

	/* Moved logic for Multimon and Span monitors to force fullscreen, so
	 * that the rdp file also triggers this functionality */
	if (freerdp_settings_get_bool(settings, FreeRDP_SpanMonitors))
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_UseMultimon, TRUE))
			goto out_error;
		if (!freerdp_settings_set_bool(settings, FreeRDP_Fullscreen, TRUE))
			goto out_error;
	}
	else if (freerdp_settings_get_bool(settings, FreeRDP_UseMultimon))
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_Fullscreen, TRUE))
			goto out_error;
	}

	/* deal with the smartcard / smartcard logon stuff */
	if (freerdp_settings_get_bool(settings, FreeRDP_SmartcardLogon))
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, TRUE))
			goto out_error;
		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectSmartCards, TRUE))
			goto out_error;
		if (!freerdp_settings_set_bool(settings, FreeRDP_DeviceRedirection, TRUE))
			goto out_error;
		if (!freerdp_settings_set_bool(settings, FreeRDP_PasswordIsSmartcardPin, TRUE))
			goto out_error;
	}

	return TRUE;
out_error:
	return FALSE;
}

int freerdp_client_settings_parse_command_line(rdpSettings* settings, int argc, char** argv,
                                               BOOL allowUnknown)

{
	return freerdp_client_settings_parse_command_line_ex(settings, argc, argv, allowUnknown, NULL,
	                                                     0, NULL, NULL);
}

int freerdp_client_settings_parse_command_line_ex(
    rdpSettings* settings, int argc, char** argv, BOOL allowUnknown, COMMAND_LINE_ARGUMENT_A* args,
    size_t count, freerdp_command_line_handle_option_t handle_option, void* handle_userdata)
{
	int status = 0;

	if (argc < 1)
		return 0;

	if (!argv)
		return -1;

	status = freerdp_client_settings_parse_command_line_arguments_ex(
	    settings, argc, argv, allowUnknown, args, count, handle_option, handle_userdata);

	if (status < 0)
		return status;

	/* This function will call logic that is applicable to the settings
	 * from command line parsing AND the rdp file parsing */
	if (!freerdp_client_settings_post_process(settings))
		status = -1;

	const char* name = argv[0];
	WLog_DBG(TAG, "This is [%s] %s %s", name, freerdp_get_version_string(),
	         freerdp_get_build_config());
	return status;
}

int freerdp_client_settings_parse_connection_file(rdpSettings* settings, const char* filename)
{
	rdpFile* file = NULL;
	int ret = -1;
	file = freerdp_client_rdp_file_new();

	if (!file)
		return -1;

	if (!freerdp_client_parse_rdp_file(file, filename))
		goto out;

	if (!freerdp_client_populate_settings_from_rdp_file(file, settings))
		goto out;

	ret = 0;
out:
	freerdp_client_rdp_file_free(file);
	return ret;
}

int freerdp_client_settings_parse_connection_file_buffer(rdpSettings* settings, const BYTE* buffer,
                                                         size_t size)
{
	rdpFile* file = NULL;
	int status = -1;
	file = freerdp_client_rdp_file_new();

	if (!file)
		return -1;

	if (freerdp_client_parse_rdp_file_buffer(file, buffer, size) &&
	    freerdp_client_populate_settings_from_rdp_file(file, settings))
	{
		status = 0;
	}

	freerdp_client_rdp_file_free(file);
	return status;
}

int freerdp_client_settings_write_connection_file(const rdpSettings* settings, const char* filename,
                                                  BOOL unicode)
{
	rdpFile* file = NULL;
	int ret = -1;
	file = freerdp_client_rdp_file_new();

	if (!file)
		return -1;

	if (!freerdp_client_populate_rdp_file_from_settings(file, settings))
		goto out;

	if (!freerdp_client_write_rdp_file(file, filename, unicode))
		goto out;

	ret = 0;
out:
	freerdp_client_rdp_file_free(file);
	return ret;
}

int freerdp_client_settings_parse_assistance_file(rdpSettings* settings, int argc, char* argv[])
{
	int status = 0;
	int ret = -1;
	char* filename = NULL;
	char* password = NULL;
	rdpAssistanceFile* file = NULL;

	if (!settings || !argv || (argc < 2))
		return -1;

	filename = argv[1];

	for (int x = 2; x < argc; x++)
	{
		const char* key = strstr(argv[x], "assistance:");

		if (key)
			password = strchr(key, ':') + 1;
	}

	file = freerdp_assistance_file_new();

	if (!file)
		return -1;

	status = freerdp_assistance_parse_file(file, filename, password);

	if (status < 0)
		goto out;

	if (!freerdp_assistance_populate_settings_from_assistance_file(file, settings))
		goto out;

	ret = 0;
out:
	freerdp_assistance_file_free(file);
	return ret;
}

static int client_cli_read_string(freerdp* instance, const char* what, const char* suggestion,
                                  char** result)
{
	WINPR_ASSERT(instance);
	WINPR_ASSERT(what);
	WINPR_ASSERT(result);

	size_t size = 0;
	printf("%s", what);
	(void)fflush(stdout);

	char* line = NULL;
	if (suggestion && strlen(suggestion) > 0)
	{
		line = _strdup(suggestion);
		size = strlen(suggestion);
	}

	const SSIZE_T rc = freerdp_interruptible_get_line(instance->context, &line, &size, stdin);
	if (rc < 0)
	{
		char ebuffer[256] = { 0 };
		WLog_ERR(TAG, "freerdp_interruptible_get_line returned %s [%d]",
		         winpr_strerror(errno, ebuffer, sizeof(ebuffer)), errno);
		free(line);
		return -1;
	}

	free(*result);
	*result = NULL;

	if (line)
	{
		line = StrSep(&line, "\r");
		line = StrSep(&line, "\n");
		*result = line;
	}
	return 0;
}

/** @brief Callback set in the rdp_freerdp structure, and used to get the user's password,
 *  if required to establish the connection.
 *  This function is actually called in credssp_ntlmssp_client_init()
 *
 *  @see rdp_server_accept_nego() and rdp_check_fds()
 *  @param instance pointer to the rdp_freerdp structure that contains the connection settings
 *  @param username on input can contain a suggestion (must be allocated and is released by \b free
 * ). On output the allocated username entered by the user.
 *  @param password on input can contain a suggestion (must be allocated and is released by \b free
 * ). On output the allocated password entered by the user.
 *  @param domain on input can contain a suggestion (must be allocated and is released by \b free
 * ). On output the allocated domain entered by the user.
 *  @return TRUE if a password was successfully entered. See freerdp_passphrase_read() for more
 * details.
 */
static BOOL client_cli_authenticate_raw(freerdp* instance, rdp_auth_reason reason, char** username,
                                        char** password, char** domain)
{
	static const size_t password_size = 512;
	const char* userAuth = "Username:        ";
	const char* domainAuth = "Domain:          ";
	const char* pwdAuth = "Password:        ";
	BOOL pinOnly = FALSE;
	BOOL queryAll = FALSE;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);
	WINPR_ASSERT(instance->context->settings);

	switch (reason)
	{
		case AUTH_SMARTCARD_PIN:
			pwdAuth = "Smartcard-Pin:   ";
			pinOnly = TRUE;
			break;
		case AUTH_RDSTLS:
			queryAll = TRUE;
			break;
		case AUTH_TLS:
		case AUTH_RDP:
		case AUTH_NLA:
			break;
		case GW_AUTH_HTTP:
		case GW_AUTH_RDG:
		case GW_AUTH_RPC:
			userAuth = "GatewayUsername: ";
			domainAuth = "GatewayDomain:   ";
			pwdAuth = "GatewayPassword: ";
			break;
		default:
			return FALSE;
	}

	if (!username || !password || !domain)
		return FALSE;

	if (!pinOnly)
	{
		const char* suggest = *username;
		if (queryAll || !suggest)
		{
			const int rc = client_cli_read_string(instance, userAuth, suggest, username);
			if (rc < 0)
				goto fail;
		}
	}

	if (!pinOnly)
	{
		const char* suggest = *domain;
		if (queryAll || !suggest)
		{
			const int rc = client_cli_read_string(instance, domainAuth, suggest, domain);
			if (rc < 0)
				goto fail;
		}
	}

	{
		char* line = calloc(password_size, sizeof(char));

		if (!line)
			goto fail;

		const BOOL fromStdin =
		    freerdp_settings_get_bool(instance->context->settings, FreeRDP_CredentialsFromStdin);
		const char* rc =
		    freerdp_passphrase_read(instance->context, pwdAuth, line, password_size, fromStdin);
		if (rc == NULL)
			goto fail;

		if (password_size > 0)
		{
			free(*password);
			*password = line;
		}
	}

	return TRUE;
fail:
	free(*username);
	free(*domain);
	free(*password);
	*username = NULL;
	*domain = NULL;
	*password = NULL;
	return FALSE;
}

BOOL client_cli_authenticate_ex(freerdp* instance, char** username, char** password, char** domain,
                                rdp_auth_reason reason)
{
	WINPR_ASSERT(instance);
	WINPR_ASSERT(username);
	WINPR_ASSERT(password);
	WINPR_ASSERT(domain);

	switch (reason)
	{
		case AUTH_RDSTLS:
		case AUTH_NLA:
			break;

		case AUTH_TLS:
		case AUTH_RDP:
		case AUTH_SMARTCARD_PIN: /* in this case password is pin code */
			if ((*username) && (*password))
				return TRUE;
			break;
		case GW_AUTH_HTTP:
		case GW_AUTH_RDG:
		case GW_AUTH_RPC:
			break;
		default:
			return FALSE;
	}

	return client_cli_authenticate_raw(instance, reason, username, password, domain);
}

BOOL client_cli_choose_smartcard(WINPR_ATTR_UNUSED freerdp* instance, SmartcardCertInfo** cert_list,
                                 DWORD count, DWORD* choice, BOOL gateway)
{
	unsigned long answer = 0;
	char* p = NULL;

	printf("Multiple smartcards are available for use:\n");
	for (DWORD i = 0; i < count; i++)
	{
		const SmartcardCertInfo* cert = cert_list[i];
		char* reader = ConvertWCharToUtf8Alloc(cert->reader, NULL);
		char* container_name = ConvertWCharToUtf8Alloc(cert->containerName, NULL);

		printf("[%" PRIu32
		       "] %s\n\tReader: %s\n\tUser: %s@%s\n\tSubject: %s\n\tIssuer: %s\n\tUPN: %s\n",
		       i, container_name, reader, cert->userHint, cert->domainHint, cert->subject,
		       cert->issuer, cert->upn);

		free(reader);
		free(container_name);
	}

	while (1)
	{
		char input[10] = { 0 };

		printf("\nChoose a smartcard to use for %s (0 - %" PRIu32 "): ",
		       gateway ? "gateway authentication" : "logon", count - 1);
		(void)fflush(stdout);
		if (!fgets(input, 10, stdin))
		{
			WLog_ERR(TAG, "could not read from stdin");
			return FALSE;
		}

		answer = strtoul(input, &p, 10);
		if ((*p == '\n' && p != input) && answer < count)
		{
			*choice = (UINT32)answer;
			return TRUE;
		}
	}
}

#if defined(WITH_FREERDP_DEPRECATED)
BOOL client_cli_authenticate(freerdp* instance, char** username, char** password, char** domain)
{
	if (freerdp_settings_get_bool(instance->settings, FreeRDP_SmartcardLogon))
	{
		WLog_INFO(TAG, "Authentication via smartcard");
		return TRUE;
	}

	return client_cli_authenticate_raw(instance, FALSE, username, password, domain);
}

BOOL client_cli_gw_authenticate(freerdp* instance, char** username, char** password, char** domain)
{
	return client_cli_authenticate_raw(instance, TRUE, username, password, domain);
}
#endif

static DWORD client_cli_accept_certificate(freerdp* instance)
{
	int answer = 0;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

	const rdpSettings* settings = instance->context->settings;
	WINPR_ASSERT(settings);

	const BOOL fromStdin = freerdp_settings_get_bool(settings, FreeRDP_CredentialsFromStdin);
	if (fromStdin)
		return 0;

	while (1)
	{
		printf("Do you trust the above certificate? (Y/T/N) ");
		(void)fflush(stdout);
		answer = freerdp_interruptible_getc(instance->context, stdin);

		if ((answer == EOF) || feof(stdin))
		{
			printf("\nError: Could not read answer from stdin.\n");
			return 0;
		}

		switch (answer)
		{
			case 'y':
			case 'Y':
				answer = freerdp_interruptible_getc(instance->context, stdin);
				if (answer == EOF)
					return 0;
				return 1;

			case 't':
			case 'T':
				answer = freerdp_interruptible_getc(instance->context, stdin);
				if (answer == EOF)
					return 0;
				return 2;

			case 'n':
			case 'N':
				answer = freerdp_interruptible_getc(instance->context, stdin);
				if (answer == EOF)
					return 0;
				return 0;

			default:
				break;
		}

		printf("\n");
	}
}

/** Callback set in the rdp_freerdp structure, and used to make a certificate validation
 *  when the connection requires it.
 *  This function will actually be called by tls_verify_certificate().
 *  @see rdp_client_connect() and freerdp_tls_connect()
 *  @deprecated Use client_cli_verify_certificate_ex
 *  @param instance - pointer to the rdp_freerdp structure that contains the connection settings
 *  @param common_name
 *  @param subject
 *  @param issuer
 *  @param fingerprint
 *  @param host_mismatch Indicates the certificate host does not match.
 *  @return 1 if the certificate is trusted, 2 if temporary trusted, 0 otherwise.
 */
#if defined(WITH_FREERDP_DEPRECATED)
DWORD client_cli_verify_certificate(freerdp* instance, const char* common_name, const char* subject,
                                    const char* issuer, const char* fingerprint, BOOL host_mismatch)
{
	WINPR_UNUSED(common_name);
	WINPR_UNUSED(host_mismatch);

	printf("WARNING: This callback is deprecated, migrate to client_cli_verify_certificate_ex\n");
	printf("Certificate details:\n");
	printf("\tSubject: %s\n", subject);
	printf("\tIssuer: %s\n", issuer);
	printf("\tThumbprint: %s\n", fingerprint);
	printf("The above X.509 certificate could not be verified, possibly because you do not have\n"
	       "the CA certificate in your certificate store, or the certificate has expired.\n"
	       "Please look at the OpenSSL documentation on how to add a private CA to the store.\n");
	return client_cli_accept_certificate(instance);
}
#endif

static char* client_cli_pem_cert(const char* pem)
{
	rdpCertificate* cert = freerdp_certificate_new_from_pem(pem);
	if (!cert)
		return NULL;

	char* fp = freerdp_certificate_get_fingerprint(cert);
	char* start = freerdp_certificate_get_validity(cert, TRUE);
	char* end = freerdp_certificate_get_validity(cert, FALSE);
	freerdp_certificate_free(cert);

	char* str = NULL;
	size_t slen = 0;
	winpr_asprintf(&str, &slen,
	               "\tValid from:  %s\n"
	               "\tValid to:    %s\n"
	               "\tThumbprint:  %s\n",
	               start, end, fp);
	free(fp);
	free(start);
	free(end);
	return str;
}

/** Callback set in the rdp_freerdp structure, and used to make a certificate validation
 *  when the connection requires it.
 *  This function will actually be called by tls_verify_certificate().
 *  @see rdp_client_connect() and freerdp_tls_connect()
 *  @param instance     pointer to the rdp_freerdp structure that contains the connection settings
 *  @param host         The host currently connecting to
 *  @param port         The port currently connecting to
 *  @param common_name  The common name of the certificate, should match host or an alias of it
 *  @param subject      The subject of the certificate
 *  @param issuer       The certificate issuer name
 *  @param fingerprint  The fingerprint of the certificate
 *  @param flags        See VERIFY_CERT_FLAG_* for possible values.
 *
 *  @return 1 if the certificate is trusted, 2 if temporary trusted, 0 otherwise.
 */
DWORD client_cli_verify_certificate_ex(freerdp* instance, const char* host, UINT16 port,
                                       const char* common_name, const char* subject,
                                       const char* issuer, const char* fingerprint, DWORD flags)
{
	const char* type = "RDP-Server";

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);
	WINPR_ASSERT(instance->context->settings);

	if (flags & VERIFY_CERT_FLAG_GATEWAY)
		type = "RDP-Gateway";

	if (flags & VERIFY_CERT_FLAG_REDIRECT)
		type = "RDP-Redirect";

	printf("Certificate details for %s:%" PRIu16 " (%s):\n", host, port, type);
	printf("\tCommon Name: %s\n", common_name);
	printf("\tSubject:     %s\n", subject);
	printf("\tIssuer:      %s\n", issuer);
	/* Newer versions of FreeRDP allow exposing the whole PEM by setting
	 * FreeRDP_CertificateCallbackPreferPEM to TRUE
	 */
	if (flags & VERIFY_CERT_FLAG_FP_IS_PEM)
	{
		char* str = client_cli_pem_cert(fingerprint);
		printf("%s", str);
		free(str);
	}
	else
		printf("\tThumbprint:  %s\n", fingerprint);

	printf("The above X.509 certificate could not be verified, possibly because you do not have\n"
	       "the CA certificate in your certificate store, or the certificate has expired.\n"
	       "Please look at the OpenSSL documentation on how to add a private CA to the store.\n");
	return client_cli_accept_certificate(instance);
}

/** Callback set in the rdp_freerdp structure, and used to make a certificate validation
 *  when a stored certificate does not match the remote counterpart.
 *  This function will actually be called by tls_verify_certificate().
 *  @see rdp_client_connect() and freerdp_tls_connect()
 *  @deprecated Use client_cli_verify_changed_certificate_ex
 *  @param instance - pointer to the rdp_freerdp structure that contains the connection settings
 *  @param common_name
 *  @param subject
 *  @param issuer
 *  @param fingerprint
 *  @param old_subject
 *  @param old_issuer
 *  @param old_fingerprint
 *  @return 1 if the certificate is trusted, 2 if temporary trusted, 0 otherwise.
 */
#if defined(WITH_FREERDP_DEPRECATED)
DWORD client_cli_verify_changed_certificate(freerdp* instance, const char* common_name,
                                            const char* subject, const char* issuer,
                                            const char* fingerprint, const char* old_subject,
                                            const char* old_issuer, const char* old_fingerprint)
{
	WINPR_UNUSED(common_name);

	printf("WARNING: This callback is deprecated, migrate to "
	       "client_cli_verify_changed_certificate_ex\n");
	printf("!!! Certificate has changed !!!\n");
	printf("\n");
	printf("New Certificate details:\n");
	printf("\tSubject: %s\n", subject);
	printf("\tIssuer: %s\n", issuer);
	printf("\tThumbprint: %s\n", fingerprint);
	printf("\n");
	printf("Old Certificate details:\n");
	printf("\tSubject: %s\n", old_subject);
	printf("\tIssuer: %s\n", old_issuer);
	printf("\tThumbprint: %s\n", old_fingerprint);
	printf("\n");
	printf("The above X.509 certificate does not match the certificate used for previous "
	       "connections.\n"
	       "This may indicate that the certificate has been tampered with.\n"
	       "Please contact the administrator of the RDP server and clarify.\n");
	return client_cli_accept_certificate(instance);
}
#endif

/** Callback set in the rdp_freerdp structure, and used to make a certificate validation
 *  when a stored certificate does not match the remote counterpart.
 *  This function will actually be called by tls_verify_certificate().
 *  @see rdp_client_connect() and freerdp_tls_connect()
 *  @param instance        pointer to the rdp_freerdp structure that contains the connection
 * settings
 *  @param host            The host currently connecting to
 *  @param port            The port currently connecting to
 *  @param common_name     The common name of the certificate, should match host or an alias of it
 *  @param subject         The subject of the certificate
 *  @param issuer          The certificate issuer name
 *  @param fingerprint     The fingerprint of the certificate
 *  @param old_subject     The subject of the previous certificate
 *  @param old_issuer      The previous certificate issuer name
 *  @param old_fingerprint The fingerprint of the previous certificate
 *  @param flags           See VERIFY_CERT_FLAG_* for possible values.
 *
 *  @return 1 if the certificate is trusted, 2 if temporary trusted, 0 otherwise.
 */
DWORD client_cli_verify_changed_certificate_ex(freerdp* instance, const char* host, UINT16 port,
                                               const char* common_name, const char* subject,
                                               const char* issuer, const char* fingerprint,
                                               const char* old_subject, const char* old_issuer,
                                               const char* old_fingerprint, DWORD flags)
{
	const char* type = "RDP-Server";

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);
	WINPR_ASSERT(instance->context->settings);

	if (flags & VERIFY_CERT_FLAG_GATEWAY)
		type = "RDP-Gateway";

	if (flags & VERIFY_CERT_FLAG_REDIRECT)
		type = "RDP-Redirect";

	printf("!!!Certificate for %s:%" PRIu16 " (%s) has changed!!!\n", host, port, type);
	printf("\n");
	printf("New Certificate details:\n");
	printf("\tCommon Name: %s\n", common_name);
	printf("\tSubject:     %s\n", subject);
	printf("\tIssuer:      %s\n", issuer);
	/* Newer versions of FreeRDP allow exposing the whole PEM by setting
	 * FreeRDP_CertificateCallbackPreferPEM to TRUE
	 */
	if (flags & VERIFY_CERT_FLAG_FP_IS_PEM)
	{
		char* str = client_cli_pem_cert(fingerprint);
		printf("%s", str);
		free(str);
	}
	else
		printf("\tThumbprint:  %s\n", fingerprint);
	printf("\n");
	printf("Old Certificate details:\n");
	printf("\tSubject:     %s\n", old_subject);
	printf("\tIssuer:      %s\n", old_issuer);
	/* Newer versions of FreeRDP allow exposing the whole PEM by setting
	 * FreeRDP_CertificateCallbackPreferPEM to TRUE
	 */
	if (flags & VERIFY_CERT_FLAG_FP_IS_PEM)
	{
		char* str = client_cli_pem_cert(old_fingerprint);
		printf("%s", str);
		free(str);
	}
	else
		printf("\tThumbprint:  %s\n", old_fingerprint);
	printf("\n");
	if (flags & VERIFY_CERT_FLAG_MATCH_LEGACY_SHA1)
	{
		printf("\tA matching entry with legacy SHA1 was found in local known_hosts2 store.\n");
		printf("\tIf you just upgraded from a FreeRDP version before 2.0 this is expected.\n");
		printf("\tThe hashing algorithm has been upgraded from SHA1 to SHA256.\n");
		printf("\tAll manually accepted certificates must be reconfirmed!\n");
		printf("\n");
	}
	printf("The above X.509 certificate does not match the certificate used for previous "
	       "connections.\n"
	       "This may indicate that the certificate has been tampered with.\n"
	       "Please contact the administrator of the RDP server and clarify.\n");
	return client_cli_accept_certificate(instance);
}

BOOL client_cli_present_gateway_message(freerdp* instance, UINT32 type, BOOL isDisplayMandatory,
                                        BOOL isConsentMandatory, size_t length,
                                        const WCHAR* message)
{
	int answer = 0;
	const char* msgType = (type == GATEWAY_MESSAGE_CONSENT) ? "Consent message" : "Service message";

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);
	WINPR_ASSERT(instance->context->settings);

	if (!isDisplayMandatory && !isConsentMandatory)
		return TRUE;

	printf("%s:\n", msgType);
#if defined(WIN32)
	printf("%.*S\n", (int)length, message);
#else
	{
		LPSTR msg = ConvertWCharNToUtf8Alloc(message, length / sizeof(WCHAR), NULL);
		if (!msg)
		{
			printf("Failed to convert message!\n");
			return FALSE;
		}
		printf("%s\n", msg);
		free(msg);
	}
#endif

	while (isConsentMandatory)
	{
		printf("I understand and agree to the terms of this policy (Y/N) \n");
		(void)fflush(stdout);
		answer = freerdp_interruptible_getc(instance->context, stdin);

		if ((answer == EOF) || feof(stdin))
		{
			printf("\nError: Could not read answer from stdin.\n");
			return FALSE;
		}

		switch (answer)
		{
			case 'y':
			case 'Y':
				answer = freerdp_interruptible_getc(instance->context, stdin);
				if (answer == EOF)
					return FALSE;
				return TRUE;

			case 'n':
			case 'N':
				(void)freerdp_interruptible_getc(instance->context, stdin);
				return FALSE;

			default:
				break;
		}

		printf("\n");
	}

	return TRUE;
}

static const char* extract_authorization_code(char* url)
{
	WINPR_ASSERT(url);

	for (char* p = strchr(url, '?'); p++ != NULL; p = strchr(p, '&'))
	{
		if (strncmp(p, "code=", 5) != 0)
			continue;

		char* end = NULL;
		p += 5;

		end = strchr(p, '&');
		if (end)
			*end = '\0';

		return p;
	}

	return NULL;
}

#if defined(WITH_AAD)
static BOOL client_cli_get_rdsaad_access_token(freerdp* instance, const char* scope,
                                               const char* req_cnf, char** token)
{
	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

	size_t size = 0;
	char* url = NULL;
	char* token_request = NULL;

	WINPR_ASSERT(scope);
	WINPR_ASSERT(req_cnf);
	WINPR_ASSERT(token);

	BOOL rc = FALSE;
	*token = NULL;

	char* request = freerdp_client_get_aad_url((rdpClientContext*)instance->context,
	                                           FREERDP_CLIENT_AAD_AUTH_REQUEST, scope);

	printf("Browse to: %s\n", request);
	free(request);
	printf("Paste redirect URL here: \n");

	if (freerdp_interruptible_get_line(instance->context, &url, &size, stdin) < 0)
		goto cleanup;

	{
		const char* code = extract_authorization_code(url);
		if (!code)
			goto cleanup;
		token_request =
		    freerdp_client_get_aad_url((rdpClientContext*)instance->context,
		                               FREERDP_CLIENT_AAD_TOKEN_REQUEST, scope, code, req_cnf);
	}
	if (!token_request)
		goto cleanup;

	rc = client_common_get_access_token(instance, token_request, token);

cleanup:
	free(token_request);
	free(url);
	return rc && (*token != NULL);
}

static BOOL client_cli_get_avd_access_token(freerdp* instance, char** token)
{
	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

	size_t size = 0;
	char* url = NULL;
	char* token_request = NULL;

	WINPR_ASSERT(token);

	BOOL rc = FALSE;

	*token = NULL;

	char* request = freerdp_client_get_aad_url((rdpClientContext*)instance->context,
	                                           FREERDP_CLIENT_AAD_AVD_AUTH_REQUEST);
	if (!request)
		return FALSE;
	printf("Browse to: %s\n", request);
	free(request);
	printf("Paste redirect URL here: \n");

	if (freerdp_interruptible_get_line(instance->context, &url, &size, stdin) < 0)
		goto cleanup;

	{
		const char* code = extract_authorization_code(url);
		if (!code)
			goto cleanup;
		token_request = freerdp_client_get_aad_url((rdpClientContext*)instance->context,
		                                           FREERDP_CLIENT_AAD_AVD_TOKEN_REQUEST, code);
	}

	if (!token_request)
		goto cleanup;

	rc = client_common_get_access_token(instance, token_request, token);

cleanup:
	free(token_request);
	free(url);
	return rc && (*token != NULL);
}
#endif

BOOL client_cli_get_access_token(freerdp* instance, AccessTokenType tokenType, char** token,
                                 size_t count, ...)
{
	WINPR_ASSERT(instance);
	WINPR_ASSERT(token);

#if !defined(WITH_AAD)
	WLog_ERR(TAG, "Build does not support AAD authentication");
	return FALSE;
#else
	BOOL rc = FALSE;
	WINPR_ASSERT(instance->context);
	const BOOL saved =
	    freerdp_settings_get_bool(instance->context->settings, FreeRDP_UseCommonStdioCallbacks);
	if (!freerdp_settings_set_bool(instance->context->settings, FreeRDP_UseCommonStdioCallbacks,
	                               TRUE))
		return FALSE;

	switch (tokenType)
	{
		case ACCESS_TOKEN_TYPE_AAD:
		{
			if (count < 2)
			{
				WLog_ERR(TAG,
				         "ACCESS_TOKEN_TYPE_AAD expected 2 additional arguments, but got %" PRIuz
				         ", aborting",
				         count);
				return FALSE;
			}
			else if (count > 2)
				WLog_WARN(TAG,
				          "ACCESS_TOKEN_TYPE_AAD expected 2 additional arguments, but got %" PRIuz
				          ", ignoring",
				          count);
			va_list ap = { 0 };
			va_start(ap, count);
			const char* scope = va_arg(ap, const char*);
			const char* req_cnf = va_arg(ap, const char*);
			rc = client_cli_get_rdsaad_access_token(instance, scope, req_cnf, token);
			va_end(ap);
		}
		break;
		case ACCESS_TOKEN_TYPE_AVD:
			if (count != 0)
				WLog_WARN(TAG,
				          "ACCESS_TOKEN_TYPE_AVD expected 0 additional arguments, but got %" PRIuz
				          ", ignoring",
				          count);
			rc = client_cli_get_avd_access_token(instance, token);
			break;
		default:
			WLog_ERR(TAG, "Unexpected value for AccessTokenType [%u], aborting", tokenType);
			break;
	}

	if (!freerdp_settings_set_bool(instance->context->settings, FreeRDP_UseCommonStdioCallbacks,
	                               saved))
		return FALSE;
	return rc;
#endif
}

BOOL client_common_get_access_token(freerdp* instance, const char* request, char** token)
{
#ifdef WITH_AAD
	WINPR_ASSERT(request);
	WINPR_ASSERT(token);

	BOOL ret = FALSE;
	long resp_code = 0;
	BYTE* response = NULL;
	size_t response_length = 0;

	wLog* log = WLog_Get(TAG);

	const char* token_ep =
	    freerdp_utils_aad_get_wellknown_string(instance->context, AAD_WELLKNOWN_token_endpoint);
	if (!freerdp_http_request(token_ep, request, &resp_code, &response, &response_length))
	{
		WLog_ERR(TAG, "access token request failed");
		return FALSE;
	}

	if (resp_code != HTTP_STATUS_OK)
	{
		char buffer[64] = { 0 };

		WLog_Print(log, WLOG_ERROR,
		           "Server unwilling to provide access token; returned status code %s",
		           freerdp_http_status_string_format(resp_code, buffer, sizeof(buffer)));
		if (response_length > 0)
			WLog_Print(log, WLOG_ERROR, "[status message] %s", response);
		goto cleanup;
	}

	*token = freerdp_utils_aad_get_access_token(log, (const char*)response, response_length);
	if (*token)
		ret = TRUE;

cleanup:
	free(response);
	return ret;
#else
	return FALSE;
#endif
}

SSIZE_T client_common_retry_dialog(freerdp* instance, const char* what, size_t current,
                                   void* userarg)
{
	WINPR_UNUSED(instance);
	WINPR_ASSERT(instance->context);
	WINPR_UNUSED(userarg);
	WINPR_ASSERT(instance);
	WINPR_ASSERT(what);

	if ((strcmp(what, "arm-transport") != 0) && (strcmp(what, "connection") != 0))
	{
		WLog_ERR(TAG, "Unknown module %s, aborting", what);
		return -1;
	}

	if (current == 0)
	{
		if (strcmp(what, "arm-transport") == 0)
			WLog_INFO(TAG, "[%s] Starting your VM. It may take up to 5 minutes", what);
	}

	const rdpSettings* settings = instance->context->settings;
	const BOOL enabled = freerdp_settings_get_bool(settings, FreeRDP_AutoReconnectionEnabled);
	if (!enabled)
	{
		WLog_WARN(TAG, "Automatic reconnection disabled, terminating. Try to connect again later");
		return -1;
	}

	const size_t max = freerdp_settings_get_uint32(settings, FreeRDP_AutoReconnectMaxRetries);
	const size_t delay = freerdp_settings_get_uint32(settings, FreeRDP_TcpConnectTimeout);
	if (current >= max)
	{
		WLog_ERR(TAG,
		         "[%s] retries exceeded. Your VM failed to start. Try again later or contact your "
		         "tech support for help if this keeps happening.",
		         what);
		return -1;
	}

	WLog_INFO(TAG, "[%s] retry %" PRIuz "/%" PRIuz ", delaying %" PRIuz "ms before next attempt",
	          what, current + 1, max, delay);
	return WINPR_ASSERTING_INT_CAST(SSIZE_T, delay);
}

BOOL client_auto_reconnect(freerdp* instance)
{
	return client_auto_reconnect_ex(instance, NULL);
}

BOOL client_auto_reconnect_ex(freerdp* instance, BOOL (*window_events)(freerdp* instance))
{
	BOOL retry = TRUE;
	UINT32 error = 0;
	UINT32 numRetries = 0;
	rdpSettings* settings = NULL;

	if (!instance)
		return FALSE;

	WINPR_ASSERT(instance->context);

	settings = instance->context->settings;
	WINPR_ASSERT(settings);

	const UINT32 maxRetries =
	    freerdp_settings_get_uint32(settings, FreeRDP_AutoReconnectMaxRetries);

	/* Only auto reconnect on network disconnects. */
	error = freerdp_error_info(instance);
	switch (error)
	{
		case ERRINFO_GRAPHICS_SUBSYSTEM_FAILED:
			/* A network disconnect was detected */
			WLog_WARN(TAG, "Disconnected by server hitting a bug or resource limit [%s]",
			          freerdp_get_error_info_string(error));
			break;
		case ERRINFO_SUCCESS:
			/* A network disconnect was detected */
			WLog_INFO(TAG, "Network disconnect!");
			break;
		default:
			WLog_DBG(TAG, "Other error: %s", freerdp_get_error_info_string(error));
			return FALSE;
	}

	if (!freerdp_settings_get_bool(settings, FreeRDP_AutoReconnectionEnabled))
	{
		/* No auto-reconnect - just quit */
		WLog_DBG(TAG, "AutoReconnect not enabled, quitting.");
		return FALSE;
	}

	switch (freerdp_get_last_error(instance->context))
	{
		case FREERDP_ERROR_CONNECT_CANCELLED:
			WLog_WARN(TAG, "Connection aborted by user");
			return FALSE;
		default:
			break;
	}

	/* Perform an auto-reconnect. */
	while (retry)
	{
		/* Quit retrying if max retries has been exceeded */
		if ((maxRetries > 0) && (numRetries >= maxRetries))
		{
			WLog_DBG(TAG, "AutoReconnect retries exceeded.");
			return FALSE;
		}

		/* Attempt the next reconnect */
		WLog_INFO(TAG, "Attempting reconnect (%" PRIu32 " of %" PRIu32 ")", numRetries, maxRetries);

		const SSIZE_T delay =
		    IFCALLRESULT(5000, instance->RetryDialog, instance, "connection", numRetries, NULL);
		if (delay < 0)
			return FALSE;
		numRetries++;

		if (freerdp_reconnect(instance))
			return TRUE;

		switch (freerdp_get_last_error(instance->context))
		{
			case FREERDP_ERROR_CONNECT_CANCELLED:
				WLog_WARN(TAG, "Autoreconnect aborted by user");
				return FALSE;
			default:
				break;
		}
		for (SSIZE_T x = 0; x < delay / 10; x++)
		{
			if (!IFCALLRESULT(TRUE, window_events, instance))
			{
				WLog_ERR(TAG, "window_events failed!");
				return FALSE;
			}

			Sleep(10);
		}
	}

	WLog_ERR(TAG, "Maximum reconnect retries exceeded");
	return FALSE;
}

int freerdp_client_common_stop(rdpContext* context)
{
	rdpClientContext* cctx = (rdpClientContext*)context;
	WINPR_ASSERT(cctx);

	freerdp_abort_connect_context(&cctx->context);

	if (cctx->thread)
	{
		(void)WaitForSingleObject(cctx->thread, INFINITE);
		(void)CloseHandle(cctx->thread);
		cctx->thread = NULL;
	}

	return 0;
}

#if defined(CHANNEL_ENCOMSP_CLIENT)
BOOL freerdp_client_encomsp_toggle_control(EncomspClientContext* encomsp)
{
	rdpClientContext* cctx = NULL;
	BOOL state = 0;

	if (!encomsp)
		return FALSE;

	cctx = (rdpClientContext*)encomsp->custom;

	state = cctx->controlToggle;
	cctx->controlToggle = !cctx->controlToggle;
	return freerdp_client_encomsp_set_control(encomsp, state);
}

BOOL freerdp_client_encomsp_set_control(EncomspClientContext* encomsp, BOOL control)
{
	ENCOMSP_CHANGE_PARTICIPANT_CONTROL_LEVEL_PDU pdu = { 0 };

	if (!encomsp)
		return FALSE;

	pdu.ParticipantId = encomsp->participantId;
	pdu.Flags = ENCOMSP_REQUEST_VIEW;

	if (control)
		pdu.Flags |= ENCOMSP_REQUEST_INTERACT;

	encomsp->ChangeParticipantControlLevel(encomsp, &pdu);

	return TRUE;
}

static UINT
client_encomsp_participant_created(EncomspClientContext* context,
                                   const ENCOMSP_PARTICIPANT_CREATED_PDU* participantCreated)
{
	rdpClientContext* cctx = NULL;
	rdpSettings* settings = NULL;
	BOOL request = 0;

	if (!context || !context->custom || !participantCreated)
		return ERROR_INVALID_PARAMETER;

	cctx = (rdpClientContext*)context->custom;
	WINPR_ASSERT(cctx);

	settings = cctx->context.settings;
	WINPR_ASSERT(settings);

	if (participantCreated->Flags & ENCOMSP_IS_PARTICIPANT)
		context->participantId = participantCreated->ParticipantId;

	request = freerdp_settings_get_bool(settings, FreeRDP_RemoteAssistanceRequestControl);
	if (request && (participantCreated->Flags & ENCOMSP_MAY_VIEW) &&
	    !(participantCreated->Flags & ENCOMSP_MAY_INTERACT))
	{
		if (!freerdp_client_encomsp_set_control(context, TRUE))
			return ERROR_INTERNAL_ERROR;

		/* if auto-request-control setting is enabled then only request control once upon connect,
		 * otherwise it will auto request control again every time server turns off control which
		 * is a bit annoying */
		if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteAssistanceRequestControl, FALSE))
			return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

static void client_encomsp_init(rdpClientContext* cctx, EncomspClientContext* encomsp)
{
	cctx->encomsp = encomsp;
	encomsp->custom = (void*)cctx;
	encomsp->ParticipantCreated = client_encomsp_participant_created;
}

static void client_encomsp_uninit(rdpClientContext* cctx, EncomspClientContext* encomsp)
{
	if (encomsp)
	{
		encomsp->custom = NULL;
		encomsp->ParticipantCreated = NULL;
	}

	if (cctx)
		cctx->encomsp = NULL;
}
#endif

void freerdp_client_OnChannelConnectedEventHandler(void* context,
                                                   const ChannelConnectedEventArgs* e)
{
	rdpClientContext* cctx = (rdpClientContext*)context;

	WINPR_ASSERT(cctx);
	WINPR_ASSERT(e);

	if (0)
	{
	}
#if defined(CHANNEL_AINPUT_CLIENT)
	else if (strcmp(e->name, AINPUT_DVC_CHANNEL_NAME) == 0)
		cctx->ainput = (AInputClientContext*)e->pInterface;
#endif
#if defined(CHANNEL_RDPEI_CLIENT)
	else if (strcmp(e->name, RDPEI_DVC_CHANNEL_NAME) == 0)
	{
		cctx->rdpei = (RdpeiClientContext*)e->pInterface;
	}
#endif
#if defined(CHANNEL_RDPGFX_CLIENT)
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		gdi_graphics_pipeline_init(cctx->context.gdi, (RdpgfxClientContext*)e->pInterface);
	}
#endif
#if defined(CHANNEL_GEOMETRY_CLIENT)
	else if (strcmp(e->name, GEOMETRY_DVC_CHANNEL_NAME) == 0)
	{
		gdi_video_geometry_init(cctx->context.gdi, (GeometryClientContext*)e->pInterface);
	}
#endif
#if defined(CHANNEL_VIDEO_CLIENT)
	else if (strcmp(e->name, VIDEO_CONTROL_DVC_CHANNEL_NAME) == 0)
	{
		gdi_video_control_init(cctx->context.gdi, (VideoClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, VIDEO_DATA_DVC_CHANNEL_NAME) == 0)
	{
		gdi_video_data_init(cctx->context.gdi, (VideoClientContext*)e->pInterface);
	}
#endif
#if defined(CHANNEL_ENCOMSP_CLIENT)
	else if (strcmp(e->name, ENCOMSP_SVC_CHANNEL_NAME) == 0)
	{
		client_encomsp_init(cctx, (EncomspClientContext*)e->pInterface);
	}
#endif
}

void freerdp_client_OnChannelDisconnectedEventHandler(void* context,
                                                      const ChannelDisconnectedEventArgs* e)
{
	rdpClientContext* cctx = (rdpClientContext*)context;

	WINPR_ASSERT(cctx);
	WINPR_ASSERT(e);

	if (0)
	{
	}
#if defined(CHANNEL_AINPUT_CLIENT)
	else if (strcmp(e->name, AINPUT_DVC_CHANNEL_NAME) == 0)
		cctx->ainput = NULL;
#endif
#if defined(CHANNEL_RDPEI_CLIENT)
	else if (strcmp(e->name, RDPEI_DVC_CHANNEL_NAME) == 0)
	{
		cctx->rdpei = NULL;
	}
#endif
#if defined(CHANNEL_RDPGFX_CLIENT)
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		gdi_graphics_pipeline_uninit(cctx->context.gdi, (RdpgfxClientContext*)e->pInterface);
	}
#endif
#if defined(CHANNEL_GEOMETRY_CLIENT)
	else if (strcmp(e->name, GEOMETRY_DVC_CHANNEL_NAME) == 0)
	{
		gdi_video_geometry_uninit(cctx->context.gdi, (GeometryClientContext*)e->pInterface);
	}
#endif
#if defined(CHANNEL_VIDEO_CLIENT)
	else if (strcmp(e->name, VIDEO_CONTROL_DVC_CHANNEL_NAME) == 0)
	{
		gdi_video_control_uninit(cctx->context.gdi, (VideoClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, VIDEO_DATA_DVC_CHANNEL_NAME) == 0)
	{
		gdi_video_data_uninit(cctx->context.gdi, (VideoClientContext*)e->pInterface);
	}
#endif
#if defined(CHANNEL_ENCOMSP_CLIENT)
	else if (strcmp(e->name, ENCOMSP_SVC_CHANNEL_NAME) == 0)
	{
		client_encomsp_uninit(cctx, (EncomspClientContext*)e->pInterface);
	}
#endif
}

BOOL freerdp_client_send_wheel_event(rdpClientContext* cctx, UINT16 mflags)
{
	BOOL handled = FALSE;

	WINPR_ASSERT(cctx);

#if defined(CHANNEL_AINPUT_CLIENT)
	if (cctx->ainput)
	{
		UINT rc = 0;
		UINT64 flags = 0;
		INT32 x = 0;
		INT32 y = 0;
		INT32 value = mflags & 0xFF;

		if (mflags & PTR_FLAGS_WHEEL_NEGATIVE)
			value = -1 * (0x100 - value);

		/* We have discrete steps, scale this so we can also support high
		 * resolution wheels. */
		value *= 0x10000;

		if (mflags & PTR_FLAGS_WHEEL)
		{
			flags |= AINPUT_FLAGS_WHEEL;
			y = value;
		}

		if (mflags & PTR_FLAGS_HWHEEL)
		{
			flags |= AINPUT_FLAGS_WHEEL;
			x = value;
		}

		WINPR_ASSERT(cctx->ainput->AInputSendInputEvent);
		rc = cctx->ainput->AInputSendInputEvent(cctx->ainput, flags, x, y);
		if (rc == CHANNEL_RC_OK)
			handled = TRUE;
	}
#endif

	if (!handled)
		freerdp_input_send_mouse_event(cctx->context.input, mflags, 0, 0);

	return TRUE;
}

#if defined(CHANNEL_AINPUT_CLIENT)
static inline BOOL ainput_send_diff_event(rdpClientContext* cctx, UINT64 flags, INT32 x, INT32 y)
{
	UINT rc = 0;

	WINPR_ASSERT(cctx);
	WINPR_ASSERT(cctx->ainput);
	WINPR_ASSERT(cctx->ainput->AInputSendInputEvent);

	rc = cctx->ainput->AInputSendInputEvent(cctx->ainput, flags, x, y);

	return rc == CHANNEL_RC_OK;
}
#endif

static bool button_pressed(const rdpClientContext* cctx)
{
	WINPR_ASSERT(cctx);
	for (size_t x = 0; x < ARRAYSIZE(cctx->pressed_buttons); x++)
	{
		const BOOL cur = cctx->pressed_buttons[x];
		if (cur)
			return true;
	}
	return false;
}

BOOL freerdp_client_send_button_event(rdpClientContext* cctx, BOOL relative, UINT16 mflags, INT32 x,
                                      INT32 y)
{
	BOOL handled = FALSE;

	WINPR_ASSERT(cctx);

	if (mflags & PTR_FLAGS_BUTTON1)
		cctx->pressed_buttons[0] = mflags & PTR_FLAGS_DOWN;
	if (mflags & PTR_FLAGS_BUTTON2)
		cctx->pressed_buttons[1] = mflags & PTR_FLAGS_DOWN;
	if (mflags & PTR_FLAGS_BUTTON3)
		cctx->pressed_buttons[2] = mflags & PTR_FLAGS_DOWN;

	if (((mflags & PTR_FLAGS_MOVE) != 0) &&
	    !freerdp_settings_get_bool(cctx->context.settings, FreeRDP_MouseMotion))
	{
		if (!button_pressed(cctx))
			return TRUE;
	}

	const BOOL haveRelative =
	    freerdp_settings_get_bool(cctx->context.settings, FreeRDP_HasRelativeMouseEvent);
	if (relative && haveRelative)
	{
		return freerdp_input_send_rel_mouse_event(cctx->context.input, mflags,
		                                          WINPR_ASSERTING_INT_CAST(int16_t, x),
		                                          WINPR_ASSERTING_INT_CAST(int16_t, y));
	}

#if defined(CHANNEL_AINPUT_CLIENT)
	if (cctx->ainput)
	{
		UINT64 flags = 0;

		if (cctx->mouse_grabbed && freerdp_client_use_relative_mouse_events(cctx))
			flags |= AINPUT_FLAGS_HAVE_REL;

		if (relative)
			flags |= AINPUT_FLAGS_REL;

		if (mflags & PTR_FLAGS_DOWN)
			flags |= AINPUT_FLAGS_DOWN;
		if (mflags & PTR_FLAGS_BUTTON1)
			flags |= AINPUT_FLAGS_BUTTON1;
		if (mflags & PTR_FLAGS_BUTTON2)
			flags |= AINPUT_FLAGS_BUTTON2;
		if (mflags & PTR_FLAGS_BUTTON3)
			flags |= AINPUT_FLAGS_BUTTON3;
		if (mflags & PTR_FLAGS_MOVE)
			flags |= AINPUT_FLAGS_MOVE;
		handled = ainput_send_diff_event(cctx, flags, x, y);
	}
#endif

	if (!handled)
	{
		if (relative)
		{
			cctx->lastX += x;
			cctx->lastY += y;
			WLog_WARN(TAG, "Relative mouse input channel not available, sending absolute!");
		}
		else
		{
			cctx->lastX = x;
			cctx->lastY = y;
		}
		freerdp_input_send_mouse_event(cctx->context.input, mflags, (UINT16)cctx->lastX,
		                               (UINT16)cctx->lastY);
	}
	return TRUE;
}

BOOL freerdp_client_send_extended_button_event(rdpClientContext* cctx, BOOL relative, UINT16 mflags,
                                               INT32 x, INT32 y)
{
	BOOL handled = FALSE;
	WINPR_ASSERT(cctx);

	if (mflags & PTR_XFLAGS_BUTTON1)
		cctx->pressed_buttons[3] = mflags & PTR_XFLAGS_DOWN;
	if (mflags & PTR_XFLAGS_BUTTON2)
		cctx->pressed_buttons[4] = mflags & PTR_XFLAGS_DOWN;

	const BOOL haveRelative =
	    freerdp_settings_get_bool(cctx->context.settings, FreeRDP_HasRelativeMouseEvent);
	if (relative && haveRelative)
	{
		return freerdp_input_send_rel_mouse_event(cctx->context.input, mflags,
		                                          WINPR_ASSERTING_INT_CAST(int16_t, x),
		                                          WINPR_ASSERTING_INT_CAST(int16_t, y));
	}

#if defined(CHANNEL_AINPUT_CLIENT)
	if (cctx->ainput)
	{
		UINT64 flags = 0;

		if (relative)
			flags |= AINPUT_FLAGS_REL;
		if (mflags & PTR_XFLAGS_DOWN)
			flags |= AINPUT_FLAGS_DOWN;
		if (mflags & PTR_XFLAGS_BUTTON1)
			flags |= AINPUT_XFLAGS_BUTTON1;
		if (mflags & PTR_XFLAGS_BUTTON2)
			flags |= AINPUT_XFLAGS_BUTTON2;

		handled = ainput_send_diff_event(cctx, flags, x, y);
	}
#endif

	if (!handled)
	{
		if (relative)
		{
			cctx->lastX += x;
			cctx->lastY += y;
			WLog_WARN(TAG, "Relative mouse input channel not available, sending absolute!");
		}
		else
		{
			cctx->lastX = x;
			cctx->lastY = y;
		}
		freerdp_input_send_extended_mouse_event(cctx->context.input, mflags, (UINT16)cctx->lastX,
		                                        (UINT16)cctx->lastY);
	}

	return TRUE;
}

static BOOL freerdp_handle_touch_to_mouse(rdpClientContext* cctx, BOOL down,
                                          const FreeRDP_TouchContact* contact)
{
	const UINT16 flags = PTR_FLAGS_MOVE | (down ? PTR_FLAGS_DOWN : 0);
	const UINT16 xflags = down ? PTR_XFLAGS_DOWN : 0;
	WINPR_ASSERT(contact);
	WINPR_ASSERT(contact->x <= UINT16_MAX);
	WINPR_ASSERT(contact->y <= UINT16_MAX);

	switch (contact->count)
	{
		case 1:
			return freerdp_client_send_button_event(cctx, FALSE, flags | PTR_FLAGS_BUTTON1,
			                                        contact->x, contact->y);
		case 2:
			return freerdp_client_send_button_event(cctx, FALSE, flags | PTR_FLAGS_BUTTON2,
			                                        contact->x, contact->y);
		case 3:
			return freerdp_client_send_button_event(cctx, FALSE, flags | PTR_FLAGS_BUTTON3,
			                                        contact->x, contact->y);
		case 4:
			return freerdp_client_send_extended_button_event(
			    cctx, FALSE, xflags | PTR_XFLAGS_BUTTON1, contact->x, contact->y);
		case 5:
			return freerdp_client_send_extended_button_event(
			    cctx, FALSE, xflags | PTR_XFLAGS_BUTTON1, contact->x, contact->y);
		default:
			/* unmapped events, ignore */
			return TRUE;
	}
}

static BOOL freerdp_handle_touch_up(rdpClientContext* cctx, const FreeRDP_TouchContact* contact)
{
	WINPR_ASSERT(cctx);
	WINPR_ASSERT(contact);

#if defined(CHANNEL_RDPEI_CLIENT)
	RdpeiClientContext* rdpei = cctx->rdpei;

	if (!rdpei)
		return freerdp_handle_touch_to_mouse(cctx, FALSE, contact);

	int contactId = 0;

	if (rdpei->TouchRawEvent)
	{
		const UINT32 flags = RDPINPUT_CONTACT_FLAG_UP;
		const UINT32 contactFlags = ((contact->flags & FREERDP_TOUCH_HAS_PRESSURE) != 0)
		                                ? CONTACT_DATA_PRESSURE_PRESENT
		                                : 0;
		// Ensure contact position is unchanged from "engaged" to "out of range" state
		rdpei->TouchRawEvent(rdpei, contact->id, contact->x, contact->y, &contactId,
		                     RDPINPUT_CONTACT_FLAG_UPDATE | RDPINPUT_CONTACT_FLAG_INRANGE |
		                         RDPINPUT_CONTACT_FLAG_INCONTACT,
		                     contactFlags, contact->pressure);
		rdpei->TouchRawEvent(rdpei, contact->id, contact->x, contact->y, &contactId, flags,
		                     contactFlags, contact->pressure);
	}
	else
	{
		WINPR_ASSERT(rdpei->TouchEnd);
		rdpei->TouchEnd(rdpei, contact->id, contact->x, contact->y, &contactId);
	}
	return TRUE;
#else
	WLog_WARN(TAG, "Touch event detected but RDPEI support not compiled in. Recompile with "
	               "-DCHANNEL_RDPEI_CLIENT=ON");
	return freerdp_handle_touch_to_mouse(cctx, FALSE, contact);
#endif
}

static BOOL freerdp_handle_touch_down(rdpClientContext* cctx, const FreeRDP_TouchContact* contact)
{
	WINPR_ASSERT(cctx);
	WINPR_ASSERT(contact);

#if defined(CHANNEL_RDPEI_CLIENT)
	RdpeiClientContext* rdpei = cctx->rdpei;

	// Emulate mouse click if touch is not possible, like in login screen
	if (!rdpei)
		return freerdp_handle_touch_to_mouse(cctx, TRUE, contact);

	int contactId = 0;

	if (rdpei->TouchRawEvent)
	{
		const UINT32 flags = RDPINPUT_CONTACT_FLAG_DOWN | RDPINPUT_CONTACT_FLAG_INRANGE |
		                     RDPINPUT_CONTACT_FLAG_INCONTACT;
		const UINT32 contactFlags = ((contact->flags & FREERDP_TOUCH_HAS_PRESSURE) != 0)
		                                ? CONTACT_DATA_PRESSURE_PRESENT
		                                : 0;
		rdpei->TouchRawEvent(rdpei, contact->id, contact->x, contact->y, &contactId, flags,
		                     contactFlags, contact->pressure);
	}
	else
	{
		WINPR_ASSERT(rdpei->TouchBegin);
		rdpei->TouchBegin(rdpei, contact->id, contact->x, contact->y, &contactId);
	}

	return TRUE;
#else
	WLog_WARN(TAG, "Touch event detected but RDPEI support not compiled in. Recompile with "
	               "-DCHANNEL_RDPEI_CLIENT=ON");
	return freerdp_handle_touch_to_mouse(cctx, TRUE, contact);
#endif
}

static BOOL freerdp_handle_touch_motion_to_mouse(rdpClientContext* cctx,
                                                 const FreeRDP_TouchContact* contact)
{
	const UINT16 flags = PTR_FLAGS_MOVE;

	WINPR_ASSERT(contact);
	WINPR_ASSERT(contact->x <= UINT16_MAX);
	WINPR_ASSERT(contact->y <= UINT16_MAX);
	return freerdp_client_send_button_event(cctx, FALSE, flags, contact->x, contact->y);
}

static BOOL freerdp_handle_touch_motion(rdpClientContext* cctx, const FreeRDP_TouchContact* contact)
{
	WINPR_ASSERT(cctx);
	WINPR_ASSERT(contact);

#if defined(CHANNEL_RDPEI_CLIENT)
	RdpeiClientContext* rdpei = cctx->rdpei;

	if (!rdpei)
		return freerdp_handle_touch_motion_to_mouse(cctx, contact);

	int contactId = 0;

	if (rdpei->TouchRawEvent)
	{
		const UINT32 flags = RDPINPUT_CONTACT_FLAG_UPDATE | RDPINPUT_CONTACT_FLAG_INRANGE |
		                     RDPINPUT_CONTACT_FLAG_INCONTACT;
		const UINT32 contactFlags = ((contact->flags & FREERDP_TOUCH_HAS_PRESSURE) != 0)
		                                ? CONTACT_DATA_PRESSURE_PRESENT
		                                : 0;
		rdpei->TouchRawEvent(rdpei, contact->id, contact->x, contact->y, &contactId, flags,
		                     contactFlags, contact->pressure);
	}
	else
	{
		WINPR_ASSERT(rdpei->TouchUpdate);
		rdpei->TouchUpdate(rdpei, contact->id, contact->x, contact->y, &contactId);
	}

	return TRUE;
#else
	WLog_WARN(TAG, "Touch event detected but RDPEI support not compiled in. Recompile with "
	               "-DCHANNEL_RDPEI_CLIENT=ON");
	return freerdp_handle_touch_motion_to_mouse(cctx, contact);
#endif
}

static BOOL freerdp_client_touch_update(rdpClientContext* cctx, UINT32 flags, INT32 touchId,
                                        UINT32 pressure, INT32 x, INT32 y,
                                        FreeRDP_TouchContact* pcontact)
{
	WINPR_ASSERT(cctx);
	WINPR_ASSERT(pcontact);

	for (size_t i = 0; i < ARRAYSIZE(cctx->contacts); i++)
	{
		FreeRDP_TouchContact* contact = &cctx->contacts[i];

		const BOOL newcontact = ((contact->id == 0) && ((flags & FREERDP_TOUCH_DOWN) != 0));
		if (newcontact || (contact->id == touchId))
		{
			contact->id = touchId;
			contact->flags = flags;
			contact->pressure = pressure;
			contact->x = x;
			contact->y = y;

			*pcontact = *contact;

			const BOOL resetcontact = (flags & FREERDP_TOUCH_UP) != 0;
			if (resetcontact)
			{
				FreeRDP_TouchContact empty = { 0 };
				*contact = empty;
			}
			return TRUE;
		}
	}

	return FALSE;
}

BOOL freerdp_client_handle_touch(rdpClientContext* cctx, UINT32 flags, INT32 finger,
                                 UINT32 pressure, INT32 x, INT32 y)
{
	const UINT32 mask = FREERDP_TOUCH_DOWN | FREERDP_TOUCH_UP | FREERDP_TOUCH_MOTION;
	WINPR_ASSERT(cctx);

	FreeRDP_TouchContact contact = { 0 };

	if (!freerdp_client_touch_update(cctx, flags, finger, pressure, x, y, &contact))
		return FALSE;

	switch (flags & mask)
	{
		case FREERDP_TOUCH_DOWN:
			return freerdp_handle_touch_down(cctx, &contact);
		case FREERDP_TOUCH_UP:
			return freerdp_handle_touch_up(cctx, &contact);
		case FREERDP_TOUCH_MOTION:
			return freerdp_handle_touch_motion(cctx, &contact);
		default:
			WLog_WARN(TAG, "Unhandled FreeRDPTouchEventType %" PRIu32 ", ignoring", flags);
			return FALSE;
	}
}

BOOL freerdp_client_load_channels(freerdp* instance)
{
	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

	if (!freerdp_client_load_addins(instance->context->channels, instance->context->settings))
	{
		WLog_ERR(TAG, "Failed to load addins [%08" PRIx32 "]", GetLastError());
		return FALSE;
	}
	return TRUE;
}

int client_cli_logon_error_info(freerdp* instance, UINT32 data, UINT32 type)
{
	const char* str_data = freerdp_get_logon_error_info_data(data);
	const char* str_type = freerdp_get_logon_error_info_type(type);

	if (!instance || !instance->context)
		return -1;

	WLog_INFO(TAG, "Logon Error Info %s [%s]", str_data, str_type);
	return 1;
}

static FreeRDP_PenDevice* freerdp_client_get_pen(rdpClientContext* cctx, INT32 deviceid,
                                                 size_t* pos)
{
	WINPR_ASSERT(cctx);

	for (size_t i = 0; i < ARRAYSIZE(cctx->pens); i++)
	{
		FreeRDP_PenDevice* pen = &cctx->pens[i];
		if (deviceid == pen->deviceid)
		{
			if (pos)
				*pos = i;
			return pen;
		}
	}
	return NULL;
}

static BOOL freerdp_client_register_pen(rdpClientContext* cctx, UINT32 flags, INT32 deviceid,
                                        double pressure)
{
	static const INT32 null_deviceid = 0;

	WINPR_ASSERT(cctx);
	WINPR_ASSERT((flags & FREERDP_PEN_REGISTER) != 0);
	if (freerdp_client_is_pen(cctx, deviceid))
	{
		WLog_WARN(TAG, "trying to double register pen device %" PRId32, deviceid);
		return FALSE;
	}

	size_t pos = 0;
	FreeRDP_PenDevice* pen = freerdp_client_get_pen(cctx, null_deviceid, &pos);
	if (pen)
	{
		const FreeRDP_PenDevice empty = { 0 };
		*pen = empty;

		pen->deviceid = deviceid;
		pen->max_pressure = pressure;
		pen->flags = flags;

		WLog_DBG(TAG, "registered pen at index %" PRIuz, pos);
		return TRUE;
	}

	WLog_WARN(TAG, "No free slot for an additional pen device, skipping");
	return TRUE;
}

BOOL freerdp_client_handle_pen(rdpClientContext* cctx, UINT32 flags, INT32 deviceid, ...)
{
#if defined(CHANNEL_RDPEI_CLIENT)
	if ((flags & FREERDP_PEN_REGISTER) != 0)
	{
		va_list args;

		va_start(args, deviceid);
		double pressure = va_arg(args, double);
		va_end(args);
		return freerdp_client_register_pen(cctx, flags, deviceid, pressure);
	}
	size_t pos = 0;
	FreeRDP_PenDevice* pen = freerdp_client_get_pen(cctx, deviceid, &pos);
	if (!pen)
	{
		WLog_WARN(TAG, "unregistered pen device %" PRId32 " event 0x%08" PRIx32, deviceid, flags);
		return FALSE;
	}

	UINT32 fieldFlags = RDPINPUT_PEN_CONTACT_PENFLAGS_PRESENT;
	UINT32 penFlags =
	    ((pen->flags & FREERDP_PEN_IS_INVERTED) != 0) ? RDPINPUT_PEN_FLAG_INVERTED : 0;

	RdpeiClientContext* rdpei = cctx->rdpei;
	WINPR_ASSERT(rdpei);

	UINT32 normalizedpressure = 1024;
	INT32 x = 0;
	INT32 y = 0;
	UINT16 rotation = 0;
	INT16 tiltX = 0;
	INT16 tiltY = 0;
	va_list args;
	va_start(args, deviceid);

	x = va_arg(args, INT32);
	y = va_arg(args, INT32);
	if ((flags & FREERDP_PEN_HAS_PRESSURE) != 0)
	{
		const double pressure = va_arg(args, double);
		const double np = (pressure * 1024.0) / pen->max_pressure;
		normalizedpressure = (UINT32)lround(np);
		WLog_DBG(TAG, "pen pressure %lf -> %" PRIu32, pressure, normalizedpressure);
		fieldFlags |= RDPINPUT_PEN_CONTACT_PRESSURE_PRESENT;
	}
	if ((flags & FREERDP_PEN_HAS_ROTATION) != 0)
	{
		const unsigned arg = va_arg(args, unsigned);
		rotation = WINPR_ASSERTING_INT_CAST(UINT16, arg);
		fieldFlags |= RDPINPUT_PEN_CONTACT_ROTATION_PRESENT;
	}
	if ((flags & FREERDP_PEN_HAS_TILTX) != 0)
	{
		const int arg = va_arg(args, int);
		tiltX = WINPR_ASSERTING_INT_CAST(INT16, arg);
		fieldFlags |= RDPINPUT_PEN_CONTACT_TILTX_PRESENT;
	}
	if ((flags & FREERDP_PEN_HAS_TILTY) != 0)
	{
		const int arg = va_arg(args, int);
		tiltY = WINPR_ASSERTING_INT_CAST(INT16, arg);
		fieldFlags |= RDPINPUT_PEN_CONTACT_TILTY_PRESENT;
	}
	va_end(args);

	if ((flags & FREERDP_PEN_PRESS) != 0)
	{
		// Ensure that only one button is pressed
		if (pen->pressed)
			flags = FREERDP_PEN_MOTION |
			        (flags & (UINT32) ~(FREERDP_PEN_PRESS | FREERDP_PEN_BARREL_PRESSED));
		else if ((flags & FREERDP_PEN_BARREL_PRESSED) != 0)
			pen->flags |= FREERDP_PEN_BARREL_PRESSED;
	}
	else if ((flags & FREERDP_PEN_RELEASE) != 0)
	{
		if (!pen->pressed ||
		    ((flags & FREERDP_PEN_BARREL_PRESSED) ^ (pen->flags & FREERDP_PEN_BARREL_PRESSED)))
			flags = FREERDP_PEN_MOTION |
			        (flags & (UINT32) ~(FREERDP_PEN_RELEASE | FREERDP_PEN_BARREL_PRESSED));
		else
			pen->flags &= (UINT32)~FREERDP_PEN_BARREL_PRESSED;
	}

	flags |= pen->flags;
	if ((flags & FREERDP_PEN_ERASER_PRESSED) != 0)
		penFlags |= RDPINPUT_PEN_FLAG_ERASER_PRESSED;
	if ((flags & FREERDP_PEN_BARREL_PRESSED) != 0)
		penFlags |= RDPINPUT_PEN_FLAG_BARREL_PRESSED;

	pen->last_x = x;
	pen->last_y = y;
	if ((flags & FREERDP_PEN_PRESS) != 0)
	{
		WLog_DBG(TAG, "Pen press %" PRId32, deviceid);
		pen->hovering = FALSE;
		pen->pressed = TRUE;

		WINPR_ASSERT(rdpei->PenBegin);
		const UINT rc = rdpei->PenBegin(rdpei, deviceid, fieldFlags, x, y, penFlags,
		                                normalizedpressure, rotation, tiltX, tiltY);
		return rc == CHANNEL_RC_OK;
	}
	else if ((flags & FREERDP_PEN_MOTION) != 0)
	{
		UINT rc = ERROR_INTERNAL_ERROR;
		if (pen->pressed)
		{
			WLog_DBG(TAG, "Pen update %" PRId32, deviceid);

			// TODO: what if no rotation is supported but tilt is?
			WINPR_ASSERT(rdpei->PenUpdate);
			rc = rdpei->PenUpdate(rdpei, deviceid, fieldFlags, x, y, penFlags, normalizedpressure,
			                      rotation, tiltX, tiltY);
		}
		else if (pen->hovering)
		{
			WLog_DBG(TAG, "Pen hover update %" PRId32, deviceid);

			WINPR_ASSERT(rdpei->PenHoverUpdate);
			rc = rdpei->PenHoverUpdate(rdpei, deviceid, RDPINPUT_PEN_CONTACT_PENFLAGS_PRESENT, x, y,
			                           penFlags, normalizedpressure, rotation, tiltX, tiltY);
		}
		else
		{
			WLog_DBG(TAG, "Pen hover begin %" PRId32, deviceid);
			pen->hovering = TRUE;

			WINPR_ASSERT(rdpei->PenHoverBegin);
			rc = rdpei->PenHoverBegin(rdpei, deviceid, RDPINPUT_PEN_CONTACT_PENFLAGS_PRESENT, x, y,
			                          penFlags, normalizedpressure, rotation, tiltX, tiltY);
		}
		return rc == CHANNEL_RC_OK;
	}
	else if ((flags & FREERDP_PEN_RELEASE) != 0)
	{
		WLog_DBG(TAG, "Pen release %" PRId32, deviceid);
		pen->pressed = FALSE;
		pen->hovering = TRUE;

		WINPR_ASSERT(rdpei->PenUpdate);
		const UINT rc = rdpei->PenUpdate(rdpei, deviceid, fieldFlags, x, y, penFlags,
		                                 normalizedpressure, rotation, tiltX, tiltY);
		if (rc != CHANNEL_RC_OK)
			return FALSE;
		WINPR_ASSERT(rdpei->PenEnd);
		const UINT re = rdpei->PenEnd(rdpei, deviceid, RDPINPUT_PEN_CONTACT_PENFLAGS_PRESENT, x, y,
		                              penFlags, normalizedpressure, rotation, tiltX, tiltY);
		return re == CHANNEL_RC_OK;
	}

	WLog_WARN(TAG, "Invalid pen %" PRId32 " flags 0x%08" PRIx32, deviceid, flags);
#else
	WLog_WARN(TAG, "Pen event detected but RDPEI support not compiled in. Recompile with "
	               "-DCHANNEL_RDPEI_CLIENT=ON");
#endif

	return FALSE;
}

BOOL freerdp_client_pen_cancel_all(rdpClientContext* cctx)
{
	WINPR_ASSERT(cctx);

#if defined(CHANNEL_RDPEI_CLIENT)
	RdpeiClientContext* rdpei = cctx->rdpei;

	if (!rdpei)
		return FALSE;

	for (size_t i = 0; i < ARRAYSIZE(cctx->pens); i++)
	{
		FreeRDP_PenDevice* pen = &cctx->pens[i];
		if (pen->hovering)
		{
			WLog_DBG(TAG, "unhover pen %" PRId32, pen->deviceid);
			pen->hovering = FALSE;
			rdpei->PenHoverCancel(rdpei, pen->deviceid, 0, pen->last_x, pen->last_y);
		}
	}
	return TRUE;
#else
	WLog_WARN(TAG, "Pen event detected but RDPEI support not compiled in. Recompile with "
	               "-DCHANNEL_RDPEI_CLIENT=ON");
	return FALSE;
#endif
}

BOOL freerdp_client_is_pen(rdpClientContext* cctx, INT32 deviceid)
{
	WINPR_ASSERT(cctx);

	if (deviceid == 0)
		return FALSE;

	for (size_t x = 0; x < ARRAYSIZE(cctx->pens); x++)
	{
		const FreeRDP_PenDevice* pen = &cctx->pens[x];
		if (pen->deviceid == deviceid)
			return TRUE;
	}

	return FALSE;
}

BOOL freerdp_client_use_relative_mouse_events(rdpClientContext* ccontext)
{
	WINPR_ASSERT(ccontext);

	const rdpSettings* settings = ccontext->context.settings;
	const BOOL useRelative = freerdp_settings_get_bool(settings, FreeRDP_MouseUseRelativeMove);
	const BOOL haveRelative = freerdp_settings_get_bool(settings, FreeRDP_HasRelativeMouseEvent);
	BOOL ainput = FALSE;
#if defined(CHANNEL_AINPUT_CLIENT)
	ainput = ccontext->ainput != NULL;
#endif

	return useRelative && (haveRelative || ainput);
}

#if defined(WITH_AAD)
WINPR_ATTR_MALLOC(free, 1)
static char* get_redirect_uri(const rdpSettings* settings)
{
	char* redirect_uri = NULL;
	const bool cli = freerdp_settings_get_bool(settings, FreeRDP_UseCommonStdioCallbacks);
	if (cli)
	{
		const char* redirect_fmt =
		    freerdp_settings_get_string(settings, FreeRDP_GatewayAvdAccessAadFormat);
		const BOOL useTenant = freerdp_settings_get_bool(settings, FreeRDP_GatewayAvdUseTenantid);
		const char* tenantid = "common";
		if (useTenant)
			tenantid = freerdp_settings_get_string(settings, FreeRDP_GatewayAvdAadtenantid);

		if (tenantid && redirect_fmt)
		{
			const char* url =
			    freerdp_settings_get_string(settings, FreeRDP_GatewayAzureActiveDirectory);

			size_t redirect_len = 0;
			winpr_asprintf(&redirect_uri, &redirect_len, redirect_fmt, url, tenantid);
		}
	}
	else
	{
		const char* client_id = freerdp_settings_get_string(settings, FreeRDP_GatewayAvdClientID);
		const char* redirect_fmt =
		    freerdp_settings_get_string(settings, FreeRDP_GatewayAvdAccessTokenFormat);

		size_t redirect_len = 0;
		winpr_asprintf(&redirect_uri, &redirect_len, redirect_fmt, client_id);
	}
	return redirect_uri;
}

static char* avd_auth_request(rdpClientContext* cctx, WINPR_ATTR_UNUSED va_list ap)
{
	const rdpSettings* settings = cctx->context.settings;
	const char* client_id = freerdp_settings_get_string(settings, FreeRDP_GatewayAvdClientID);
	const char* ep = freerdp_utils_aad_get_wellknown_string(&cctx->context,
	                                                        AAD_WELLKNOWN_authorization_endpoint);
	const char* scope = freerdp_settings_get_string(settings, FreeRDP_GatewayAvdScope);

	if (!client_id || !ep || !scope)
		return NULL;

	char* redirect_uri = get_redirect_uri(settings);
	if (!redirect_uri)
		return NULL;

	char* url = NULL;
	size_t urllen = 0;
	winpr_asprintf(&url, &urllen, "%s?client_id=%s&response_type=code&scope=%s&redirect_uri=%s", ep,
	               client_id, scope, redirect_uri);
	free(redirect_uri);
	return url;
}

static char* avd_token_request(rdpClientContext* cctx, WINPR_ATTR_UNUSED va_list ap)
{
	const rdpSettings* settings = cctx->context.settings;
	const char* client_id = freerdp_settings_get_string(settings, FreeRDP_GatewayAvdClientID);
	const char* ep = freerdp_utils_aad_get_wellknown_string(&cctx->context,
	                                                        AAD_WELLKNOWN_authorization_endpoint);
	const char* scope = freerdp_settings_get_string(settings, FreeRDP_GatewayAvdScope);

	if (!client_id || !ep || !scope)
		return NULL;

	char* redirect_uri = get_redirect_uri(settings);
	if (!redirect_uri)
		return NULL;

	char* url = NULL;
	size_t urllen = 0;

	const char* code = va_arg(ap, const char*);
	winpr_asprintf(&url, &urllen,
	               "grant_type=authorization_code&code=%s&client_id=%s&scope=%s&redirect_uri=%s",
	               code, client_id, scope, redirect_uri);
	free(redirect_uri);
	return url;
}

static char* aad_auth_request(rdpClientContext* cctx, WINPR_ATTR_UNUSED va_list ap)
{
	const rdpSettings* settings = cctx->context.settings;
	char* url = NULL;
	size_t urllen = 0;
	char* redirect_uri = get_redirect_uri(settings);

	const char* client_id = freerdp_settings_get_string(settings, FreeRDP_GatewayAvdClientID);
	if (!client_id || !redirect_uri)
		goto cleanup;

	{
		const char* scope = va_arg(ap, const char*);
		if (!scope)
			goto cleanup;

		{
			const char* ep = freerdp_utils_aad_get_wellknown_string(
			    &cctx->context, AAD_WELLKNOWN_authorization_endpoint);
			winpr_asprintf(&url, &urllen,
			               "%s?client_id=%s&response_type=code&scope=%s&redirect_uri=%s", ep,
			               client_id, scope, redirect_uri);
		}
	}

cleanup:
	free(redirect_uri);
	return url;
}

static char* aad_token_request(rdpClientContext* cctx, WINPR_ATTR_UNUSED va_list ap)
{
	const rdpSettings* settings = cctx->context.settings;
	const char* client_id = freerdp_settings_get_string(settings, FreeRDP_GatewayAvdClientID);
	const char* ep = freerdp_utils_aad_get_wellknown_string(&cctx->context,
	                                                        AAD_WELLKNOWN_authorization_endpoint);
	const char* scope = va_arg(ap, const char*);
	const char* code = va_arg(ap, const char*);
	const char* req_cnf = va_arg(ap, const char*);

	if (!client_id || !ep || !scope || !code || !req_cnf)
		return NULL;

	char* redirect_uri = get_redirect_uri(settings);
	if (!redirect_uri)
		return NULL;

	char* url = NULL;
	size_t urllen = 0;

	winpr_asprintf(
	    &url, &urllen,
	    "grant_type=authorization_code&code=%s&client_id=%s&scope=%s&redirect_uri=%s&req_cnf=%s",
	    code, client_id, scope, redirect_uri, req_cnf);
	free(redirect_uri);
	return url;
}
#endif

char* freerdp_client_get_aad_url(rdpClientContext* cctx, freerdp_client_aad_type type, ...)
{
	WINPR_ASSERT(cctx);
	char* str = NULL;

	va_list ap;
	va_start(ap, type);
	switch (type)
	{
#if defined(WITH_AAD)
		case FREERDP_CLIENT_AAD_AUTH_REQUEST:
			str = aad_auth_request(cctx, ap);
			break;
		case FREERDP_CLIENT_AAD_TOKEN_REQUEST:
			str = aad_token_request(cctx, ap);
			break;
		case FREERDP_CLIENT_AAD_AVD_AUTH_REQUEST:
			str = avd_auth_request(cctx, ap);
			break;
		case FREERDP_CLIENT_AAD_AVD_TOKEN_REQUEST:
			str = avd_token_request(cctx, ap);
			break;
#endif
		default:
			break;
	}
	va_end(ap);
	return str;
}

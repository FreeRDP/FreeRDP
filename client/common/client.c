/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Client Common
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <string.h>
#include <errno.h>

#include <freerdp/client.h>

#include <freerdp/addin.h>
#include <freerdp/assistance.h>
#include <freerdp/client/file.h>
#include <freerdp/utils/passphrase.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/client/channels.h>

#include <freerdp/log.h>
#define TAG CLIENT_TAG("common")

static BOOL freerdp_client_common_new(freerdp* instance, rdpContext* context)
{
	RDP_CLIENT_ENTRY_POINTS* pEntryPoints = instance->pClientEntryPoints;
	return IFCALLRESULT(TRUE, pEntryPoints->ClientNew, instance, context);
}

static void freerdp_client_common_free(freerdp* instance, rdpContext* context)
{
	RDP_CLIENT_ENTRY_POINTS* pEntryPoints = instance->pClientEntryPoints;
	IFCALL(pEntryPoints->ClientFree, instance, context);
}

/* Common API */

rdpContext* freerdp_client_context_new(RDP_CLIENT_ENTRY_POINTS* pEntryPoints)
{
	freerdp* instance;
	rdpContext* context;

	if (!pEntryPoints)
		return NULL;

	IFCALL(pEntryPoints->GlobalInit);
	instance = freerdp_new();

	if (!instance)
		return NULL;

	instance->settings = pEntryPoints->settings;
	instance->ContextSize = pEntryPoints->ContextSize;
	instance->ContextNew = freerdp_client_common_new;
	instance->ContextFree = freerdp_client_common_free;
	instance->pClientEntryPoints = (RDP_CLIENT_ENTRY_POINTS*)malloc(pEntryPoints->Size);

	if (!instance->pClientEntryPoints)
		goto out_fail;

	CopyMemory(instance->pClientEntryPoints, pEntryPoints, pEntryPoints->Size);

	if (!freerdp_context_new(instance))
		goto out_fail2;

	context = instance->context;
	context->instance = instance;
	context->settings = instance->settings;

	if (freerdp_register_addin_provider(freerdp_channels_load_static_addin_entry, 0) !=
	    CHANNEL_RC_OK)
		goto out_fail2;

	return context;
out_fail2:
	free(instance->pClientEntryPoints);
out_fail:
	freerdp_free(instance);
	return NULL;
}

void freerdp_client_context_free(rdpContext* context)
{
	freerdp* instance;

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
	RDP_CLIENT_ENTRY_POINTS* pEntryPoints;

	if (!context || !context->instance || !context->instance->pClientEntryPoints)
		return ERROR_BAD_ARGUMENTS;

	pEntryPoints = context->instance->pClientEntryPoints;
	return IFCALLRESULT(CHANNEL_RC_OK, pEntryPoints->ClientStart, context);
}

int freerdp_client_stop(rdpContext* context)
{
	RDP_CLIENT_ENTRY_POINTS* pEntryPoints;

	if (!context || !context->instance || !context->instance->pClientEntryPoints)
		return ERROR_BAD_ARGUMENTS;

	pEntryPoints = context->instance->pClientEntryPoints;
	return IFCALLRESULT(CHANNEL_RC_OK, pEntryPoints->ClientStop, context);
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
	if (settings->GatewayEnabled)
	{
		if (settings->GatewayUseSameCredentials)
		{
			if (settings->Username)
			{
				free(settings->GatewayUsername);
				settings->GatewayUsername = _strdup(settings->Username);

				if (!settings->GatewayUsername)
					goto out_error;
			}

			if (settings->Domain)
			{
				free(settings->GatewayDomain);
				settings->GatewayDomain = _strdup(settings->Domain);

				if (!settings->GatewayDomain)
					goto out_error;
			}

			if (settings->Password)
			{
				free(settings->GatewayPassword);
				settings->GatewayPassword = _strdup(settings->Password);

				if (!settings->GatewayPassword)
					goto out_error;
			}
		}
	}

	/* Moved logic for Multimon and Span monitors to force fullscreen, so
	 * that the rdp file also triggers this functionality */
	if (settings->SpanMonitors)
	{
		settings->UseMultimon = TRUE;
		settings->Fullscreen = TRUE;
	}
	else if (settings->UseMultimon)
	{
		settings->Fullscreen = TRUE;
	}

	return TRUE;
out_error:
	free(settings->GatewayUsername);
	free(settings->GatewayDomain);
	free(settings->GatewayPassword);
	return FALSE;
}

int freerdp_client_settings_parse_command_line(rdpSettings* settings, int argc, char** argv,
                                               BOOL allowUnknown)
{
	int status;

	if (argc < 1)
		return 0;

	if (!argv)
		return -1;

	status =
	    freerdp_client_settings_parse_command_line_arguments(settings, argc, argv, allowUnknown);

	if (status < 0)
		return status;

	/* This function will call logic that is applicable to the settings
	 * from command line parsing AND the rdp file parsing */
	if (!freerdp_client_settings_post_process(settings))
		status = -1;

	WLog_DBG(TAG, "This is %s", freerdp_get_build_config());
	return status;
}

int freerdp_client_settings_parse_connection_file(rdpSettings* settings, const char* filename)
{
	rdpFile* file;
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
	rdpFile* file;
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
	rdpFile* file;
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
	int status, x;
	int ret = -1;
	char* filename;
	char* password = NULL;
	rdpAssistanceFile* file;

	if (!settings || !argv || (argc < 2))
		return -1;

	filename = argv[1];

	for (x = 2; x < argc; x++)
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

/** Callback set in the rdp_freerdp structure, and used to get the user's password,
 *  if required to establish the connection.
 *  This function is actually called in credssp_ntlmssp_client_init()
 *  @see rdp_server_accept_nego() and rdp_check_fds()
 *  @param instance - pointer to the rdp_freerdp structure that contains the connection settings
 *  @param username - unused
 *  @param password - on return: pointer to a character string that will be filled by the password
 * entered by the user. Note that this character string will be allocated inside the function, and
 * needs to be deallocated by the caller using free(), even in case this function fails.
 *  @param domain - unused
 *  @return TRUE if a password was successfully entered. See freerdp_passphrase_read() for more
 * details.
 */
static BOOL client_cli_authenticate_raw(freerdp* instance, BOOL gateway, char** username,
                                        char** password, char** domain)
{
	static const size_t password_size = 512;
	const char* auth[] = { "Username: ", "Domain:   ", "Password: " };
	const char* gw[] = { "GatewayUsername: ", "GatewayDomain:   ", "GatewayPassword: " };
	const char** prompt = (gateway) ? gw : auth;

	if (!username || !password || !domain)
		return FALSE;

	if (!*username)
	{
		size_t username_size = 0;
		printf("%s", prompt[0]);

		if (GetLine(username, &username_size, stdin) < 0)
		{
			WLog_ERR(TAG, "GetLine returned %s [%d]", strerror(errno), errno);
			goto fail;
		}

		if (*username)
		{
			*username = StrSep(username, "\r");
			*username = StrSep(username, "\n");
		}
	}

	if (!*domain)
	{
		size_t domain_size = 0;
		printf("%s", prompt[1]);

		if (GetLine(domain, &domain_size, stdin) < 0)
		{
			WLog_ERR(TAG, "GetLine returned %s [%d]", strerror(errno), errno);
			goto fail;
		}

		if (*domain)
		{
			*domain = StrSep(domain, "\r");
			*domain = StrSep(domain, "\n");
		}
	}

	if (!*password)
	{
		*password = calloc(password_size, sizeof(char));

		if (!*password)
			goto fail;

		if (freerdp_passphrase_read(prompt[2], *password, password_size,
		                            instance->settings->CredentialsFromStdin) == NULL)
			goto fail;
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

BOOL client_cli_authenticate(freerdp* instance, char** username, char** password, char** domain)
{
	if (instance->settings->SmartcardLogon)
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

static DWORD client_cli_accept_certificate(rdpSettings* settings)
{
	char answer;

	if (settings->CredentialsFromStdin)
		return 0;

	while (1)
	{
		printf("Do you trust the above certificate? (Y/T/N) ");
		fflush(stdout);
		answer = fgetc(stdin);

		if (feof(stdin))
		{
			printf("\nError: Could not read answer from stdin.");

			if (settings->CredentialsFromStdin)
				printf(" - Run without parameter \"--from-stdin\" to set trust.");

			printf("\n");
			return 0;
		}

		switch (answer)
		{
			case 'y':
			case 'Y':
				return 1;

			case 't':
			case 'T':
				return 2;

			case 'n':
			case 'N':
				return 0;

			default:
				break;
		}

		printf("\n");
	}

	return 0;
}

/** Callback set in the rdp_freerdp structure, and used to make a certificate validation
 *  when the connection requires it.
 *  This function will actually be called by tls_verify_certificate().
 *  @see rdp_client_connect() and tls_connect()
 *  @deprecated Use client_cli_verify_certificate_ex
 *  @param instance - pointer to the rdp_freerdp structure that contains the connection settings
 *  @param common_name
 *  @param subject
 *  @param issuer
 *  @param fingerprint
 *  @param host_mismatch Indicates the certificate host does not match.
 *  @return 1 if the certificate is trusted, 2 if temporary trusted, 0 otherwise.
 */
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
	return client_cli_accept_certificate(instance->settings);
}

/** Callback set in the rdp_freerdp structure, and used to make a certificate validation
 *  when the connection requires it.
 *  This function will actually be called by tls_verify_certificate().
 *  @see rdp_client_connect() and tls_connect()
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

	if (flags & VERIFY_CERT_FLAG_GATEWAY)
		type = "RDP-Gateway";

	if (flags & VERIFY_CERT_FLAG_REDIRECT)
		type = "RDP-Redirect";

	printf("Certificate details for %s:%" PRIu16 " (%s):\n", host, port, type);
	printf("\tCommon Name: %s\n", common_name);
	printf("\tSubject:     %s\n", subject);
	printf("\tIssuer:      %s\n", issuer);
	printf("\tThumbprint:  %s\n", fingerprint);
	printf("The above X.509 certificate could not be verified, possibly because you do not have\n"
	       "the CA certificate in your certificate store, or the certificate has expired.\n"
	       "Please look at the OpenSSL documentation on how to add a private CA to the store.\n");
	return client_cli_accept_certificate(instance->settings);
}

/** Callback set in the rdp_freerdp structure, and used to make a certificate validation
 *  when a stored certificate does not match the remote counterpart.
 *  This function will actually be called by tls_verify_certificate().
 *  @see rdp_client_connect() and tls_connect()
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
	return client_cli_accept_certificate(instance->settings);
}

/** Callback set in the rdp_freerdp structure, and used to make a certificate validation
 *  when a stored certificate does not match the remote counterpart.
 *  This function will actually be called by tls_verify_certificate().
 *  @see rdp_client_connect() and tls_connect()
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
	printf("\tThumbprint:  %s\n", fingerprint);
	printf("\n");
	printf("Old Certificate details:\n");
	printf("\tSubject:     %s\n", old_subject);
	printf("\tIssuer:      %s\n", old_issuer);
	printf("\tThumbprint:  %s\n", old_fingerprint);
	printf("\n");
	printf("The above X.509 certificate does not match the certificate used for previous "
	       "connections.\n"
	       "This may indicate that the certificate has been tampered with.\n"
	       "Please contact the administrator of the RDP server and clarify.\n");
	return client_cli_accept_certificate(instance->settings);
}

BOOL client_auto_reconnect(freerdp* instance)
{
	return client_auto_reconnect_ex(instance, NULL);
}

BOOL client_auto_reconnect_ex(freerdp* instance, BOOL (*window_events)(freerdp* instance))
{
	UINT32 maxRetries;
	UINT32 numRetries = 0;
	rdpSettings* settings;

	if (!instance || !instance->settings)
		return FALSE;

	settings = instance->settings;
	maxRetries = settings->AutoReconnectMaxRetries;

	/* Only auto reconnect on network disconnects. */
	if (freerdp_error_info(instance) != 0)
		return FALSE;

	/* A network disconnect was detected */
	WLog_INFO(TAG, "Network disconnect!");

	if (!settings->AutoReconnectionEnabled)
	{
		/* No auto-reconnect - just quit */
		return FALSE;
	}

	/* Perform an auto-reconnect. */
	while (TRUE)
	{
		UINT32 x;

		/* Quit retrying if max retries has been exceeded */
		if ((maxRetries > 0) && (numRetries++ >= maxRetries))
		{
			return FALSE;
		}

		/* Attempt the next reconnect */
		WLog_INFO(TAG, "Attempting reconnect (%" PRIu32 " of %" PRIu32 ")", numRetries, maxRetries);

		if (freerdp_reconnect(instance))
			return TRUE;

		for (x = 0; x < 50; x++)
		{
			if (!IFCALLRESULT(TRUE, window_events, instance))
				return FALSE;

			Sleep(100);
		}
	}

	WLog_ERR(TAG, "Maximum reconnect retries exceeded");
	return FALSE;
}

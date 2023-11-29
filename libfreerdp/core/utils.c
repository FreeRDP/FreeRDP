/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Terminal Server Gateway (utils)
 *
 * Copyright 2021 Armin Novak <armin.novak@thincast.com>
 * Copyright 2021 Thincast Technologies GmbH
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

#include <freerdp/config.h>

#include "settings.h"

#include <winpr/assert.h>

#include <freerdp/freerdp.h>

#include <freerdp/log.h>
#define TAG FREERDP_TAG("core.gateway.utils")

#include "utils.h"

#include "../core/rdp.h"

BOOL utils_str_copy(const char* value, char** dst)
{
	WINPR_ASSERT(dst);

	free(*dst);
	*dst = NULL;
	if (!value)
		return TRUE;

	(*dst) = _strdup(value);
	return (*dst) != NULL;
}

static BOOL utils_copy_smartcard_settings(const rdpSettings* settings, rdpSettings* origSettings)
{
	/* update original settings with provided smart card settings */
	origSettings->SmartcardLogon = settings->SmartcardLogon;
	origSettings->PasswordIsSmartcardPin = settings->PasswordIsSmartcardPin;
	if (!utils_str_copy(settings->ReaderName, &origSettings->ReaderName))
		return FALSE;
	if (!utils_str_copy(settings->CspName, &origSettings->CspName))
		return FALSE;
	if (!utils_str_copy(settings->ContainerName, &origSettings->ContainerName))
		return FALSE;

	return TRUE;
}

auth_status utils_authenticate_gateway(freerdp* instance, rdp_auth_reason reason)
{
	rdpSettings* settings;
	rdpSettings* origSettings;
	BOOL prompt = FALSE;
	BOOL proceed;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);
	WINPR_ASSERT(instance->context->settings);
	WINPR_ASSERT(instance->context->rdp);
	WINPR_ASSERT(instance->context->rdp->originalSettings);

	settings = instance->context->settings;
	origSettings = instance->context->rdp->originalSettings;

	if (freerdp_shall_disconnect_context(instance->context))
		return AUTH_FAILED;

	if (utils_str_is_empty(freerdp_settings_get_string(settings, FreeRDP_GatewayPassword)))
		prompt = TRUE;
	if (utils_str_is_empty(freerdp_settings_get_string(settings, FreeRDP_GatewayUsername)))
		prompt = TRUE;

	if (!prompt)
		return AUTH_SKIP;

	if (!instance->GatewayAuthenticate && !instance->AuthenticateEx)
		return AUTH_NO_CREDENTIALS;

	if (!instance->GatewayAuthenticate)
	{
		proceed =
		    instance->AuthenticateEx(instance, &settings->GatewayUsername,
		                             &settings->GatewayPassword, &settings->GatewayDomain, reason);
		if (!proceed)
			return AUTH_CANCELLED;
	}
	else
	{
		proceed =
		    instance->GatewayAuthenticate(instance, &settings->GatewayUsername,
		                                  &settings->GatewayPassword, &settings->GatewayDomain);
		if (!proceed)
			return AUTH_CANCELLED;
	}

	if (utils_str_is_empty(settings->GatewayUsername) ||
	    utils_str_is_empty(settings->GatewayPassword))
		return AUTH_NO_CREDENTIALS;

	if (!utils_sync_credentials(settings, FALSE))
		return AUTH_FAILED;

	/* update original settings with provided user credentials */
	if (!utils_str_copy(settings->GatewayUsername, &origSettings->GatewayUsername))
		return AUTH_FAILED;
	if (!utils_str_copy(settings->GatewayDomain, &origSettings->GatewayDomain))
		return AUTH_FAILED;
	if (!utils_str_copy(settings->GatewayPassword, &origSettings->GatewayPassword))
		return AUTH_FAILED;
	if (!utils_sync_credentials(origSettings, FALSE))
		return AUTH_FAILED;

	if (!utils_copy_smartcard_settings(settings, origSettings))
		return AUTH_FAILED;

	return AUTH_SUCCESS;
}

auth_status utils_authenticate(freerdp* instance, rdp_auth_reason reason, BOOL override)
{
	rdpSettings* settings;
	rdpSettings* origSettings;
	BOOL prompt = !override;
	BOOL proceed;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);
	WINPR_ASSERT(instance->context->settings);
	WINPR_ASSERT(instance->context->rdp);
	WINPR_ASSERT(instance->context->rdp->originalSettings);

	settings = instance->context->settings;
	origSettings = instance->context->rdp->originalSettings;

	if (freerdp_shall_disconnect_context(instance->context))
		return AUTH_FAILED;

	if (settings->ConnectChildSession)
		return AUTH_NO_CREDENTIALS;

	/* Ask for auth data if no or an empty username was specified or no password was given */
	if (utils_str_is_empty(freerdp_settings_get_string(settings, FreeRDP_Username)) ||
	    (settings->Password == NULL && settings->RedirectionPassword == NULL))
		prompt = TRUE;

	if (!prompt)
		return AUTH_SKIP;

	switch (reason)
	{
		case AUTH_RDP:
		case AUTH_TLS:
			if (settings->SmartcardLogon)
			{
				if (!utils_str_is_empty(settings->Password))
				{
					WLog_INFO(TAG, "Authentication via smartcard");
					return AUTH_SUCCESS;
				}
				reason = AUTH_SMARTCARD_PIN;
			}
			break;
		case AUTH_NLA:
			if (settings->SmartcardLogon)
				reason = AUTH_SMARTCARD_PIN;
			break;
		default:
			break;
	}

	/* If no callback is specified still continue connection */
	if (!instance->Authenticate && !instance->AuthenticateEx)
		return AUTH_NO_CREDENTIALS;

	if (!instance->Authenticate)
	{
		proceed = instance->AuthenticateEx(instance, &settings->Username, &settings->Password,
		                                   &settings->Domain, reason);
		if (!proceed)
			return AUTH_CANCELLED;
	}
	else
	{
		proceed = instance->Authenticate(instance, &settings->Username, &settings->Password,
		                                 &settings->Domain);
		if (!proceed)
			return AUTH_NO_CREDENTIALS;
	}

	if (utils_str_is_empty(settings->Username) || utils_str_is_empty(settings->Password))
		return AUTH_NO_CREDENTIALS;

	if (!utils_sync_credentials(settings, TRUE))
		return AUTH_FAILED;

	/* update original settings with provided user credentials */
	if (!utils_str_copy(settings->Username, &origSettings->Username))
		return AUTH_FAILED;
	if (!utils_str_copy(settings->Domain, &origSettings->Domain))
		return AUTH_FAILED;
	if (!utils_str_copy(settings->Password, &origSettings->Password))
		return AUTH_FAILED;
	if (!utils_sync_credentials(origSettings, TRUE))
		return AUTH_FAILED;

	if (!utils_copy_smartcard_settings(settings, origSettings))
		return AUTH_FAILED;

	return AUTH_SUCCESS;
}

BOOL utils_sync_credentials(rdpSettings* settings, BOOL toGateway)
{
	WINPR_ASSERT(settings);
	if (!settings->GatewayUseSameCredentials)
		return TRUE;

	if (toGateway)
	{
		if (!utils_str_copy(settings->Username, &settings->GatewayUsername))
			return FALSE;
		if (!utils_str_copy(settings->Domain, &settings->GatewayDomain))
			return FALSE;
		if (!utils_str_copy(settings->Password, &settings->GatewayPassword))
			return FALSE;
	}
	else
	{
		if (!utils_str_copy(settings->GatewayUsername, &settings->Username))
			return FALSE;
		if (!utils_str_copy(settings->GatewayDomain, &settings->Domain))
			return FALSE;
		if (!utils_str_copy(settings->GatewayPassword, &settings->Password))
			return FALSE;
	}
	return TRUE;
}

BOOL utils_str_is_empty(const char* str)
{
	if (!str)
		return TRUE;
	if (strlen(str) == 0)
		return TRUE;
	return FALSE;
}

BOOL utils_abort_connect(rdpRdp* rdp)
{
	if (!rdp)
		return FALSE;

	return SetEvent(rdp->abortEvent);
}

BOOL utils_reset_abort(rdpRdp* rdp)
{
	WINPR_ASSERT(rdp);

	return ResetEvent(rdp->abortEvent);
}

HANDLE utils_get_abort_event(rdpRdp* rdp)
{
	WINPR_ASSERT(rdp);
	return rdp->abortEvent;
}

BOOL utils_abort_event_is_set(rdpRdp* rdp)
{
	DWORD status;
	WINPR_ASSERT(rdp);
	status = WaitForSingleObject(rdp->abortEvent, 0);
	return status == WAIT_OBJECT_0;
}

const char* utils_is_vsock(const char* hostname)
{
	if (!hostname)
		return NULL;

	const char vsock[8] = "vsock://";
	if (strncmp(hostname, vsock, sizeof(vsock)) == 0)
		return &hostname[sizeof(vsock)];
	return NULL;
}

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

auth_status utils_authenticate_gateway(freerdp* instance, rdp_auth_reason reason)
{
	rdpSettings* settings;
	BOOL prompt = FALSE;
	BOOL proceed;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);
	WINPR_ASSERT(instance->context->settings);

	settings = instance->context->settings;

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

	if (instance->AuthenticateEx)
		proceed =
		    instance->AuthenticateEx(instance, &settings->GatewayUsername,
		                             &settings->GatewayPassword, &settings->GatewayDomain, reason);
	else
		proceed =
		    instance->GatewayAuthenticate(instance, &settings->GatewayUsername,
		                                  &settings->GatewayPassword, &settings->GatewayDomain);

	if (!proceed)
		return AUTH_NO_CREDENTIALS;

	if (!utils_sync_credentials(settings, FALSE))
		return AUTH_FAILED;
	return AUTH_SUCCESS;
}

auth_status utils_authenticate(freerdp* instance, rdp_auth_reason reason, BOOL override)
{
	rdpSettings* settings;
	BOOL prompt = !override;
	BOOL proceed;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);
	WINPR_ASSERT(instance->context->settings);

	settings = instance->context->settings;

	if (freerdp_shall_disconnect_context(instance->context))
		return AUTH_FAILED;

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

	if (instance->AuthenticateEx)
		proceed = instance->AuthenticateEx(instance, &settings->Username, &settings->Password,
		                                   &settings->Domain, reason);
	else
		proceed = instance->Authenticate(instance, &settings->Username, &settings->Password,
		                                 &settings->Domain);

	if (!proceed)
		return AUTH_NO_CREDENTIALS;

	if (!utils_sync_credentials(settings, TRUE))
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

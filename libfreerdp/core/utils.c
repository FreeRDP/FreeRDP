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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/assert.h>

#include <freerdp/freerdp.h>

#include <freerdp/log.h>
#define TAG FREERDP_TAG("core.gateway.utils")

#include "utils.h"

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
	WINPR_ASSERT(instance->settings);

	settings = instance->settings;

	if (freerdp_shall_disconnect(instance))
		return AUTH_FAILED;

	if (!settings->GatewayPassword || !settings->GatewayUsername ||
	    !strlen(settings->GatewayPassword) || !strlen(settings->GatewayUsername))
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
	WINPR_ASSERT(instance->settings);

	settings = instance->settings;

	if (freerdp_shall_disconnect(instance))
		return AUTH_FAILED;

	/* Ask for auth data if no or an empty username was specified or no password was given */
	if (utils_str_is_empty(settings->Username) ||
	    (settings->Password == NULL && settings->RedirectionPassword == NULL))
		prompt = TRUE;

	if (!prompt)
		return AUTH_SKIP;

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

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
#include <winpr/string.h>

#include <freerdp/config.h>

#include <string.h>
#include <ctype.h>
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
#include <freerdp/event.h>
#include <freerdp/utils/smartcardlogon.h>
#include <freerdp/session.h>

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

#include <freerdp/channels/rdpewa.h>

#ifdef WITH_AAD
#include <winpr/custom-crypto.h>

#include <freerdp/crypto/crypto.h>
#include <freerdp/utils/http.h>
#include <freerdp/utils/aad.h>
#endif

#ifdef WITH_SSO_MIB
#include "sso_mib_tokens.h"
#endif

#include <freerdp/log.h>
#define TAG CLIENT_TAG("common")

/** @brief State of one interactive OAuth transaction
 *
 *  Created by an authorization request built with \ref freerdp_client_get_aad_url and kept in
 *  \b rdpClientContext::aad_oauth until it is replaced by the next authorization request or
 *  released by \ref freerdp_client_aad_reset. Only one transaction exists per client context
 *  and access to it is not synchronized.
 */
typedef struct client_aad_oauth
{
	char* state;        /**< the 'state' value sent with the authorization request */
	char* verifier;     /**< the PKCE code verifier sent with the token request */
	char* redirect_uri; /**< percent decoded URI the authorization response must arrive at */
	BOOL consumed;      /**< whether the authorization response was already seen */
} client_aad_oauth;

/** @brief Release a string that held credential material
 *
 *  @param str The string to scrub and release, may be \b nullptr
 */
static void client_free_secret(char* str)
{
	if (str)
		SecureZeroMemory(str, strlen(str));
	free(str);
}

static void client_aad_oauth_free(client_aad_oauth* oauth)
{
	if (!oauth)
		return;

	free(oauth->state);
	client_free_secret(oauth->verifier);
	free(oauth->redirect_uri);
	free(oauth);
}

void freerdp_client_aad_reset(rdpClientContext* cctx)
{
	if (!cctx)
		return;

	client_aad_oauth_free(cctx->aad_oauth);
	cctx->aad_oauth = nullptr;
}

/** @brief The parts of a URI the OAuth callback check compares
 *
 *  \b scheme, \b host and \b path are percent decoded, \b query points into the URI that was
 *  parsed and keeps its escapes.
 */
typedef struct
{
	char* scheme;
	char* host;
	char* path;
	const char* query;
	UINT32 port;
	BOOL userinfo;
	BOOL fragment;
} client_uri;

/** @brief Percent decode a URI component the callback check compares
 *
 *  \b winpr_str_url_decode leaves an escape that is not \c '%' HEXDIG HEXDIG in place and
 *  decodes \c %00 to an embedded NUL, so a component could carry more than the C string it
 *  decodes to shows: every following comparison stops at the NUL and a different host or path
 *  would pass as the expected one. Accept only well formed escapes, reject \c %00 and verify
 *  that the result is as long as the escapes consumed say it must be.
 *
 *  @param str The component to decode
 *  @param len The number of octets of \b str that belong to the component
 *  @return The decoded component, to be released with \b free, or \b nullptr if \b str is not
 *          a valid percent encoding of a string without NUL
 */
WINPR_ATTR_MALLOC(free, 1)
static char* client_uri_decode(const char* str, size_t len)
{
	size_t declen = 0;

	for (size_t x = 0; x < len; x++, declen++)
	{
		if (str[x] != '%')
			continue;

		if (x + 2 >= len)
			return nullptr; /* an incomplete escape */
		if (!isxdigit((unsigned char)str[x + 1]) || !isxdigit((unsigned char)str[x + 2]))
			return nullptr;
		if ((str[x + 1] == '0') && (str[x + 2] == '0'))
			return nullptr; /* a NUL would truncate every comparison */
		x += 2;
	}

	char* decoded = winpr_str_url_decode(str, len);
	if (!decoded)
		return nullptr;

	/* Whatever the decoder did, the result has to be the string the escapes describe. */
	if (strlen(decoded) != declen)
	{
		free(decoded);
		return nullptr;
	}

	return decoded;
}

static void client_uri_free(client_uri* uri)
{
	free(uri->scheme);
	free(uri->host);
	free(uri->path);
	const client_uri empty = WINPR_C_ARRAY_INIT;
	*uri = empty;
}

/** The port a scheme uses when the authority does not name one. */
static UINT32 client_uri_default_port(const char* scheme)
{
	if (_stricmp(scheme, "https") == 0)
		return 443;
	if (_stricmp(scheme, "http") == 0)
		return 80;
	return 0;
}

/** @brief Split a URI into the parts the callback check compares
 *
 *  Only absolute \c scheme://authority[/path][?query][#fragment] URIs are accepted, which is
 *  what an OAuth redirect URI is. Nothing here is logged: the URI can hold an authorization
 *  code.
 *
 *  @param str The URI to split
 *  @param uri Receives the parts, to be released with \b client_uri_free
 *  @return \b TRUE if \b str is such a URI
 */
static BOOL client_uri_parse(const char* str, client_uri* uri)
{
	const client_uri empty = WINPR_C_ARRAY_INIT;
	*uri = empty;

	const char* sep = strstr(str, "://");
	if (!sep || (sep == str))
		return FALSE;

	/* scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ) */
	if (!isalpha((unsigned char)str[0]))
		return FALSE;
	for (const char* pos = str; pos < sep; pos++)
	{
		const char cur = *pos;
		if (!isalnum((unsigned char)cur) && (cur != '+') && (cur != '-') && (cur != '.'))
			return FALSE;
	}

	uri->scheme = strndup(str, WINPR_ASSERTING_INT_CAST(size_t, sep - str));
	if (!uri->scheme)
		goto fail;

	const char* authority = sep + 3;
	const char* rest = authority + strcspn(authority, "/?#");

	/* An authority may be preceded by userinfo, which a redirect URI never uses. */
	for (const char* pos = authority; pos < rest; pos++)
	{
		if (*pos != '@')
			continue;
		uri->userinfo = TRUE;
		authority = pos + 1;
	}

	const char* hostend = authority;
	if (*authority == '[') /* IP-literal */
	{
		while ((hostend < rest) && (*hostend != ']'))
			hostend++;
		if (hostend == rest)
			goto fail;
		hostend++;
	}
	else
	{
		while ((hostend < rest) && (*hostend != ':'))
			hostend++;
	}

	if (hostend == authority)
		goto fail;

	uri->host = client_uri_decode(authority, WINPR_ASSERTING_INT_CAST(size_t, hostend - authority));
	if (!uri->host)
		goto fail;

	if (hostend < rest)
	{
		if (*hostend != ':')
			goto fail;

		uri->port = 0;
		for (const char* pos = hostend + 1; pos < rest; pos++)
		{
			if (!isdigit((unsigned char)*pos) || (uri->port > UINT16_MAX))
				goto fail;
			uri->port = uri->port * 10 + (UINT32)(*pos - '0');
		}
		if (uri->port > UINT16_MAX)
			goto fail;
		if (hostend + 1 == rest) /* an empty port means the default port */
			uri->port = client_uri_default_port(uri->scheme);
	}
	else
		uri->port = client_uri_default_port(uri->scheme);

	const char* pathend = rest + strcspn(rest, "?#");
	uri->path = client_uri_decode(rest, WINPR_ASSERTING_INT_CAST(size_t, pathend - rest));
	if (!uri->path)
		goto fail;

	if (*pathend == '?')
	{
		uri->query = pathend + 1;
		uri->fragment = strchr(uri->query, '#') != nullptr;
	}
	else if (*pathend == '#')
		uri->fragment = TRUE;

	return TRUE;

fail:
	client_uri_free(uri);
	return FALSE;
}

/** @brief Whether two URIs address the same resource
 *
 *  The query is deliberately not compared: the authorization response adds parameters to the
 *  redirect URI the request was made with.
 */
static BOOL client_uri_same_destination(const client_uri* lhs, const client_uri* rhs)
{
	/* An empty path and "/" address the same resource. */
	const char* lpath = (lhs->path[0] == '\0') ? "/" : lhs->path;
	const char* rpath = (rhs->path[0] == '\0') ? "/" : rhs->path;

	return (_stricmp(lhs->scheme, rhs->scheme) == 0) && (_stricmp(lhs->host, rhs->host) == 0) &&
	       (lhs->port == rhs->port) && (strcmp(lpath, rpath) == 0);
}

/** @brief Result of looking a parameter up in a URI query */
typedef enum
{
	CLIENT_QUERY_MISSING,
	CLIENT_QUERY_FOUND,
	CLIENT_QUERY_DUPLICATE
} client_query_result;

/** @brief Look one parameter up in a percent encoded URI query
 *
 *  A parameter given without a value counts as an occurrence, so a query can not smuggle a
 *  second \c code or \c state past the duplicate check by leaving the value out.
 *
 *  @param query The query to search, may be \b nullptr
 *  @param name The name of the parameter
 *  @param value Receives the percent decoded value, may be \b nullptr
 *
 *  @return whether \b name occurs exactly once in \b query with a value that decodes
 */
static client_query_result client_uri_query_value(const char* query, const char* name, char** value)
{
	client_query_result rc = CLIENT_QUERY_MISSING;

	if (value)
		*value = nullptr;

	if (!query)
		return CLIENT_QUERY_MISSING;

	for (const char* pos = query; pos; pos = strchr(pos, '&'))
	{
		if (*pos == '&')
			pos++;

		const size_t pairlen = strcspn(pos, "&#");
		const size_t keylen = strcspn(pos, "=&#");

		char* key = client_uri_decode(pos, keylen);
		if (!key)
			return CLIENT_QUERY_DUPLICATE; /* a key that does not decode makes the query unusable */

		const BOOL match = strcmp(key, name) == 0;
		free(key);

		if (!match)
			continue;

		if (rc != CLIENT_QUERY_MISSING)
		{
			if (value)
			{
				free(*value);
				*value = nullptr;
			}
			return CLIENT_QUERY_DUPLICATE;
		}

		rc = CLIENT_QUERY_FOUND;
		if (keylen == pairlen) /* the parameter was given without a value */
			continue;

		if (value)
		{
			*value = client_uri_decode(&pos[keylen + 1], pairlen - keylen - 1);
			if (!*value)
				return CLIENT_QUERY_DUPLICATE; /* a value that does not decode is unusable */
		}
	}

	return rc;
}

/** @brief Compare two strings in a time that does not depend on their contents
 *
 *  The 'state' value is the secret an authorization response has to know. A response that does
 *  not carry it neither ends the transaction nor closes the web view, so a page that can drive
 *  top level navigations may try again as often as it likes.
 *
 *  @param a The first string
 *  @param b The second string
 *  @return \b TRUE if both strings are equal
 */
static BOOL client_const_time_equal(const char* a, const char* b)
{
	const size_t alen = strlen(a);
	const size_t blen = strlen(b);

	if (alen != blen)
		return FALSE;

	BYTE acc = 0;
	for (size_t x = 0; x < alen; x++)
		acc |= (BYTE)a[x] ^ (BYTE)b[x];

	return acc == 0;
}

static const char* client_aad_callback_result_str(freerdp_client_aad_callback_result result)
{
	switch (result)
	{
		case FREERDP_CLIENT_AAD_CALLBACK_UNRELATED:
			return "UNRELATED";
		case FREERDP_CLIENT_AAD_CALLBACK_CODE:
			return "CODE";
		case FREERDP_CLIENT_AAD_CALLBACK_ERROR:
			return "ERROR";
		case FREERDP_CLIENT_AAD_CALLBACK_INVALID:
		default:
			return "INVALID";
	}
}

freerdp_client_aad_callback_result freerdp_client_aad_parse_callback(rdpClientContext* cctx,
                                                                     const char* uri, char** code)
{
	freerdp_client_aad_callback_result result = FREERDP_CLIENT_AAD_CALLBACK_INVALID;
	client_uri actual = WINPR_C_ARRAY_INIT;
	client_uri expected = WINPR_C_ARRAY_INIT;
	char* state = nullptr;
	char* value = nullptr;

	if (code)
		*code = nullptr;

	if (!cctx || !uri)
		return FREERDP_CLIENT_AAD_CALLBACK_INVALID;

	client_aad_oauth* oauth = cctx->aad_oauth;
	if (!oauth || !oauth->state || !oauth->redirect_uri)
	{
		WLog_ERR(TAG, "no AAD authorization request is in flight");
		return FREERDP_CLIENT_AAD_CALLBACK_INVALID;
	}

	/* An authorization response is answered once. A second one, whatever it carries, is a
	 * replay of the first or a response to a request this client did not make. */
	if (oauth->consumed)
	{
		WLog_ERR(TAG, "the AAD authorization request was already answered");
		return FREERDP_CLIENT_AAD_CALLBACK_INVALID;
	}

	if (!client_uri_parse(oauth->redirect_uri, &expected))
	{
		WLog_ERR(TAG, "the redirect URI of the authorization request is not an absolute URI");
		goto cleanup;
	}

	/* A web view navigates to all kinds of URIs, only the redirect URI is ours. */
	if (!client_uri_parse(uri, &actual) || !client_uri_same_destination(&actual, &expected))
	{
		result = FREERDP_CLIENT_AAD_CALLBACK_UNRELATED;
		goto cleanup;
	}

	if (actual.userinfo || actual.fragment)
		goto cleanup;

	if ((client_uri_query_value(actual.query, "state", &state) != CLIENT_QUERY_FOUND) || !state ||
	    !client_const_time_equal(state, oauth->state))
		goto cleanup;

	const client_query_result hasCode = client_uri_query_value(actual.query, "code", &value);
	const client_query_result hasError = client_uri_query_value(actual.query, "error", nullptr);

	if ((hasCode == CLIENT_QUERY_FOUND) && (hasError == CLIENT_QUERY_MISSING))
	{
		if (!value || (value[0] == '\0'))
			goto cleanup;

		result = FREERDP_CLIENT_AAD_CALLBACK_CODE;
		if (code)
		{
			*code = value;
			value = nullptr;
		}
	}
	else if ((hasError == CLIENT_QUERY_FOUND) && (hasCode == CLIENT_QUERY_MISSING))
		result = FREERDP_CLIENT_AAD_CALLBACK_ERROR;

cleanup:
	/* The transaction has reached its end, only the code verifier is still needed. */
	if ((result == FREERDP_CLIENT_AAD_CALLBACK_CODE) ||
	    (result == FREERDP_CLIENT_AAD_CALLBACK_ERROR))
		oauth->consumed = TRUE;

	/* Never log the URI or anything parsed out of it: it can hold an authorization code and
	 * the error description can name the account that was used. */
	WLog_Print(WLog_Get(TAG),
	           (result == FREERDP_CLIENT_AAD_CALLBACK_UNRELATED) ? WLOG_DEBUG : WLOG_INFO,
	           "AAD authorization callback: %s", client_aad_callback_result_str(result));
	free(state);
	free(value);
	client_uri_free(&actual);
	client_uri_free(&expected);
	return result;
}

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

	WINPR_ASSERT(instance->context);
	WINPR_ASSERT(instance->context->update);
	instance->context->update->SaveSessionInfo = client_common_save_session_info;
}

static void client_cli_user_notification(void* context, const UserNotificationEventArgs* e)
{
	WINPR_UNUSED(context);
	WINPR_ASSERT(e);
	if (strcmp(e->e.Sender, RDPEWA_CHANNEL_NAME) != 0)
		return;

	if (!e->message || e->message[0] == '\0')
		return;
	(void)fprintf(stderr, "[%s] Touch the security key\n", e->e.Sender);
	(void)fflush(stderr);
}

static BOOL freerdp_client_common_new(freerdp* instance, rdpContext* context)
{
	RDP_CLIENT_ENTRY_POINTS* pEntryPoints = nullptr;

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
	RDP_CLIENT_ENTRY_POINTS* pEntryPoints = nullptr;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(context);

	pEntryPoints = instance->pClientEntryPoints;
	WINPR_ASSERT(pEntryPoints);
	IFCALL(pEntryPoints->ClientFree, instance, context);

	freerdp_client_aad_reset((rdpClientContext*)context);
}

/* Common API */

rdpContext* freerdp_client_context_new(const RDP_CLIENT_ENTRY_POINTS* pEntryPoints)
{
	freerdp* instance = nullptr;
	rdpContext* context = nullptr;

	if (!pEntryPoints)
		return nullptr;

	if (!IFCALLRESULT(TRUE, pEntryPoints->GlobalInit))
		return nullptr;

	instance = freerdp_new();

	if (!instance)
		return nullptr;

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
	return nullptr;
}

void freerdp_client_context_free(rdpContext* context)
{
	freerdp* instance = nullptr;

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
	RDP_CLIENT_ENTRY_POINTS* pEntryPoints = nullptr;

	if (!context || !context->instance || !context->instance->pClientEntryPoints)
		return ERROR_BAD_ARGUMENTS;

	if (freerdp_settings_get_bool(context->settings, FreeRDP_UseCommonStdioCallbacks))
	{
		set_default_callbacks(context->instance);
		if (context->pubSub)
		{
			const int rc =
			    PubSub_SubscribeUserNotification(context->pubSub, client_cli_user_notification);
			if (rc < 0)
				return FALSE;
		}
	}

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
	RDP_CLIENT_ENTRY_POINTS* pEntryPoints = nullptr;

	if (!context || !context->instance || !context->instance->pClientEntryPoints)
		return ERROR_BAD_ARGUMENTS;

	pEntryPoints = context->instance->pClientEntryPoints;
	const int rc = IFCALLRESULT(CHANNEL_RC_OK, pEntryPoints->ClientStop, context);

	if (freerdp_settings_get_bool(context->settings, FreeRDP_UseCommonStdioCallbacks))
		PubSub_UnsubscribeUserNotification(context->pubSub, client_cli_user_notification);

#ifdef WITH_SSO_MIB
	rdpClientContext* client_context = (rdpClientContext*)context;
	sso_mib_free(client_context->mibClientWrapper);
	client_context->mibClientWrapper = nullptr;
#endif // WITH_SSO_MIB
	return rc;
}

freerdp* freerdp_client_get_instance(rdpContext* context)
{
	if (!context || !context->instance)
		return nullptr;

	return context->instance;
}

HANDLE freerdp_client_get_thread(rdpContext* context)
{
	if (!context)
		return nullptr;

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
	return freerdp_client_settings_parse_command_line_ex(settings, argc, argv, allowUnknown,
	                                                     nullptr, 0, nullptr, nullptr);
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
	rdpFile* file = nullptr;
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
	rdpFile* file = nullptr;
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
	rdpFile* file = nullptr;
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
	char* filename = nullptr;
	char* password = nullptr;
	rdpAssistanceFile* file = nullptr;

	if (!settings || !argv || (argc < 2))
		return -1;

	filename = argv[1];

	for (int x = 2; x < argc; x++)
	{
		const char* key = strstr(argv[x], "assistance:");

		if (key)
		{
			char* sep = strchr(key, ':');
			if (!sep)
				return -1;

			password = sep + 1;
		}
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

	char* line = nullptr;
	if (suggestion && strlen(suggestion) > 0)
	{
		line = _strdup(suggestion);
		size = strlen(suggestion);
	}

	const SSIZE_T rc = freerdp_interruptible_get_line(instance->context, &line, &size, stdin);
	if (rc < 0)
	{
		char ebuffer[256] = WINPR_C_ARRAY_INIT;
		WLog_ERR(TAG, "freerdp_interruptible_get_line returned %s [%d]",
		         winpr_strerror(errno, ebuffer, sizeof(ebuffer)), errno);
		free(line);
		return -1;
	}

	free(*result);
	*result = nullptr;

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
		case AUTH_FIDO_PIN:
			pwdAuth = "FIDO2 PIN:       ";
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
		if (rc == nullptr)
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
	*username = nullptr;
	*domain = nullptr;
	*password = nullptr;
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
		case AUTH_FIDO_PIN:
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
	char* p = nullptr;

	printf("Multiple smartcards are available for use:\n");
	for (DWORD i = 0; i < count; i++)
	{
		const SmartcardCertInfo* cert = cert_list[i];
		char* reader = ConvertWCharToUtf8Alloc(cert->reader, nullptr);
		char* container_name = ConvertWCharToUtf8Alloc(cert->containerName, nullptr);

		printf("[%" PRIu32
		       "] %s\n\tReader: %s\n\tUser: %s@%s\n\tSubject: %s\n\tIssuer: %s\n\tUPN: %s\n",
		       i, container_name, reader, cert->userHint, cert->domainHint, cert->subject,
		       cert->issuer, cert->upn);

		free(reader);
		free(container_name);
	}

	while (1)
	{
		char input[10] = WINPR_C_ARRAY_INIT;

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
				printf("\n");
				if (answer == EOF)
					return 0;
				return 1;

			case 't':
			case 'T':
				answer = freerdp_interruptible_getc(instance->context, stdin);
				printf("\n");
				if (answer == EOF)
					return 0;
				return 2;

			case 'n':
			case 'N':
				answer = freerdp_interruptible_getc(instance->context, stdin);
				printf("\n");
				if (answer == EOF)
					return 0;
				return 0;

			default:
				break;
		}
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
		return nullptr;

	char* fp = freerdp_certificate_get_fingerprint(cert);
	char* start = freerdp_certificate_get_validity(cert, TRUE);
	char* end = freerdp_certificate_get_validity(cert, FALSE);
	freerdp_certificate_free(cert);

	char* str = nullptr;
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
		LPSTR msg = ConvertWCharNToUtf8Alloc(message, length / sizeof(WCHAR), nullptr);
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
		const int answer = freerdp_interruptible_getc(instance->context, stdin);

		if ((answer == EOF) || feof(stdin))
		{
			printf("\nError: Could not read answer from stdin.\n");
			return FALSE;
		}

		const int confirm = freerdp_interruptible_getc(instance->context, stdin);
		switch (answer)
		{
			case 'y':
			case 'Y':
				printf("\n");
				return confirm != EOF;

			case 'n':
			case 'N':
				printf("\n");
				return FALSE;

			default:
				break;
		}
	}

	return TRUE;
}

#if defined(WITH_AAD)
/** @brief Strip what a terminal or a mail client wrapped around a pasted URI
 *
 *  A pasted line arrives with the line ending, often with leading white space and sometimes
 *  inside the angle brackets or quotes that clients put around a URI. None of that belongs to
 *  the URI, and one stray character is enough to fail the single attempt this flow has.
 *
 *  @param url The line to trim in place
 *  @return The first character of the URI in \b url
 */
static char* client_trim_pasted_uri(char* url)
{
	char* start = url;
	size_t len = strlen(start);

	for (BOOL done = FALSE; !done;)
	{
		done = TRUE;

		while ((len > 0) && isspace((unsigned char)start[0]))
		{
			start++;
			len--;
			done = FALSE;
		}

		while ((len > 0) && isspace((unsigned char)start[len - 1]))
		{
			start[--len] = '\0';
			done = FALSE;
		}

		/* <https://...>, "https://..." and 'https://...' all show up in pasted lines. */
		const char first = (len >= 2) ? start[0] : '\0';
		const char last = (len >= 2) ? start[len - 1] : '\0';
		if (((first == '<') && (last == '>')) ||
		    (((first == '"') || (first == '\'')) && (last == first)))
		{
			start[len - 1] = '\0';
			start++;
			len -= 2;
			done = FALSE;
		}
	}

	return start;
}

/** @brief Read the redirect URI the user pasted and return the authorization code
 *
 *  @param instance The RDP instance to read for
 *  @param cctx The client context the authorization request was built with
 *  @return The percent decoded authorization code or \b nullptr
 */
WINPR_ATTR_MALLOC(free, 1)
static char* client_cli_read_authorization_code(freerdp* instance, rdpClientContext* cctx)
{
	size_t size = 0;
	char* url = nullptr;
	char* code = nullptr;

	printf("Paste redirect URL here: \n");

	if (freerdp_interruptible_get_line(instance->context, &url, &size, stdin) < 0)
	{
		free(url);
		return nullptr;
	}

	switch (freerdp_client_aad_parse_callback(cctx, client_trim_pasted_uri(url), &code))
	{
		case FREERDP_CLIENT_AAD_CALLBACK_CODE:
			break;
		case FREERDP_CLIENT_AAD_CALLBACK_ERROR:
			WLog_ERR(TAG, "the authorization server declined the request");
			break;
		case FREERDP_CLIENT_AAD_CALLBACK_UNRELATED:
			WLog_ERR(TAG, "this is not the redirect URI the request was made with");
			break;
		default:
			WLog_ERR(TAG, "the authorization response was rejected");
			break;
	}

	/* The pasted line holds the authorization code. */
	client_free_secret(url);
	return code;
}

static BOOL client_cli_get_rdsaad_access_token(freerdp* instance, const char* scope,
                                               const char* req_cnf, char** token)
{
	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

	char* token_request = nullptr;
	char* code = nullptr;

	WINPR_ASSERT(scope);
	WINPR_ASSERT(req_cnf);
	WINPR_ASSERT(token);

	BOOL rc = FALSE;
	*token = nullptr;

	rdpClientContext* cctx = (rdpClientContext*)instance->context;
	char* request = freerdp_client_get_aad_url(cctx, FREERDP_CLIENT_AAD_AUTH_REQUEST, scope);
	if (!request)
		return FALSE;

	printf("Browse to: %s\n", request);
	free(request);

	code = client_cli_read_authorization_code(instance, cctx);
	if (!code)
		goto cleanup;

	token_request =
	    freerdp_client_get_aad_url(cctx, FREERDP_CLIENT_AAD_TOKEN_REQUEST, scope, code, req_cnf);
	if (!token_request)
		goto cleanup;

	rc = client_common_get_access_token(instance, token_request, token);

cleanup:
	client_free_secret(token_request);
	client_free_secret(code);
	return rc && (*token != nullptr);
}

static BOOL client_cli_get_avd_access_token(freerdp* instance, char** token)
{
	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

	char* token_request = nullptr;
	char* code = nullptr;

	WINPR_ASSERT(token);

	BOOL rc = FALSE;

	*token = nullptr;

	rdpClientContext* cctx = (rdpClientContext*)instance->context;
	char* request = freerdp_client_get_aad_url(cctx, FREERDP_CLIENT_AAD_AVD_AUTH_REQUEST);
	if (!request)
		return FALSE;

	printf("Browse to: %s\n", request);
	free(request);

	code = client_cli_read_authorization_code(instance, cctx);
	if (!code)
		goto cleanup;

	token_request = freerdp_client_get_aad_url(cctx, FREERDP_CLIENT_AAD_AVD_TOKEN_REQUEST, code);
	if (!token_request)
		goto cleanup;

	rc = client_common_get_access_token(instance, token_request, token);

cleanup:
	client_free_secret(token_request);
	client_free_secret(code);
	return rc && (*token != nullptr);
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
			}
			else
			{
				if (count > 2)
					WLog_WARN(
					    TAG,
					    "ACCESS_TOKEN_TYPE_AAD expected 2 additional arguments, but got %" PRIuz
					    ", ignoring",
					    count);
				va_list ap = WINPR_C_ARRAY_INIT;
				va_start(ap, count);
				const char* scope = va_arg(ap, const char*);
				const char* req_cnf = va_arg(ap, const char*);
				rc = client_cli_get_rdsaad_access_token(instance, scope, req_cnf, token);
				va_end(ap);
			}
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
	BYTE* response = nullptr;
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
		char buffer[64] = WINPR_C_ARRAY_INIT;

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
	return client_auto_reconnect_ex(instance, nullptr);
}

BOOL client_auto_reconnect_ex(freerdp* instance, BOOL (*window_events)(freerdp* instance))
{
	BOOL retry = TRUE;
	UINT32 error = 0;
	UINT32 numRetries = 0;
	rdpSettings* settings = nullptr;

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

	const UINT err = freerdp_get_last_error(instance->context);
	switch (err)
	{
		case FREERDP_ERROR_CONNECT_LOGON_FAILURE:
		case FREERDP_ERROR_CONNECT_CLIENT_REVOKED:
		case FREERDP_ERROR_CONNECT_WRONG_PASSWORD:
		case FREERDP_ERROR_CONNECT_ACCESS_DENIED:
		case FREERDP_ERROR_CONNECT_ACCOUNT_RESTRICTION:
		case FREERDP_ERROR_CONNECT_ACCOUNT_LOCKED_OUT:
		case FREERDP_ERROR_CONNECT_ACCOUNT_EXPIRED:
		case FREERDP_ERROR_CONNECT_NO_OR_MISSING_CREDENTIALS:
			WLog_WARN(TAG, "Connection aborted: credentials do not work [%s]",
			          freerdp_get_last_error_name(err));
			return FALSE;
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
		    IFCALLRESULT(5000, instance->RetryDialog, instance, "connection", numRetries, nullptr);
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
		cctx->thread = nullptr;
	}

	return 0;
}

#if defined(CHANNEL_ENCOMSP_CLIENT)
BOOL freerdp_client_encomsp_toggle_control(EncomspClientContext* encomsp)
{
	rdpClientContext* cctx = nullptr;
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
	ENCOMSP_CHANGE_PARTICIPANT_CONTROL_LEVEL_PDU pdu = WINPR_C_ARRAY_INIT;

	if (!encomsp)
		return FALSE;

	pdu.ParticipantId = encomsp->participantId;
	pdu.Flags = ENCOMSP_REQUEST_VIEW;

	if (control)
		pdu.Flags |= ENCOMSP_REQUEST_INTERACT;

	const UINT rc = encomsp->ChangeParticipantControlLevel(encomsp, &pdu);
	return rc == CHANNEL_RC_OK;
}

static UINT
client_encomsp_participant_created(EncomspClientContext* context,
                                   const ENCOMSP_PARTICIPANT_CREATED_PDU* participantCreated)
{
	rdpClientContext* cctx = nullptr;
	rdpSettings* settings = nullptr;
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
		encomsp->custom = nullptr;
		encomsp->ParticipantCreated = nullptr;
	}

	if (cctx)
		cctx->encomsp = nullptr;
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
		cctx->ainput = nullptr;
#endif
#if defined(CHANNEL_RDPEI_CLIENT)
	else if (strcmp(e->name, RDPEI_DVC_CHANNEL_NAME) == 0)
	{
		cctx->rdpei = nullptr;
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

	const CONNECTION_STATE state = freerdp_get_state(&cctx->context);
	if (state != CONNECTION_STATE_ACTIVE)
		return TRUE;

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
		return freerdp_input_send_mouse_event(cctx->context.input, mflags, 0, 0);

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
	const CONNECTION_STATE state = freerdp_get_state(&cctx->context);
	if (state != CONNECTION_STATE_ACTIVE)
		return TRUE;

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
		return freerdp_input_send_mouse_event(cctx->context.input, mflags, (UINT16)cctx->lastX,
		                                      (UINT16)cctx->lastY);
	}
	return TRUE;
}

BOOL freerdp_client_send_extended_button_event(rdpClientContext* cctx, BOOL relative, UINT16 mflags,
                                               INT32 x, INT32 y)
{
	BOOL handled = FALSE;
	WINPR_ASSERT(cctx);

	const CONNECTION_STATE state = freerdp_get_state(&cctx->context);
	if (state != CONNECTION_STATE_ACTIVE)
		return TRUE;

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
		const UINT rc1 =
		    rdpei->TouchRawEvent(rdpei, contact->id, contact->x, contact->y, &contactId,
		                         RDPINPUT_CONTACT_FLAG_UPDATE | RDPINPUT_CONTACT_FLAG_INRANGE |
		                             RDPINPUT_CONTACT_FLAG_INCONTACT,
		                         contactFlags, contact->pressure);
		if (rc1 != CHANNEL_RC_OK)
			return FALSE;

		const UINT rc2 = rdpei->TouchRawEvent(rdpei, contact->id, contact->x, contact->y,
		                                      &contactId, flags, contactFlags, contact->pressure);
		if (rc2 != CHANNEL_RC_OK)
			return FALSE;
	}
	else
	{
		WINPR_ASSERT(rdpei->TouchEnd);
		const UINT rc = rdpei->TouchEnd(rdpei, contact->id, contact->x, contact->y, &contactId);
		if (rc != CHANNEL_RC_OK)
			return FALSE;
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
		const UINT rc = rdpei->TouchRawEvent(rdpei, contact->id, contact->x, contact->y, &contactId,
		                                     flags, contactFlags, contact->pressure);
		if (rc != CHANNEL_RC_OK)
			return FALSE;
	}
	else
	{
		WINPR_ASSERT(rdpei->TouchBegin);
		const UINT rc = rdpei->TouchBegin(rdpei, contact->id, contact->x, contact->y, &contactId);
		if (rc != CHANNEL_RC_OK)
			return FALSE;
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
		const UINT rc = rdpei->TouchRawEvent(rdpei, contact->id, contact->x, contact->y, &contactId,
		                                     flags, contactFlags, contact->pressure);
		if (rc != CHANNEL_RC_OK)
			return FALSE;
	}
	else
	{
		WINPR_ASSERT(rdpei->TouchUpdate);
		const UINT rc = rdpei->TouchUpdate(rdpei, contact->id, contact->x, contact->y, &contactId);
		if (rc != CHANNEL_RC_OK)
			return FALSE;
	}

	return TRUE;
#else
	WLog_WARN(TAG, "Touch event detected but RDPEI support not compiled in. Recompile with "
	               "-DCHANNEL_RDPEI_CLIENT=ON");
	return freerdp_handle_touch_motion_to_mouse(cctx, contact);
#endif
}

static BOOL freerdp_handle_touch_cancel(rdpClientContext* cctx, const FreeRDP_TouchContact* contact)
{
	WINPR_ASSERT(cctx);
	WINPR_ASSERT(contact);

#if defined(CHANNEL_RDPEI_CLIENT)
	RdpeiClientContext* rdpei = cctx->rdpei;

	if (!rdpei)
		return freerdp_handle_touch_to_mouse(cctx, false, contact);

	int contactId = 0;

	if (rdpei->TouchRawEvent)
	{
		const UINT32 flags = RDPINPUT_CONTACT_FLAG_UPDATE | RDPINPUT_CONTACT_FLAG_CANCELED;
		const UINT32 contactFlags = ((contact->flags & FREERDP_TOUCH_HAS_PRESSURE) != 0)
		                                ? CONTACT_DATA_PRESSURE_PRESENT
		                                : 0;
		const UINT rc = rdpei->TouchRawEvent(rdpei, contact->id, contact->x, contact->y, &contactId,
		                                     flags, contactFlags, contact->pressure);
		if (rc != CHANNEL_RC_OK)
			return FALSE;
	}
	else
	{
		WINPR_ASSERT(rdpei->TouchUpdate);
		const UINT rc = rdpei->TouchEnd(rdpei, contact->id, contact->x, contact->y, &contactId);
		if (rc != CHANNEL_RC_OK)
			return FALSE;
	}

	return TRUE;
#else
	WLog_WARN(TAG, "Touch event detected but RDPEI support not compiled in. Recompile with "
	               "-DCHANNEL_RDPEI_CLIENT=ON");
	return freerdp_handle_touch_to_mouse(cctx, false, contact);
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
				FreeRDP_TouchContact empty = WINPR_C_ARRAY_INIT;
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
	const UINT32 mask =
	    FREERDP_TOUCH_DOWN | FREERDP_TOUCH_UP | FREERDP_TOUCH_MOTION | FREERDP_TOUCH_CANCEL;
	WINPR_ASSERT(cctx);

	const CONNECTION_STATE state = freerdp_get_state(&cctx->context);
	if (state != CONNECTION_STATE_ACTIVE)
		return TRUE;

	FreeRDP_TouchContact contact = WINPR_C_ARRAY_INIT;

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
		case FREERDP_TOUCH_CANCEL:
			return freerdp_handle_touch_cancel(cctx, &contact);
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
	return nullptr;
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
		const FreeRDP_PenDevice empty = WINPR_C_ARRAY_INIT;
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
	const CONNECTION_STATE state = freerdp_get_state(&cctx->context);
	if (state != CONNECTION_STATE_ACTIVE)
		return TRUE;

#if defined(CHANNEL_RDPEI_CLIENT)
	if ((flags & FREERDP_PEN_REGISTER) != 0)
	{
		va_list args = WINPR_C_ARRAY_INIT;

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
	va_list args = WINPR_C_ARRAY_INIT;
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

	const CONNECTION_STATE state = freerdp_get_state(&cctx->context);
	if (state != CONNECTION_STATE_ACTIVE)
		return TRUE;

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
			const UINT rc =
			    rdpei->PenHoverCancel(rdpei, pen->deviceid, 0, pen->last_x, pen->last_y);
			if (rc != CHANNEL_RC_OK)
				return FALSE;
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

BOOL freerdp_client_use_relative_mouse_events(rdpClientContext* cctx)
{
	WINPR_ASSERT(cctx);

	const rdpSettings* settings = cctx->context.settings;
	const BOOL useRelative = freerdp_settings_get_bool(settings, FreeRDP_MouseUseRelativeMove);
	const BOOL haveRelative = freerdp_settings_get_bool(settings, FreeRDP_HasRelativeMouseEvent);
	BOOL ainput = FALSE;
#if defined(CHANNEL_AINPUT_CLIENT)
	ainput = cctx->ainput != nullptr;
#endif

	return useRelative && (haveRelative || ainput);
}

#if defined(WITH_AAD)

/** The longest tenant identifier accepted in a redirect URI.
 *
 *  Entra tenant identifiers are GUIDs or verified domain names; 128 characters is well beyond
 *  both and keeps the value from dominating the URL.
 */
#define CLIENT_AAD_TENANTID_MAXLEN 128

/** @brief Check a redirect URI format string taken from the settings
 *
 *  \b FreeRDP_GatewayAvdAccessAadFormat and \b FreeRDP_GatewayAvdAccessTokenFormat are
 *  passed to \b winpr_asprintf as the format string, so a value that does not describe the
 *  arguments the caller pushes reads past the end of the argument list. Accept literal text,
 *  \c %% and at most \b conversions \c %s; reject every other conversion, including length
 *  modifiers, positional arguments and \c %n.
 *
 *  Fewer conversions than expected are allowed: a cloud that publishes a fixed redirect URI
 *  configures a format string without any conversion.
 *
 *  @param fmt The format string to check
 *  @param conversions The number of \c %s conversions the caller supplies arguments for
 *  @param key The setting \b fmt was read from, for the error message
 *
 *  @return \b TRUE if \b fmt is safe to expand with \b conversions string arguments
 */
static BOOL client_aad_check_redirect_format(const char* fmt, size_t conversions,
                                             FreeRDP_Settings_Keys_String key)
{
	const char* name = freerdp_settings_get_name_for_key(key);

	if (!fmt)
	{
		WLog_ERR(TAG, "setting %s is not set", name);
		return FALSE;
	}

	size_t count = 0;
	for (const char* pos = strchr(fmt, '%'); pos; pos = strchr(pos, '%'))
	{
		switch (pos[1])
		{
			case '%':
				break;
			case 's':
				count++;
				break;
			default:
				WLog_ERR(TAG,
				         "setting %s uses an unsupported conversion, only '%%s' and '%%%%' "
				         "are allowed",
				         name);
				return FALSE;
		}
		pos += 2;
	}

	if (count > conversions)
	{
		WLog_ERR(TAG, "setting %s has %" PRIuz " '%%s' conversions, at most %" PRIuz " are allowed",
		         name, count, conversions);
		return FALSE;
	}

	return TRUE;
}

/** @brief Check a tenant identifier before it is expanded into a URL
 *
 *  \b isalnum() is locale dependent and a client that called \b setlocale() can have bytes
 *  >= 0x80 pass it, so the character class is spelled out. A label of dots only would walk the
 *  path of the URL the tenant is expanded into.
 *
 *  @param tenantid The value of \b FreeRDP_GatewayAvdAadtenantid or the default tenant
 *  @return \b TRUE if \b tenantid consists of ASCII alphanumerics, \c - and \c ., is not
 *          only dots and is not longer than \ref CLIENT_AAD_TENANTID_MAXLEN
 */
static BOOL client_aad_check_tenantid(const char* tenantid)
{
	const char* name = freerdp_settings_get_name_for_key(FreeRDP_GatewayAvdAadtenantid);

	if (!tenantid)
	{
		WLog_ERR(TAG, "setting %s is not set", name);
		return FALSE;
	}

	const size_t len = strnlen(tenantid, CLIENT_AAD_TENANTID_MAXLEN + 1);
	if ((len == 0) || (len > CLIENT_AAD_TENANTID_MAXLEN))
	{
		WLog_ERR(TAG, "setting %s must be 1 to %d characters long", name,
		         CLIENT_AAD_TENANTID_MAXLEN);
		return FALSE;
	}

	for (size_t x = 0; x < len; x++)
	{
		const char cur = tenantid[x];
		const BOOL alnum = ((cur >= '0') && (cur <= '9')) || ((cur >= 'a') && (cur <= 'z')) ||
		                   ((cur >= 'A') && (cur <= 'Z'));
		if (alnum || (cur == '-') || (cur == '.'))
			continue;

		WLog_ERR(TAG, "setting %s must only contain ASCII alphanumerics, '-' and '.'", name);
		return FALSE;
	}

	if (strspn(tenantid, ".") == len)
	{
		WLog_ERR(TAG, "setting %s must not consist of '.' only", name);
		return FALSE;
	}

	return TRUE;
}

WINPR_ATTR_MALLOC(free, 1)
static char* get_redirect_uri(const rdpSettings* settings)
{
	char* redirect_uri = nullptr;
	size_t redirect_len = 0;
	const bool cli = freerdp_settings_get_bool(settings, FreeRDP_UseCommonStdioCallbacks);
	if (cli)
	{
		const char* redirect_fmt =
		    freerdp_settings_get_string(settings, FreeRDP_GatewayAvdAccessAadFormat);
		const BOOL useTenant = freerdp_settings_get_bool(settings, FreeRDP_GatewayAvdUseTenantid);
		const char* tenantid = "common";
		if (useTenant)
			tenantid = freerdp_settings_get_string(settings, FreeRDP_GatewayAvdAadtenantid);

		const char* url =
		    freerdp_settings_get_string(settings, FreeRDP_GatewayAzureActiveDirectory);
		if (!url)
		{
			WLog_ERR(TAG, "setting %s is not set",
			         freerdp_settings_get_name_for_key(FreeRDP_GatewayAzureActiveDirectory));
			return nullptr;
		}

		if (!client_aad_check_tenantid(tenantid))
			return nullptr;

		if (!client_aad_check_redirect_format(redirect_fmt, 2, FreeRDP_GatewayAvdAccessAadFormat))
			return nullptr;

		winpr_asprintf(&redirect_uri, &redirect_len, redirect_fmt, url, tenantid);
	}
	else
	{
		const char* client_id = freerdp_settings_get_string(settings, FreeRDP_GatewayAvdClientID);
		const char* redirect_fmt =
		    freerdp_settings_get_string(settings, FreeRDP_GatewayAvdAccessTokenFormat);

		if (!client_id)
		{
			WLog_ERR(TAG, "setting %s is not set",
			         freerdp_settings_get_name_for_key(FreeRDP_GatewayAvdClientID));
			return nullptr;
		}

		if (!client_aad_check_redirect_format(redirect_fmt, 1, FreeRDP_GatewayAvdAccessTokenFormat))
			return nullptr;

		winpr_asprintf(&redirect_uri, &redirect_len, redirect_fmt, client_id);
	}
	return redirect_uri;
}

/** The number of random bytes behind the 'state' value and the PKCE code verifier.
 *
 *  32 bytes base64url encode to 43 characters, the shortest code verifier RFC 7636 allows.
 */
#define CLIENT_AAD_OAUTH_RANDOM_LEN 32

/** @brief Fill a buffer with unpredictable bytes
 *
 *  A build without a crypto backend has winpr_RAND() succeed without touching the buffer, which
 *  would make the 'state' value and the code verifier the same for every session. Reject a
 *  result that is all zero: a real draw of this size is that only with probability 2^-256.
 *  \b buffer is cleared first, so the check does not pass on what a previous draw left in a
 *  buffer that is used more than once.
 *
 *  @param buffer The buffer to fill
 *  @param len The size of \b buffer in bytes
 *  @return \b TRUE if \b buffer holds \b len random bytes
 */
static BOOL client_aad_oauth_random(BYTE* buffer, size_t len)
{
	memset(buffer, 0, len);

	if (winpr_RAND(buffer, len) < 0)
		return FALSE;

	BYTE acc = 0;
	for (size_t x = 0; x < len; x++)
		acc |= buffer[x];

	if (acc == 0)
	{
		WLog_ERR(TAG, "the random number generator returned zeros, no crypto backend?");
		return FALSE;
	}

	return TRUE;
}

/** @brief Generate the 'state' value and the PKCE code verifier of a transaction
 *
 *  @param oauth The transaction to fill in
 *  @param pchallenge Receives the base64url encoded SHA-256 of the code verifier
 *  @return \b TRUE on success
 */
static BOOL client_aad_oauth_generate(client_aad_oauth* oauth, char** pchallenge)
{
	BYTE random[CLIENT_AAD_OAUTH_RANDOM_LEN] = WINPR_C_ARRAY_INIT;
	BYTE hash[WINPR_SHA256_DIGEST_LENGTH] = WINPR_C_ARRAY_INIT;

	if (!client_aad_oauth_random(random, sizeof(random)))
		return FALSE;
	oauth->state = crypto_base64url_encode(random, sizeof(random));

	if (!client_aad_oauth_random(random, sizeof(random)))
		return FALSE;
	oauth->verifier = crypto_base64url_encode(random, sizeof(random));

	if (!oauth->state || !oauth->verifier)
		return FALSE;

	if (!winpr_Digest(WINPR_MD_SHA256, oauth->verifier, strlen(oauth->verifier), hash,
	                  sizeof(hash)))
		return FALSE;

	*pchallenge = crypto_base64url_encode(hash, sizeof(hash));
	return *pchallenge != nullptr;
}

/** @brief Start a new OAuth transaction on a client context
 *
 *  Replaces any transaction still attached to \b cctx.
 *
 *  @param cctx The client context to attach the transaction to
 *  @param redirect_uri The percent encoded redirect URI of the authorization request
 *  @return The query parameters to append to the authorization request or \b nullptr
 */
WINPR_ATTR_MALLOC(free, 1)
static char* client_aad_oauth_start(rdpClientContext* cctx, const char* redirect_uri)
{
	char* challenge = nullptr;
	char* params = nullptr;
	size_t paramslen = 0;

	freerdp_client_aad_reset(cctx);

	client_aad_oauth* oauth = calloc(1, sizeof(client_aad_oauth));
	if (!oauth)
		return nullptr;

	oauth->redirect_uri = winpr_str_url_decode(redirect_uri, strlen(redirect_uri));
	if (!oauth->redirect_uri)
		goto fail;

	if (!client_aad_oauth_generate(oauth, &challenge))
		goto fail;

	/* The state value and the challenge are base64url, so they need no escaping. */
	winpr_asprintf(&params, &paramslen, "&state=%s&code_challenge=%s&code_challenge_method=S256",
	               oauth->state, challenge);
	if (!params)
		goto fail;

	cctx->aad_oauth = oauth;
	free(challenge);
	return params;

fail:
	free(challenge);
	client_aad_oauth_free(oauth);
	return nullptr;
}

/** @brief The query parameters a token request has to carry for the running transaction
 *
 *  Leaves the transaction in place: it is released by \ref client_aad_oauth_token_done once the
 *  request body was built, so a build that fails does not lose the verifier.
 *
 *  @param cctx The client context the authorization request was built with
 *  @return An allocated string, empty if no transaction is in flight, or \b nullptr on error
 */
WINPR_ATTR_MALLOC(free, 1)
static char* client_aad_oauth_token_params(rdpClientContext* cctx)
{
	const client_aad_oauth* oauth = cctx->aad_oauth;
	if (!oauth || !oauth->verifier)
	{
		WLog_WARN(TAG, "no AAD authorization request was built with this client context, the "
		               "token request carries no PKCE code verifier");
		return _strdup("");
	}

	char* params = nullptr;
	size_t paramslen = 0;
	winpr_asprintf(&params, &paramslen, "&code_verifier=%s", oauth->verifier);
	return params;
}

/** @brief Release the transaction a token request was just built for
 *
 *  A code verifier belongs to exactly one token request, and the authorization code it was
 *  requested with is redeemed by that one request. Only a request that was built completely
 *  consumes them: a build that failed sent nothing, so its transaction stays usable.
 *
 *  @param cctx The client context the authorization request was built with
 *  @param request The request body that was built, or \b nullptr if the build failed
 */
static void client_aad_oauth_token_done(rdpClientContext* cctx, const char* request)
{
	if (request)
		freerdp_client_aad_reset(cctx);
}

static char* avd_auth_request(rdpClientContext* cctx, WINPR_ATTR_UNUSED va_list ap)
{
	const rdpSettings* settings = cctx->context.settings;
	const char* client_id = freerdp_settings_get_string(settings, FreeRDP_GatewayAvdClientID);
	const char* ep = freerdp_utils_aad_get_wellknown_string(&cctx->context,
	                                                        AAD_WELLKNOWN_authorization_endpoint);
	const char* scope = freerdp_settings_get_string(settings, FreeRDP_GatewayAvdScope);

	if (!client_id || !ep || !scope)
		return nullptr;

	char* redirect_uri = get_redirect_uri(settings);
	if (!redirect_uri)
		return nullptr;

	char* url = nullptr;
	size_t urllen = 0;
	char* oauth = client_aad_oauth_start(cctx, redirect_uri);
	if (oauth)
	{
		winpr_asprintf(&url, &urllen,
		               "%s?client_id=%s&response_type=code&scope=%s&redirect_uri=%s%s", ep,
		               client_id, scope, redirect_uri, oauth);
	}
	free(oauth);
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
		return nullptr;

	char* redirect_uri = get_redirect_uri(settings);
	if (!redirect_uri)
		return nullptr;

	char* url = nullptr;
	size_t urllen = 0;

	const char* code = va_arg(ap, const char*);
	char* oauth = client_aad_oauth_token_params(cctx);
	char* enccode = code ? winpr_str_url_encode(code, strlen(code)) : nullptr;
	if (oauth && enccode)
	{
		winpr_asprintf(
		    &url, &urllen,
		    "grant_type=authorization_code&code=%s&client_id=%s&scope=%s&redirect_uri=%s%s",
		    enccode, client_id, scope, redirect_uri, oauth);
	}
	client_aad_oauth_token_done(cctx, url);
	client_free_secret(enccode);
	client_free_secret(oauth);
	free(redirect_uri);
	return url;
}

static char* aad_auth_request(rdpClientContext* cctx, WINPR_ATTR_UNUSED va_list ap)
{
	const rdpSettings* settings = cctx->context.settings;
	char* url = nullptr;
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
			if (!ep)
				goto cleanup;

			char* oauth = client_aad_oauth_start(cctx, redirect_uri);
			if (oauth)
			{
				winpr_asprintf(&url, &urllen,
				               "%s?client_id=%s&response_type=code&scope=%s&redirect_uri=%s%s", ep,
				               client_id, scope, redirect_uri, oauth);
			}
			free(oauth);
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
		return nullptr;

	char* redirect_uri = get_redirect_uri(settings);
	if (!redirect_uri)
		return nullptr;

	char* url = nullptr;
	size_t urllen = 0;

	char* oauth = client_aad_oauth_token_params(cctx);
	char* enccode = winpr_str_url_encode(code, strlen(code));
	if (oauth && enccode)
	{
		winpr_asprintf(&url, &urllen,
		               "grant_type=authorization_code&code=%s&client_id=%s&scope=%s&redirect_uri=%"
		               "s&req_cnf=%s%s",
		               enccode, client_id, scope, redirect_uri, req_cnf, oauth);
	}
	client_aad_oauth_token_done(cctx, url);
	client_free_secret(enccode);
	client_free_secret(oauth);
	free(redirect_uri);
	return url;
}
#endif

char* freerdp_client_get_aad_url(rdpClientContext* cctx, freerdp_client_aad_type type, ...)
{
	WINPR_ASSERT(cctx);
	char* str = nullptr;

	va_list ap = WINPR_C_ARRAY_INIT;
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

BOOL client_common_save_session_info(WINPR_ATTR_UNUSED rdpContext* context, UINT32 type,
                                     const void* data)
{
	char buffer[128] = WINPR_C_ARRAY_INIT;
	WLog_INFO(TAG, "%s [%s]", freerdp_session_logon_type_str(type),
	          freerdp_session_logon_type_data_str(type, data, buffer, sizeof(buffer)));
	return TRUE;
}

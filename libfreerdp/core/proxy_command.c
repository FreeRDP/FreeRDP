/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * External command transport
 * SPDX-License-Identifier: Apache-2.0
 */

#include "proxy_command.h"

#include <errno.h>
#include <freerdp/error.h>
#include <freerdp/log.h>
#include <freerdp/utils/proxy_utils.h>
#include <winpr/crt.h>

#define TAG FREERDP_TAG("core.proxycommand")

BOOL proxy_command_check(proxyCommandBase* base)
{
	if (freerdp_shall_disconnect_context(base->context))
	{
		errno = ECANCELED;
		return FALSE;
	}
	if (!InterlockedCompareExchange(&base->received, 0, 0) && base->deadline &&
	    (GetTickCount64() >= base->deadline))
	{
		errno = ETIMEDOUT;
		freerdp_set_last_error_if_not(base->context, FREERDP_ERROR_CONNECT_TRANSPORT_FAILED);
		return FALSE;
	}
	return TRUE;
}

void proxy_command_mark_received(proxyCommandBase* base)
{
	/* The startup deadline only applies until the command delivers its first byte. */
	if (InterlockedExchange(&base->received, TRUE) == FALSE)
		WLog_DBG(TAG, "ProxyCommand delivered its first data, startup deadline cleared");
}

DWORD proxy_command_wait_timeout(proxyCommandBase* base, DWORD requested)
{
	/* Check cancellation at least every 100 ms, including infinite waits. */
	DWORD result = (requested > 100) ? 100 : requested;
	if (!InterlockedCompareExchange(&base->received, 0, 0) && base->deadline)
	{
		const ULONGLONG now = GetTickCount64();
		const ULONGLONG remaining = (now < base->deadline) ? base->deadline - now : 0;
		if (remaining < result)
			result = (DWORD)remaining;
	}
	return result;
}

BOOL proxy_command_validate_settings(rdpSettings* settings)
{
	const char* command = freerdp_settings_get_string(settings, FreeRDP_ProxyCommand);
	if (!command)
		return TRUE;

	const UINT32 proxy = freerdp_settings_get_uint32(settings, FreeRDP_ProxyType);
	if (!command[0] || freerdp_settings_get_bool(settings, FreeRDP_GatewayEnabled) ||
	    freerdp_settings_get_bool(settings, FreeRDP_GatewayArmTransport) ||
	    freerdp_settings_get_bool(settings, FreeRDP_ConnectChildSession) ||
	    ((proxy != PROXY_TYPE_NONE) && (proxy != PROXY_TYPE_IGNORE)))
	{
		WLog_ERR(TAG, "ProxyCommand requires a nonempty command and cannot be combined with "
		              "a gateway, a child session, or an HTTP/SOCKS proxy");
		return FALSE;
	}

	/* A command transports a byte stream only. Never negotiate a direct UDP connection. */
	return freerdp_settings_set_uint32(settings, FreeRDP_MultitransportFlags, 0) &&
	       freerdp_settings_set_bool(settings, FreeRDP_SupportMultitransport, FALSE);
}

static BOOL proxy_command_valid_host(const char* hostname)
{
	if (!hostname || !hostname[0] || (hostname[0] == '-'))
		return FALSE;
	/* Redirection hostnames are supplied by the server. They must not become shell syntax,
	 * even when the user's template surrounds %h with quotes. No local DNS lookup is needed. */
	for (const char* p = hostname; *p; p++)
	{
#if defined(_WIN32)
		/* An explicitly requested cmd.exe shell may expand %environment% variables. */
		if (*p == '%')
			return FALSE;
#endif
		if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
		    strchr("._:%-", *p))
			continue;
		return FALSE;
	}
	return TRUE;
}

static char* proxy_command_expand(const char* command, const char* hostname, int port)
{
	if (!command || !command[0] || !proxy_command_valid_host(hostname) || (port <= 0) ||
	    (port > UINT16_MAX))
		return nullptr;

	char portString[16] = WINPR_C_ARRAY_INIT;
	_snprintf(portString, sizeof(portString), "%d", port);
	const size_t hostLength = strlen(hostname);
	const size_t portLength = strlen(portString);
	size_t length = 0;
	/* Two passes: validate all tokens and sizes before writing the expanded command. */
	for (const char* p = command; *p; p++)
	{
		size_t add = 1;
		if (*p == '%')
		{
			p++;
			switch (*p)
			{
				case 'h':
					add = hostLength;
					break;
				case 'p':
					add = portLength;
					break;
				case '%':
					break;
				default:
					return nullptr;
			}
		}
		if (add >= SIZE_MAX - length)
			return nullptr;
		length += add;
	}
	char* result = calloc(length + 1, 1);
	if (!result)
		return nullptr;
	char* out = result;
	for (const char* p = command; *p; p++)
	{
		if (*p != '%')
		{
			*out++ = *p;
			continue;
		}
		p++;
		const char* replacement = (*p == 'h') ? hostname : ((*p == 'p') ? portString : "%");
		const size_t n = strlen(replacement);
		memcpy(out, replacement, n);
		out += n;
	}
	return result;
}

rdpTransportLayer* proxy_command_connect(rdpContext* context, const char* hostname, int port,
                                         DWORD timeout)
{
	if (!proxy_command_validate_settings(context->settings))
		return nullptr;
	const char* command = freerdp_settings_get_string(context->settings, FreeRDP_ProxyCommand);
	char* expanded = proxy_command_expand(command, hostname, port);
	if (!expanded)
	{
		WLog_ERR(TAG, "Invalid ProxyCommand token, target hostname, or port");
		freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_TRANSPORT_FAILED);
		return nullptr;
	}
	rdpTransportLayer* layer = proxy_command_platform_connect(context, expanded, timeout);
	free(expanded);
	if (!layer)
		freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_TRANSPORT_FAILED);
	return layer;
}

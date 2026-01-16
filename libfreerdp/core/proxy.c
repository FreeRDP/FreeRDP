/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * HTTP Proxy support
 *
 * Copyright 2016 Christian Plattner <ccpp@gmx.at>
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
#include <errno.h>
#include <limits.h>

#include <openssl/err.h>

#include "settings.h"
#include "proxy.h"
#include <freerdp/settings.h>
#include <freerdp/utils/proxy_utils.h>
#include <freerdp/crypto/crypto.h>
#include "tcp.h"

#include <winpr/assert.h>
#include <winpr/sysinfo.h>
#include <winpr/environment.h> /* For GetEnvironmentVariableA */

#define CRLF "\r\n"

#include <freerdp/log.h>
#define TAG FREERDP_TAG("core.proxy")

/* SOCKS Proxy auth methods by rfc1928 */
enum
{
	AUTH_M_NO_AUTH = 0,
	AUTH_M_GSSAPI = 1,
	AUTH_M_USR_PASS = 2
};

enum
{
	SOCKS_CMD_CONNECT = 1,
	SOCKS_CMD_BIND = 2,
	SOCKS_CMD_UDP_ASSOCIATE = 3
};

enum
{
	SOCKS_ADDR_IPV4 = 1,
	SOCKS_ADDR_FQDN = 3,
	SOCKS_ADDR_IPV6 = 4,
};

static const char logprefix[] = "SOCKS Proxy:";

/* CONN REQ replies in enum. order */
static const char* rplstat[] = { "succeeded",
	                             "general SOCKS server failure",
	                             "connection not allowed by ruleset",
	                             "Network unreachable",
	                             "Host unreachable",
	                             "Connection refused",
	                             "TTL expired",
	                             "Command not supported",
	                             "Address type not supported" };

static BOOL http_proxy_connect(rdpContext* context, BIO* bufferedBio, const char* proxyUsername,
                               const char* proxyPassword, const char* hostname, UINT16 port);
static BOOL socks_proxy_connect(rdpContext* context, BIO* bufferedBio, const char* proxyUsername,
                                const char* proxyPassword, const char* hostname, UINT16 port);
static void proxy_read_environment(rdpSettings* settings, char* envname);

BOOL proxy_prepare(rdpSettings* settings, const char** lpPeerHostname, UINT16* lpPeerPort,
                   const char** lpProxyUsername, const char** lpProxyPassword)
{
	if (freerdp_settings_get_uint32(settings, FreeRDP_ProxyType) == PROXY_TYPE_IGNORE)
		return FALSE;

	/* For TSGateway, find the system HTTPS proxy automatically */
	if (freerdp_settings_get_uint32(settings, FreeRDP_ProxyType) == PROXY_TYPE_NONE)
		proxy_read_environment(settings, "https_proxy");

	if (freerdp_settings_get_uint32(settings, FreeRDP_ProxyType) == PROXY_TYPE_NONE)
		proxy_read_environment(settings, "HTTPS_PROXY");

	if (freerdp_settings_get_uint32(settings, FreeRDP_ProxyType) != PROXY_TYPE_NONE)
		proxy_read_environment(settings, "no_proxy");

	if (freerdp_settings_get_uint32(settings, FreeRDP_ProxyType) != PROXY_TYPE_NONE)
		proxy_read_environment(settings, "NO_PROXY");

	if (freerdp_settings_get_uint32(settings, FreeRDP_ProxyType) != PROXY_TYPE_NONE)
	{
		*lpPeerHostname = freerdp_settings_get_string(settings, FreeRDP_ProxyHostname);
		*lpPeerPort = freerdp_settings_get_uint16(settings, FreeRDP_ProxyPort);
		*lpProxyUsername = freerdp_settings_get_string(settings, FreeRDP_ProxyUsername);
		*lpProxyPassword = freerdp_settings_get_string(settings, FreeRDP_ProxyPassword);
		return TRUE;
	}

	return FALSE;
}

static BOOL value_to_int(const char* value, LONGLONG* result, LONGLONG min, LONGLONG max)
{
	long long rc = 0;

	if (!value || !result)
		return FALSE;

	errno = 0;
	rc = _strtoi64(value, NULL, 0);

	if (errno != 0)
		return FALSE;

	if ((rc < min) || (rc > max))
		return FALSE;

	*result = rc;
	return TRUE;
}

static BOOL cidr4_match(const struct in_addr* addr, const struct in_addr* net, BYTE bits)
{
	if (bits == 0)
		return TRUE;

	const uint32_t mask = htonl(0xFFFFFFFFu << (32 - bits));
	const uint32_t amask = addr->s_addr & mask;
	const uint32_t nmask = net->s_addr & mask;
	return amask == nmask;
}

static BOOL cidr6_match(const struct in6_addr* address, const struct in6_addr* network,
                        uint8_t bits)
{
	const uint32_t* a = (const uint32_t*)address;
	const uint32_t* n = (const uint32_t*)network;
	const size_t bits_whole = bits >> 5;
	const size_t bits_incomplete = bits & 0x1F;

	if (bits_whole)
	{
		if (memcmp(a, n, bits_whole << 2) != 0)
			return FALSE;
	}

	if (bits_incomplete)
	{
		uint32_t mask = htonl((0xFFFFFFFFu) << (32 - bits_incomplete));

		if ((a[bits_whole] ^ n[bits_whole]) & mask)
			return FALSE;
	}

	return TRUE;
}

static BOOL option_ends_with(const char* str, const char* ext)
{
	WINPR_ASSERT(str);
	WINPR_ASSERT(ext);
	const size_t strLen = strlen(str);
	const size_t extLen = strlen(ext);

	if (strLen < extLen)
		return FALSE;

	return _strnicmp(&str[strLen - extLen], ext, extLen) == 0;
}

/* no_proxy has no proper definition, so use curl as reference:
 * https://about.gitlab.com/blog/2021/01/27/we-need-to-talk-no-proxy/
 */
static BOOL no_proxy_match_host(const char* val, const char* hostname)
{
	WINPR_ASSERT(val);
	WINPR_ASSERT(hostname);

	/* match all */
	if (_stricmp("*", val) == 0)
		return TRUE;

	/* Strip leading . */
	if (val[0] == '.')
		val++;

	/* Match suffix */
	return option_ends_with(hostname, val);
}

static BOOL starts_with(const char* val, const char* prefix)
{
	const size_t plen = strlen(prefix);
	const size_t vlen = strlen(val);
	if (vlen < plen)
		return FALSE;
	return _strnicmp(val, prefix, plen) == 0;
}

static BOOL no_proxy_match_ip(const char* val, const char* hostname)
{
	WINPR_ASSERT(val);
	WINPR_ASSERT(hostname);

	struct sockaddr_in sa4 = { 0 };
	struct sockaddr_in6 sa6 = { 0 };

	if (inet_pton(AF_INET, hostname, &sa4.sin_addr) == 1)
	{
		/* Prefix match */
		if (starts_with(hostname, val))
			return TRUE;

		char* sub = strchr(val, '/');
		if (sub)
			*sub++ = '\0';

		struct sockaddr_in mask = { 0 };
		if (inet_pton(AF_INET, val, &mask.sin_addr) == 0)
			return FALSE;

		/* IP address match */
		if (memcmp(&mask, &sa4, sizeof(mask)) == 0)
			return TRUE;

		if (sub)
		{
			const unsigned long usub = strtoul(sub, NULL, 0);
			if ((errno == 0) && (usub <= UINT8_MAX))
				return cidr4_match(&sa4.sin_addr, &mask.sin_addr, (UINT8)usub);
		}
	}
	else if (inet_pton(AF_INET6, hostname, &sa6.sin6_addr) == 1)
	{
		if (val[0] == '[')
			val++;

		char str[INET6_ADDRSTRLEN + 1] = { 0 };
		strncpy(str, val, INET6_ADDRSTRLEN);

		const size_t len = strnlen(str, INET6_ADDRSTRLEN);
		if (len > 0)
		{
			if (str[len - 1] == ']')
				str[len - 1] = '\0';
		}

		/* Prefix match */
		if (starts_with(hostname, str))
			return TRUE;

		char* sub = strchr(str, '/');
		if (sub)
			*sub++ = '\0';

		struct sockaddr_in6 mask = { 0 };
		if (inet_pton(AF_INET6, str, &mask.sin6_addr) == 0)
			return FALSE;

		/* Address match */
		if (memcmp(&mask, &sa6, sizeof(mask)) == 0)
			return TRUE;

		if (sub)
		{
			const unsigned long usub = strtoul(sub, NULL, 0);
			if ((errno == 0) && (usub <= UINT8_MAX))
				return cidr6_match(&sa6.sin6_addr, &mask.sin6_addr, (UINT8)usub);
		}
	}

	return FALSE;
}

static BOOL check_no_proxy(rdpSettings* settings, const char* no_proxy)
{
	const char* delimiter = ", ";
	BOOL result = FALSE;
	char* context = NULL;

	if (!no_proxy || !settings)
		return FALSE;

	char* copy = _strdup(no_proxy);

	if (!copy)
		return FALSE;

	char* current = strtok_s(copy, delimiter, &context);

	while (current && !result)
	{
		const size_t currentlen = strlen(current);

		if (currentlen > 0)
		{
			WLog_DBG(TAG, "%s => %s (%" PRIuz ")", settings->ServerHostname, current, currentlen);

			if (no_proxy_match_host(current, settings->ServerHostname))
				result = TRUE;
			else if (no_proxy_match_ip(current, settings->ServerHostname))
				result = TRUE;
		}

		current = strtok_s(NULL, delimiter, &context);
	}

	free(copy);
	return result;
}

void proxy_read_environment(rdpSettings* settings, char* envname)
{
	const DWORD envlen = GetEnvironmentVariableA(envname, NULL, 0);

	if (!envlen || (envlen <= 1))
		return;

	char* env = calloc(1, envlen);

	if (!env)
	{
		WLog_ERR(TAG, "Not enough memory");
		return;
	}

	if (GetEnvironmentVariableA(envname, env, envlen) == envlen - 1)
	{
		if (_strnicmp("NO_PROXY", envname, 9) == 0)
		{
			if (check_no_proxy(settings, env))
			{
				WLog_INFO(TAG, "deactivating proxy: %s [%s=%s]",
				          freerdp_settings_get_string(settings, FreeRDP_ServerHostname), envname,
				          env);
				if (!freerdp_settings_set_uint32(settings, FreeRDP_ProxyType, PROXY_TYPE_NONE))
					WLog_WARN(TAG, "failed to set FreeRDP_ProxyType=PROXY_TYPE_NONE");
			}
		}
		else
		{
			if (!proxy_parse_uri(settings, env))
			{
				WLog_WARN(
				    TAG, "Error while parsing proxy URI from environment variable; ignoring proxy");
			}
		}
	}

	free(env);
}

BOOL proxy_parse_uri(rdpSettings* settings, const char* uri_in)
{
	BOOL rc = FALSE;
	const char* protocol = "";
	UINT16 port = 0;

	if (!settings || !uri_in)
		return FALSE;

	char* uri_copy = _strdup(uri_in);
	char* uri = uri_copy;
	if (!uri)
		goto fail;

	{
		char* p = strstr(uri, "://");
		if (p)
		{
			*p = '\0';

			if (_stricmp("no_proxy", uri) == 0)
			{
				if (!freerdp_settings_set_uint32(settings, FreeRDP_ProxyType, PROXY_TYPE_IGNORE))
					goto fail;
			}
			if (_stricmp("http", uri) == 0)
			{
				if (!freerdp_settings_set_uint32(settings, FreeRDP_ProxyType, PROXY_TYPE_HTTP))
					goto fail;
				protocol = "http";
			}
			else if (_stricmp("socks5", uri) == 0)
			{
				if (!freerdp_settings_set_uint32(settings, FreeRDP_ProxyType, PROXY_TYPE_SOCKS))
					goto fail;
				protocol = "socks5";
			}
			else
			{
				WLog_ERR(TAG, "Only HTTP and SOCKS5 proxies supported by now");
				goto fail;
			}

			uri = p + 3;
		}
		else
		{
			/* default proxy protocol is http */
			if (!freerdp_settings_set_uint32(settings, FreeRDP_ProxyType, PROXY_TYPE_HTTP))
				goto fail;
			protocol = "http";
		}
	}

	/* uri is now [user:password@]hostname:port */
	{
		char* atPtr = strrchr(uri, '@');

		if (atPtr)
		{
			/* got a login / password,
			 *				 atPtr
			 *				 v
			 * [user:password@]hostname:port
			 *		^
			 *		colonPtr
			 */
			char* colonPtr = strchr(uri, ':');

			if (!colonPtr || (colonPtr > atPtr))
			{
				WLog_ERR(TAG, "invalid syntax for proxy (contains no password)");
				goto fail;
			}

			*colonPtr = '\0';
			if (!freerdp_settings_set_string(settings, FreeRDP_ProxyUsername, uri))
			{
				WLog_ERR(TAG, "unable to allocate proxy username");
				goto fail;
			}

			*atPtr = '\0';

			if (!freerdp_settings_set_string(settings, FreeRDP_ProxyPassword, colonPtr + 1))
			{
				WLog_ERR(TAG, "unable to allocate proxy password");
				goto fail;
			}

			uri = atPtr + 1;
		}
	}

	{
		char* p = strchr(uri, ':');

		if (p)
		{
			LONGLONG val = 0;

			if (!value_to_int(&p[1], &val, 0, UINT16_MAX))
			{
				WLog_ERR(TAG, "invalid syntax for proxy (invalid port)");
				goto fail;
			}

			if (val == 0)
			{
				WLog_ERR(TAG, "invalid syntax for proxy (port missing)");
				goto fail;
			}

			port = (UINT16)val;
			*p = '\0';
		}
		else
		{
			if (_stricmp("http", protocol) == 0)
			{
				/* The default is 80. Also for Proxies. */
				port = 80;
			}
			else
			{
				port = 1080;
			}

			WLog_DBG(TAG, "setting default proxy port: %" PRIu16, port);
		}

		if (!freerdp_settings_set_uint16(settings, FreeRDP_ProxyPort, port))
			goto fail;
	}
	{
		char* p = strchr(uri, '/');
		if (p)
			*p = '\0';
		if (!freerdp_settings_set_string(settings, FreeRDP_ProxyHostname, uri))
			goto fail;
	}

	if (_stricmp("", uri) == 0)
	{
		WLog_ERR(TAG, "invalid syntax for proxy (hostname missing)");
		goto fail;
	}

	if (freerdp_settings_get_string(settings, FreeRDP_ProxyUsername))
	{
		WLog_INFO(TAG, "Parsed proxy configuration: %s://%s:%s@%s:%" PRIu16, protocol,
		          freerdp_settings_get_string(settings, FreeRDP_ProxyUsername), "******",
		          freerdp_settings_get_string(settings, FreeRDP_ProxyHostname),
		          freerdp_settings_get_uint16(settings, FreeRDP_ProxyPort));
	}
	else
	{
		WLog_INFO(TAG, "Parsed proxy configuration: %s://%s:%" PRIu16, protocol,
		          freerdp_settings_get_string(settings, FreeRDP_ProxyHostname),
		          freerdp_settings_get_uint16(settings, FreeRDP_ProxyPort));
	}
	rc = TRUE;

fail:
	if (!rc)
		WLog_WARN(TAG, "Failed to parse proxy configuration: %s://%s:%" PRIu16, protocol, uri,
		          port);
	free(uri_copy);
	return rc;
}

BOOL proxy_connect(rdpContext* context, BIO* bufferedBio, const char* proxyUsername,
                   const char* proxyPassword, const char* hostname, UINT16 port)
{
	WINPR_ASSERT(context);
	rdpSettings* settings = context->settings;

	switch (freerdp_settings_get_uint32(settings, FreeRDP_ProxyType))
	{
		case PROXY_TYPE_NONE:
		case PROXY_TYPE_IGNORE:
			return TRUE;

		case PROXY_TYPE_HTTP:
			return http_proxy_connect(context, bufferedBio, proxyUsername, proxyPassword, hostname,
			                          port);

		case PROXY_TYPE_SOCKS:
			return socks_proxy_connect(context, bufferedBio, proxyUsername, proxyPassword, hostname,
			                           port);

		default:
			WLog_ERR(TAG, "Invalid internal proxy configuration");
			return FALSE;
	}
}

static const char* get_response_header(char* response)
{
	char* current_pos = strchr(response, '\r');
	if (!current_pos)
		current_pos = strchr(response, '\n');

	if (current_pos)
		*current_pos = '\0';

	return response;
}

static BOOL http_proxy_connect(rdpContext* context, BIO* bufferedBio, const char* proxyUsername,
                               const char* proxyPassword, const char* hostname, UINT16 port)
{
	BOOL rc = FALSE;
	int status = 0;
	wStream* s = NULL;
	char port_str[10] = { 0 };
	char recv_buf[256] = { 0 };
	char* eol = NULL;
	size_t resultsize = 0;
	size_t reserveSize = 0;
	size_t portLen = 0;
	size_t hostLen = 0;
	const char connect[] = "CONNECT ";
	const char httpheader[] = " HTTP/1.1" CRLF "Host: ";

	WINPR_ASSERT(context);
	WINPR_ASSERT(bufferedBio);
	WINPR_ASSERT(hostname);
	const UINT32 timeout =
	    freerdp_settings_get_uint32(context->settings, FreeRDP_TcpConnectTimeout);

	_itoa_s(port, port_str, sizeof(port_str), 10);

	hostLen = strlen(hostname);
	portLen = strnlen(port_str, sizeof(port_str));
	reserveSize = strlen(connect) + (hostLen + 1 + portLen) * 2 + strlen(httpheader);
	s = Stream_New(NULL, reserveSize);
	if (!s)
		goto fail;

	Stream_Write(s, connect, strlen(connect));
	Stream_Write(s, hostname, hostLen);
	Stream_Write_UINT8(s, ':');
	Stream_Write(s, port_str, portLen);
	Stream_Write(s, httpheader, strlen(httpheader));
	Stream_Write(s, hostname, hostLen);
	Stream_Write_UINT8(s, ':');
	Stream_Write(s, port_str, portLen);

	if (proxyUsername && proxyPassword)
	{
		const int length = _scprintf("%s:%s", proxyUsername, proxyPassword);
		if (length > 0)
		{
			const size_t size = (size_t)length + 1ull;
			char* creds = (char*)malloc(size);

			if (!creds)
				goto fail;
			else
			{
				const char basic[] = CRLF "Proxy-Authorization: Basic ";
				char* base64 = NULL;

				(void)sprintf_s(creds, size, "%s:%s", proxyUsername, proxyPassword);
				base64 = crypto_base64_encode((const BYTE*)creds, size - 1);

				if (!base64 || !Stream_EnsureRemainingCapacity(s, strlen(basic) + strlen(base64)))
				{
					free(base64);
					free(creds);
					goto fail;
				}
				Stream_Write(s, basic, strlen(basic));
				Stream_Write(s, base64, strlen(base64));

				free(base64);
			}
			free(creds);
		}
	}

	if (!Stream_EnsureRemainingCapacity(s, 4))
		goto fail;

	Stream_Write(s, CRLF CRLF, 4);
	ERR_clear_error();

	{
		const size_t pos = Stream_GetPosition(s);
		if (pos > INT32_MAX)
			goto fail;

		status = BIO_write(bufferedBio, Stream_Buffer(s), WINPR_ASSERTING_INT_CAST(int, pos));
	}

	if ((status < 0) || ((size_t)status != Stream_GetPosition(s)))
	{
		WLog_ERR(TAG, "HTTP proxy: failed to write CONNECT request");
		goto fail;
	}

	/* Read result until CR-LF-CR-LF.
	 * Keep recv_buf a null-terminated string. */
	{
		const UINT64 start = GetTickCount64();
		while (strstr(recv_buf, CRLF CRLF) == NULL)
		{
			if (resultsize >= sizeof(recv_buf) - 1)
			{
				WLog_ERR(TAG, "HTTP Reply headers too long: %s", get_response_header(recv_buf));
				goto fail;
			}
			const size_t rdsize = sizeof(recv_buf) - resultsize - 1ULL;

			ERR_clear_error();

			WINPR_ASSERT(rdsize <= INT32_MAX);
			status = BIO_read(bufferedBio, (BYTE*)recv_buf + resultsize, (int)rdsize);

			if (status < 0)
			{
				/* Error? */
				if (!freerdp_shall_disconnect_context(context) && BIO_should_retry(bufferedBio))
				{
					USleep(100);
					continue;
				}

				WLog_ERR(TAG, "Failed reading reply from HTTP proxy (Status %d)", status);
				goto fail;
			}
			else if (status == 0)
			{
				const UINT64 now = GetTickCount64();
				const UINT64 diff = now - start;
				if (freerdp_shall_disconnect_context(context) || (now < start) || (diff > timeout))
				{
					/* Error? */
					WLog_ERR(TAG, "Failed reading reply from HTTP proxy (BIO_read returned zero)");
					goto fail;
				}
				Sleep(10);
			}

			resultsize += WINPR_ASSERTING_INT_CAST(size_t, status);
		}
	}

	/* Extract HTTP status line */
	eol = strchr(recv_buf, '\r');

	if (!eol)
	{
		/* should never happen */
		goto fail;
	}

	*eol = '\0';
	WLog_INFO(TAG, "HTTP Proxy: %s", recv_buf);

	if (strnlen(recv_buf, sizeof(recv_buf)) < 12)
		goto fail;

	recv_buf[7] = 'X';

	if (strncmp(recv_buf, "HTTP/1.X 200", 12) != 0)
		goto fail;

	rc = TRUE;
fail:
	Stream_Free(s, TRUE);
	return rc;
}

static int recv_socks_reply(rdpContext* context, BIO* bufferedBio, BYTE* buf, int len, char* reason,
                            BYTE checkVer)
{
	int status = 0;

	WINPR_ASSERT(context);

	const UINT32 timeout =
	    freerdp_settings_get_uint32(context->settings, FreeRDP_TcpConnectTimeout);
	const UINT64 start = GetTickCount64();
	for (;;)
	{
		ERR_clear_error();
		status = BIO_read(bufferedBio, buf, len);

		if (status > 0)
		{
			break;
		}
		else if (status < 0)
		{
			/* Error? */
			if (!freerdp_shall_disconnect_context(context) && BIO_should_retry(bufferedBio))
			{
				USleep(100);
				continue;
			}

			WLog_ERR(TAG, "Failed reading %s reply from SOCKS proxy (Status %d)", reason, status);
			return -1;
		}
		else if (status == 0)
		{
			const UINT64 now = GetTickCount64();
			const UINT64 diff = now - start;
			if (freerdp_shall_disconnect_context(context) || (now < start) || (diff > timeout))
			{
				/* Error? */
				WLog_ERR(TAG, "Failed reading %s reply from SOCKS proxy (BIO_read returned zero)",
				         reason);
				return status;
			}
			Sleep(10);
		}
		else // if (status == 0)
		{
			/* Error? */
			WLog_ERR(TAG, "Failed reading %s reply from SOCKS proxy (BIO_read returned zero)",
			         reason);
			return -1;
		}
	}

	if (status < 2)
	{
		WLog_ERR(TAG, "SOCKS Proxy reply packet too short (%s)", reason);
		return -1;
	}

	if (buf[0] != checkVer)
	{
		WLog_ERR(TAG, "SOCKS Proxy version is not 5 (%s)", reason);
		return -1;
	}

	return status;
}

static BOOL socks_proxy_userpass(rdpContext* context, BIO* bufferedBio, const char* proxyUsername,
                                 const char* proxyPassword)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(bufferedBio);

	if (!proxyUsername || !proxyPassword)
	{
		WLog_ERR(TAG, "%s invalid username (%p) or password (%p)", logprefix,
		         WINPR_CXX_COMPAT_CAST(const void*, proxyUsername),
		         WINPR_CXX_COMPAT_CAST(const void*, proxyPassword));
		return FALSE;
	}

	const size_t usernameLen = (BYTE)strnlen(proxyUsername, 256);
	if (usernameLen > 255)
	{
		WLog_ERR(TAG, "%s username too long (%" PRIuz ", max=255)", logprefix, usernameLen);
		return FALSE;
	}

	const size_t userpassLen = (BYTE)strnlen(proxyPassword, 256);
	if (userpassLen > 255)
	{
		WLog_ERR(TAG, "%s password too long (%" PRIuz ", max=255)", logprefix, userpassLen);
		return FALSE;
	}

	/* user/password v1 method */
	{
		BYTE buf[2 * 255 + 3] = { 0 };
		size_t offset = 0;
		buf[offset++] = 1;

		buf[offset++] = WINPR_ASSERTING_INT_CAST(uint8_t, usernameLen);
		memcpy(&buf[offset], proxyUsername, usernameLen);
		offset += usernameLen;

		buf[offset++] = WINPR_ASSERTING_INT_CAST(uint8_t, userpassLen);
		memcpy(&buf[offset], proxyPassword, userpassLen);
		offset += userpassLen;

		ERR_clear_error();
		const int ioffset = WINPR_ASSERTING_INT_CAST(int, offset);
		const int status = BIO_write(bufferedBio, buf, ioffset);

		if (status != ioffset)
		{
			WLog_ERR(TAG, "%s error writing user/password request", logprefix);
			return FALSE;
		}
	}

	BYTE buf[2] = { 0 };
	const int status = recv_socks_reply(context, bufferedBio, buf, sizeof(buf), "AUTH REQ", 1);

	if (status < 2)
		return FALSE;

	if (buf[1] != 0x00)
	{
		WLog_ERR(TAG, "%s invalid user/password", logprefix);
		return FALSE;
	}
	return TRUE;
}

static BOOL socks_proxy_connect(rdpContext* context, BIO* bufferedBio, const char* proxyUsername,
                                const char* proxyPassword, const char* hostname, UINT16 port)
{
	BYTE nauthMethods = 1;
	const size_t hostnlen = strnlen(hostname, 255);

	if (proxyUsername || proxyPassword)
		nauthMethods++;

	/* select auth. method */
	{
		const BYTE buf[] = { 5,            /* SOCKS version */
			                 nauthMethods, /* #of methods offered */
			                 AUTH_M_NO_AUTH, AUTH_M_USR_PASS };

		size_t writeLen = sizeof(buf);
		if (nauthMethods <= 1)
			writeLen--;

		ERR_clear_error();
		const int iwriteLen = WINPR_ASSERTING_INT_CAST(int, writeLen);
		const int status = BIO_write(bufferedBio, buf, iwriteLen);

		if (status != iwriteLen)
		{
			WLog_ERR(TAG, "%s SOCKS proxy: failed to write AUTH METHOD request", logprefix);
			return FALSE;
		}
	}

	{
		BYTE buf[2] = { 0 };
		const int status = recv_socks_reply(context, bufferedBio, buf, sizeof(buf), "AUTH REQ", 5);

		if (status <= 0)
			return FALSE;

		switch (buf[1])
		{
			case AUTH_M_NO_AUTH:
				WLog_DBG(TAG, "%s (NO AUTH) method was selected", logprefix);
				break;

			case AUTH_M_USR_PASS:
				if (nauthMethods < 2)
				{
					WLog_ERR(TAG, "%s USER/PASS method was not proposed to server", logprefix);
					return FALSE;
				}
				if (!socks_proxy_userpass(context, bufferedBio, proxyUsername, proxyPassword))
					return FALSE;
				break;

			default:
				WLog_ERR(TAG, "%s unknown method 0x%x was selected by proxy", logprefix, buf[1]);
				return FALSE;
		}
	}
	/* CONN request */
	{
		BYTE buf[262] = { 0 };
		size_t offset = 0;
		buf[offset++] = 5;                 /* SOCKS version */
		buf[offset++] = SOCKS_CMD_CONNECT; /* command */
		buf[offset++] = 0;                 /* 3rd octet is reserved x00 */

		if (inet_pton(AF_INET6, hostname, &buf[offset + 1]) == 1)
		{
			buf[offset++] = SOCKS_ADDR_IPV6;
			offset += 16;
		}
		else if (inet_pton(AF_INET, hostname, &buf[offset + 1]) == 1)
		{
			buf[offset++] = SOCKS_ADDR_IPV4;
			offset += 4;
		}
		else
		{
			buf[offset++] = SOCKS_ADDR_FQDN;
			buf[offset++] = WINPR_ASSERTING_INT_CAST(uint8_t, hostnlen);
			memcpy(&buf[offset], hostname, hostnlen);
			offset += hostnlen;
		}

		if (offset > sizeof(buf) - 2)
			return FALSE;

		/* follows DST.PORT in netw. format */
		buf[offset++] = (port >> 8) & 0xff;
		buf[offset++] = port & 0xff;

		ERR_clear_error();
		const int ioffset = WINPR_ASSERTING_INT_CAST(int, offset);
		const int status = BIO_write(bufferedBio, buf, ioffset);

		if ((status < 0) || (status != ioffset))
		{
			WLog_ERR(TAG, "%s SOCKS proxy: failed to write CONN REQ", logprefix);
			return FALSE;
		}
	}

	BYTE buf[255] = { 0 };
	const int status = recv_socks_reply(context, bufferedBio, buf, sizeof(buf), "CONN REQ", 5);

	if (status < 4)
		return FALSE;

	if (buf[1] == 0)
	{
		WLog_INFO(TAG, "Successfully connected to %s:%" PRIu16, hostname, port);
		return TRUE;
	}

	if ((buf[1] > 0) && (buf[1] < 9))
		WLog_INFO(TAG, "SOCKS Proxy replied: %s", rplstat[buf[1]]);
	else
		WLog_INFO(TAG, "SOCKS Proxy replied: %" PRIu8 " status not listed in rfc1928", buf[1]);

	return FALSE;
}

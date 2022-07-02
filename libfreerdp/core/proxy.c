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

#include "proxy.h"
#include <freerdp/settings.h>
#include <freerdp/utils/proxy_utils.h>
#include <freerdp/crypto/crypto.h>
#include "tcp.h"

#include <winpr/assert.h>
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

static BOOL http_proxy_connect(BIO* bufferedBio, const char* proxyUsername,
                               const char* proxyPassword, const char* hostname, UINT16 port);
static BOOL socks_proxy_connect(BIO* bufferedBio, const char* proxyUsername,
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
	long long rc;

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
	uint32_t mask, amask, nmask;

	if (bits == 0)
		return TRUE;

	mask = htonl(0xFFFFFFFFu << (32 - bits));
	amask = addr->s_addr & mask;
	nmask = net->s_addr & mask;
	return amask == nmask;
}

static BOOL cidr6_match(const struct in6_addr* address, const struct in6_addr* network,
                        uint8_t bits)
{
	const uint32_t* a = (const uint32_t*)address;
	const uint32_t* n = (const uint32_t*)network;
	size_t bits_whole, bits_incomplete;
	bits_whole = bits >> 5;
	bits_incomplete = bits & 0x1F;

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

static BOOL check_no_proxy(rdpSettings* settings, const char* no_proxy)
{
	const char* delimiter = ",";
	BOOL result = FALSE;
	char* current;
	char* copy;
	char* context = NULL;
	size_t host_len;
	struct sockaddr_in sa4;
	struct sockaddr_in6 sa6;
	BOOL is_ipv4 = FALSE;
	BOOL is_ipv6 = FALSE;

	if (!no_proxy || !settings)
		return FALSE;

	if (inet_pton(AF_INET, settings->ServerHostname, &sa4.sin_addr) == 1)
		is_ipv4 = TRUE;
	else if (inet_pton(AF_INET6, settings->ServerHostname, &sa6.sin6_addr) == 1)
		is_ipv6 = TRUE;

	host_len = strlen(settings->ServerHostname);
	copy = _strdup(no_proxy);

	if (!copy)
		return FALSE;

	current = strtok_s(copy, delimiter, &context);

	while (current && !result)
	{
		const size_t currentlen = strlen(current);

		if (currentlen > 0)
		{
			WLog_DBG(TAG, "%s => %s (%" PRIdz ")", settings->ServerHostname, current, currentlen);

			/* detect left and right "*" wildcard */
			if (current[0] == '*')
			{
				if (host_len >= currentlen)
				{
					const size_t offset = host_len + 1 - currentlen;
					const char* name = settings->ServerHostname + offset;

					if (strncmp(current + 1, name, currentlen - 1) == 0)
						result = TRUE;
				}
			}
			else if (current[currentlen - 1] == '*')
			{
				if (strncmp(current, settings->ServerHostname, currentlen - 1) == 0)
					result = TRUE;
			}
			else if (current[0] ==
			         '.') /* Only compare if the no_proxy variable contains a whole domain. */
			{
				if (host_len > currentlen)
				{
					const size_t offset = host_len - currentlen;
					const char* name = settings->ServerHostname + offset;

					if (strncmp(current, name, currentlen) == 0)
						result = TRUE; /* right-aligned match for host names */
				}
			}
			else if (strcmp(current, settings->ServerHostname) == 0)
				result = TRUE; /* exact match */
			else if (is_ipv4 || is_ipv6)
			{
				char* rangedelim = strchr(current, '/');

				/* Check for IP ranges */
				if (rangedelim != NULL)
				{
					const char* range = rangedelim + 1;
					int sub;
					int rc = sscanf(range, "%u", &sub);

					if ((rc == 1) && (rc >= 0))
					{
						*rangedelim = '\0';

						if (is_ipv4)
						{
							struct sockaddr_in mask;

							if (inet_pton(AF_INET, current, &mask.sin_addr))
								result = cidr4_match(&sa4.sin_addr, &mask.sin_addr, sub);
						}
						else if (is_ipv6)
						{
							struct sockaddr_in6 mask;

							if (inet_pton(AF_INET6, current, &mask.sin6_addr))
								result = cidr6_match(&sa6.sin6_addr, &mask.sin6_addr, sub);
						}
					}
					else
						WLog_WARN(TAG, "NO_PROXY invalid entry %s", current);
				}
				else if (strncmp(current, settings->ServerHostname, currentlen) == 0)
					result = TRUE; /* left-aligned match for IPs */
			}
		}

		current = strtok_s(NULL, delimiter, &context);
	}

	free(copy);
	return result;
}

void proxy_read_environment(rdpSettings* settings, char* envname)
{
	DWORD envlen;
	char* env;
	envlen = GetEnvironmentVariableA(envname, NULL, 0);

	if (!envlen)
		return;

	env = calloc(1, envlen);

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
				freerdp_settings_set_uint32(settings, FreeRDP_ProxyType, PROXY_TYPE_NONE);
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
	UINT16 port;
	char* p;
	char* atPtr;
	char* uri_copy = _strdup(uri_in);
	char* uri = uri_copy;
	if (!uri)
		goto fail;

	p = strstr(uri, "://");

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

	/* uri is now [user:password@]hostname:port */
	atPtr = strrchr(uri, '@');

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

	p = strchr(uri, ':');

	if (p)
	{
		LONGLONG val;

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
			/* The default is 80. Also for Proxys. */
			port = 80;
		}
		else
		{
			port = 1080;
		}

		WLog_DBG(TAG, "setting default proxy port: %d", port);
	}

	if (!freerdp_settings_set_uint16(settings, FreeRDP_ProxyPort, port))
		goto fail;

	p = strchr(uri, '/');
	if (p)
		*p = '\0';
	if (!freerdp_settings_set_string(settings, FreeRDP_ProxyHostname, uri))
		goto fail;

	if (_stricmp("", uri) == 0)
	{
		WLog_ERR(TAG, "invalid syntax for proxy (hostname missing)");
		goto fail;
	}

	if (freerdp_settings_get_string(settings, FreeRDP_ProxyUsername))
	{
		WLog_INFO(TAG, "Parsed proxy configuration: %s://%s:%s@%s:%d", protocol,
		          freerdp_settings_get_string(settings, FreeRDP_ProxyUsername), "******",
		          freerdp_settings_get_string(settings, FreeRDP_ProxyHostname),
		          freerdp_settings_get_uint16(settings, FreeRDP_ProxyPort));
	}
	else
	{
		WLog_INFO(TAG, "Parsed proxy configuration: %s://%s:%d", protocol,
		          freerdp_settings_get_string(settings, FreeRDP_ProxyHostname),
		          freerdp_settings_get_uint16(settings, FreeRDP_ProxyPort));
	}
	rc = TRUE;

fail:
	free(uri_copy);
	return rc;
}

BOOL proxy_connect(rdpSettings* settings, BIO* bufferedBio, const char* proxyUsername,
                   const char* proxyPassword, const char* hostname, UINT16 port)
{
	switch (freerdp_settings_get_uint32(settings, FreeRDP_ProxyType))
	{
		case PROXY_TYPE_NONE:
		case PROXY_TYPE_IGNORE:
			return TRUE;

		case PROXY_TYPE_HTTP:
			return http_proxy_connect(bufferedBio, proxyUsername, proxyPassword, hostname, port);

		case PROXY_TYPE_SOCKS:
			return socks_proxy_connect(bufferedBio, proxyUsername, proxyPassword, hostname, port);

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

static BOOL http_proxy_connect(BIO* bufferedBio, const char* proxyUsername,
                               const char* proxyPassword, const char* hostname, UINT16 port)
{
	BOOL rc = FALSE;
	int status;
	wStream* s;
	char port_str[10] = { 0 };
	char recv_buf[256] = { 0 };
	char* eol = NULL;
	size_t resultsize = 0;
	size_t reserveSize;
	size_t portLen;
	size_t hostLen;
	const char connect[8] = "CONNECT";
	const char httpheader[17] = " HTTP/1.1" CRLF "Host: ";

	WINPR_ASSERT(bufferedBio);
	WINPR_ASSERT(hostname);

	_itoa_s(port, port_str, sizeof(port_str), 10);

	hostLen = strlen(hostname);
	portLen = strnlen(port_str, sizeof(port_str));
	reserveSize = ARRAYSIZE(connect) + (hostLen + 1 + portLen) * 2 + ARRAYSIZE(httpheader);
	s = Stream_New(NULL, reserveSize);
	if (!s)
		goto fail;

	Stream_Write(s, connect, ARRAYSIZE(connect));
	Stream_Write(s, hostname, hostLen);
	Stream_Write_UINT8(s, ':');
	Stream_Write(s, port_str, portLen);
	Stream_Write(s, httpheader, ARRAYSIZE(httpheader));
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
				char* base64;

				sprintf_s(creds, size, "%s:%s", proxyUsername, proxyPassword);
				base64 = crypto_base64_encode((const BYTE*)creds, size - 1);

				if (!base64 ||
				    !Stream_EnsureRemainingCapacity(s, ARRAYSIZE(basic) + strlen(base64)))
				{
					free(base64);
					free(creds);
					goto fail;
				}
				Stream_Write(s, basic, ARRAYSIZE(basic));
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
	status = BIO_write(bufferedBio, Stream_Buffer(s), Stream_GetPosition(s));

	if ((status < 0) || ((size_t)status != Stream_GetPosition(s)))
	{
		WLog_ERR(TAG, "HTTP proxy: failed to write CONNECT request");
		goto fail;
	}

	/* Read result until CR-LF-CR-LF.
	 * Keep recv_buf a null-terminated string. */
	while (strstr(recv_buf, CRLF CRLF) == NULL)
	{
		if (resultsize >= sizeof(recv_buf) - 1)
		{
			WLog_ERR(TAG, "HTTP Reply headers too long: %s", get_response_header(recv_buf));
			goto fail;
		}

		ERR_clear_error();
		status =
		    BIO_read(bufferedBio, (BYTE*)recv_buf + resultsize, sizeof(recv_buf) - resultsize - 1);

		if (status < 0)
		{
			/* Error? */
			if (BIO_should_retry(bufferedBio))
			{
				USleep(100);
				continue;
			}

			WLog_ERR(TAG, "Failed reading reply from HTTP proxy (Status %d)", status);
			goto fail;
		}
		else if (status == 0)
		{
			/* Error? */
			WLog_ERR(TAG, "Failed reading reply from HTTP proxy (BIO_read returned zero)");
			goto fail;
		}

		resultsize += status;
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

	if (strncmp(recv_buf, "HTTP/1.X 200", 12))
		goto fail;

	rc = TRUE;
fail:
	Stream_Free(s, TRUE);
	return rc;
}

static int recv_socks_reply(BIO* bufferedBio, BYTE* buf, int len, char* reason, BYTE checkVer)
{
	int status;

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
			if (BIO_should_retry(bufferedBio))
			{
				USleep(100);
				continue;
			}

			WLog_ERR(TAG, "Failed reading %s reply from SOCKS proxy (Status %d)", reason, status);
			return -1;
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

static BOOL socks_proxy_connect(BIO* bufferedBio, const char* proxyUsername,
                                const char* proxyPassword, const char* hostname, UINT16 port)
{
	int status;
	int nauthMethods = 1, writeLen = 3;
	BYTE buf[3 + 255 + 255]; /* biggest packet is user/pass auth */
	size_t hostnlen = strnlen(hostname, 255);

	if (proxyUsername && proxyPassword)
	{
		nauthMethods++;
		writeLen++;
	}

	/* select auth. method */
	buf[0] = 5;            /* SOCKS version */
	buf[1] = nauthMethods; /* #of methods offered */
	buf[2] = AUTH_M_NO_AUTH;

	if (nauthMethods > 1)
		buf[3] = AUTH_M_USR_PASS;

	ERR_clear_error();
	status = BIO_write(bufferedBio, buf, writeLen);

	if (status != writeLen)
	{
		WLog_ERR(TAG, "SOCKS proxy: failed to write AUTH METHOD request");
		return FALSE;
	}

	status = recv_socks_reply(bufferedBio, buf, 2, "AUTH REQ", 5);

	if (status <= 0)
		return FALSE;

	switch (buf[1])
	{
		case AUTH_M_NO_AUTH:
			WLog_DBG(TAG, "SOCKS Proxy: (NO AUTH) method was selected");
			break;

		case AUTH_M_USR_PASS:
			if (!proxyUsername || !proxyPassword)
				return FALSE;
			else
			{
				int usernameLen = strnlen(proxyUsername, 255);
				int userpassLen = strnlen(proxyPassword, 255);
				BYTE* ptr;

				if (nauthMethods < 2)
				{
					WLog_ERR(TAG, "SOCKS Proxy: USER/PASS method was not proposed to server");
					return FALSE;
				}

				/* user/password v1 method */
				ptr = buf + 2;
				buf[0] = 1;
				buf[1] = usernameLen;
				memcpy(ptr, proxyUsername, usernameLen);
				ptr += usernameLen;
				*ptr = userpassLen;
				ptr++;
				memcpy(ptr, proxyPassword, userpassLen);
				ERR_clear_error();
				status = BIO_write(bufferedBio, buf, 3 + usernameLen + userpassLen);

				if (status != 3 + usernameLen + userpassLen)
				{
					WLog_ERR(TAG, "SOCKS Proxy: error writing user/password request");
					return FALSE;
				}

				status = recv_socks_reply(bufferedBio, buf, 2, "AUTH REQ", 1);

				if (status < 2)
					return FALSE;

				if (buf[1] != 0x00)
				{
					WLog_ERR(TAG, "SOCKS Proxy: invalid user/password");
					return FALSE;
				}
			}

			break;

		default:
			WLog_ERR(TAG, "SOCKS Proxy: unknown method 0x%x was selected by proxy", buf[1]);
			return FALSE;
	}

	/* CONN request */
	buf[0] = 5;                 /* SOCKS version */
	buf[1] = SOCKS_CMD_CONNECT; /* command */
	buf[2] = 0;                 /* 3rd octet is reserved x00 */
	buf[3] = SOCKS_ADDR_FQDN;   /* addr.type */
	buf[4] = hostnlen;          /* DST.ADDR */
	memcpy(buf + 5, hostname, hostnlen);
	/* follows DST.PORT in netw. format */
	buf[hostnlen + 5] = (port >> 8) & 0xff;
	buf[hostnlen + 6] = port & 0xff;
	ERR_clear_error();
	status = BIO_write(bufferedBio, buf, hostnlen + 7U);

	if ((status < 0) || ((size_t)status != (hostnlen + 7U)))
	{
		WLog_ERR(TAG, "SOCKS proxy: failed to write CONN REQ");
		return FALSE;
	}

	status = recv_socks_reply(bufferedBio, buf, sizeof(buf), "CONN REQ", 5);

	if (status < 4)
		return FALSE;

	if (buf[1] == 0)
	{
		WLog_INFO(TAG, "Successfully connected to %s:%d", hostname, port);
		return TRUE;
	}

	if (buf[1] > 0 && buf[1] < 9)
		WLog_INFO(TAG, "SOCKS Proxy replied: %s", rplstat[buf[1]]);
	else
		WLog_INFO(TAG, "SOCKS Proxy replied: %d status not listed in rfc1928", buf[1]);

	return FALSE;
}

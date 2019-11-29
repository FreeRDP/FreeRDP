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
#include "freerdp/settings.h"
#include "tcp.h"

#include "winpr/environment.h" /* For GetEnvironmentVariableA */

#define CRLF "\r\n"
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

static BOOL http_proxy_connect(BIO* bufferedBio, const char* hostname, UINT16 port);
static BOOL socks_proxy_connect(BIO* bufferedBio, const char* proxyUsername,
                                const char* proxyPassword, const char* hostname, UINT16 port);
static DWORD proxy_read_environment(const char* envname, char** pProxyHostname, UINT16* pPort,
                                    char** pProxyUsername, char** pProxyPassword);
static BOOL check_no_proxy(const char* ServerHostname, char* no_proxy);

static char* get_from_env(const char* var)
{
	char* buffer;
	DWORD s, size = GetEnvironmentVariableA(var, NULL, 0);
	if (size == 0)
		return NULL;
	buffer = calloc(size, sizeof(char));
	if (!buffer)
		return NULL;
	s = GetEnvironmentVariableA(var, buffer, size);
	if (s != size - 1)
	{
		free(buffer);
		return NULL;
	}
	return buffer;
}

static BOOL proxy_no_proxy(const char* ServerHostname)
{
	BOOL rc = FALSE;

	const char* const var[] = { "NO_PROXY", "no_proxy", NULL };
	const char* const* next = var;

	while ((*next != NULL) && !rc)
	{
		const char* cur = *next++;
		char* buffer = get_from_env(cur);
		if (!buffer)
			continue;

		if (check_no_proxy(ServerHostname, buffer))
		{
			WLog_INFO(TAG, "deactivating proxy: %s [%s=%s]", ServerHostname, cur, buffer);
			rc = TRUE;
		}
		free(buffer);
	}

	return rc;
}

BOOL proxy_prepare(const rdpSettings* settings, DWORD* pProxyType, char** lpPeerHostname,
                   UINT16* lpPeerPort, char** lpProxyUsername, char** lpProxyPassword)
{
	size_t i = 0;
	const char* proxy_types[] = { "HTTPS_PROXY", NULL };
	char* ProxyHostname = NULL;
	char* ProxyUsername = NULL;
	char* ProxyPassword = NULL;
	UINT16 ProxyPort = 0;
	DWORD ProxyType = settings->ProxyType;
	const char* ServerHostname = settings->ServerHostname;
	if (ProxyType == PROXY_TYPE_IGNORE)
		return FALSE;

	/* User provided settings, use them and ignore the environment. */
	if (ProxyType != PROXY_TYPE_NONE)
	{
		if (lpPeerHostname)
			*lpPeerHostname = _strdup(settings->ProxyHostname);
		if (lpPeerPort)
			*lpPeerPort = settings->ProxyPort;
		if (lpProxyUsername)
			*lpProxyUsername = _strdup(settings->ProxyUsername);
		if (lpProxyPassword)
			*lpProxyPassword = _strdup(settings->ProxyPassword);
		if (pProxyType)
			*pProxyType = ProxyType;
		return TRUE;
	}

	/* First check if our host is part of the no_proxy settings */
	if (proxy_no_proxy(ServerHostname))
		return FALSE;

	while ((ProxyType == PROXY_TYPE_NONE) && proxy_types[i])
		ProxyType = proxy_read_environment(proxy_types[i++], &ProxyHostname, &ProxyPort,
		                                   &ProxyUsername, &ProxyPassword);

	if (pProxyType)
		*pProxyType = ProxyType;
	if (ProxyType != PROXY_TYPE_NONE)
	{
		if (lpPeerHostname)
			*lpPeerHostname = ProxyHostname;
		if (lpPeerPort)
			*lpPeerPort = ProxyPort;
		if (lpProxyUsername)
			*lpProxyUsername = ProxyUsername;
		if (lpProxyPassword)
			*lpProxyPassword = ProxyPassword;
		return TRUE;
	}

	return FALSE;
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

BOOL check_no_proxy(const char* ServerHostname, char* no_proxy)
{
	const char* delimiter = ",";
	BOOL result = FALSE;
	char* current;
	size_t host_len;
	struct sockaddr_in sa4;
	struct sockaddr_in6 sa6;
	BOOL is_ipv4 = FALSE;
	BOOL is_ipv6 = FALSE;

	if (!no_proxy || !ServerHostname)
		return FALSE;

	if (inet_pton(AF_INET, ServerHostname, &sa4.sin_addr) == 1)
		is_ipv4 = TRUE;
	else if (inet_pton(AF_INET6, ServerHostname, &sa6.sin6_addr) == 1)
		is_ipv6 = TRUE;

	host_len = strnlen(ServerHostname, MAX_PATH);

	current = strtok(no_proxy, delimiter);

	while (current && !result)
	{
		const size_t currentlen = strlen(current);

		if (currentlen > 0)
		{
			WLog_DBG(TAG, "%s => %s (%" PRIdz ")", ServerHostname, current, currentlen);

			/* detect left and right "*" wildcard */
			if (current[0] == '*')
			{
				if (host_len >= currentlen)
				{
					const size_t offset = host_len + 1 - currentlen;
					const char* name = ServerHostname + offset;

					if (strncmp(current + 1, name, currentlen - 1) == 0)
						result = TRUE;
				}
			}
			else if (current[currentlen - 1] == '*')
			{
				if (strncmp(current, ServerHostname, currentlen - 1) == 0)
					result = TRUE;
			}
			else if (current[0] ==
			         '.') /* Only compare if the no_proxy variable contains a whole domain. */
			{
				if (host_len > currentlen)
				{
					const size_t offset = host_len - currentlen;
					const char* name = ServerHostname + offset;

					if (strncmp(current, name, currentlen) == 0)
						result = TRUE; /* right-aligned match for host names */
				}
			}
			else if (strncmp(current, ServerHostname, 255) == 0)
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
				else if (strncmp(current, ServerHostname, currentlen) == 0)
					result = TRUE; /* left-aligned match for IPs */
			}
		}

		current = strtok(NULL, delimiter);
	}

	return result;
}

DWORD proxy_read_environment(const char* envname, char** pProxyHostname, UINT16* pPort,
                             char** pProxyUsername, char** pProxyPassword)
{
	const char* const* next;
	char* ProxyHostname = NULL;
	char* ProxyUsername = NULL;
	char* ProxyPassword = NULL;
	const char* const tries[] = { envname, NULL };
	UINT16 ProxyPort = 0;
	DWORD ProxyType = PROXY_TYPE_NONE;

	next = tries;
	while (*next && (ProxyType == PROXY_TYPE_NONE))
	{
		BOOL rc;
		const char* cur = *next++;
		char* buffer = get_from_env(cur);

		if (!buffer)
			continue;

		rc = freerdp_settings_proxy_parse_uri(buffer, &ProxyType, &ProxyUsername, &ProxyPort,
		                                      &ProxyUsername, &ProxyPassword);
		free(buffer);
		if (!rc)
			continue;
	}

	if (ProxyType != PROXY_TYPE_NONE)
	{
		if (pPort)
			*pPort = ProxyPort;

		if (pProxyHostname)
			*pProxyHostname = ProxyHostname;
		else
			free(ProxyHostname);
		if (pProxyUsername)
			*pProxyUsername = ProxyUsername;
		else
			free(ProxyUsername);
		if (pProxyPassword)
			*pProxyPassword = ProxyPassword;
		else
			free(ProxyPassword);
	}

	return ProxyType;
}

BOOL freerdp_settings_proxy_parse_uri(const char* uri, DWORD* pProxyType, char** pProxyHostname,
                                      UINT16* pPort, char** pProxyUsername, char** pProxyPassword)
{
	const char *hostname, *pport;
	const char* protocol;
	const char* p;
	char* ProxyHostname = NULL;
	char* ProxyUsername = NULL;
	char* ProxyPassword = NULL;
	UINT16 ProxyPort;
	DWORD ProxyType = PROXY_TYPE_NONE;
	size_t hostnamelen;

	/* http://user:password@hostname:port */
	p = strstr(uri, "://");

	if (p)
	{
		if (strncmp("http://", uri, 7) == 0)
		{
			ProxyType = PROXY_TYPE_HTTP;
			protocol = "http";
		}
		else if (strncmp("socks5://", uri, 9) == 0)
		{
			ProxyType = PROXY_TYPE_SOCKS;
			protocol = "socks5";
		}
		else
		{
			WLog_ERR(TAG, "Only HTTP and SOCKS5 proxies supported by now");
			return FALSE;
		}

		uri = p + 3;
	}
	else
	{
		WLog_ERR(TAG, "No scheme in proxy URI");
		return FALSE;
	}

	/* user:password@hostname:port */
	p = strchr(uri, '@');
	if (p)
	{
		size_t userLen = 0, pwdLen = 0;
		const char* q = NULL;

		hostname = p + 1;
		q = strchr(uri, ':');
		/* check for first ':' in user:password, ignore if found after '@' */
		if (q && (q < p))
		{
			userLen = (size_t)(q - uri);
			pwdLen = (size_t)(p - q - 1);
		}
		else
			userLen = (size_t)(p - uri);

		if (userLen > 0)
		{
			ProxyUsername = calloc(userLen + 1, sizeof(char));
			strncpy(ProxyUsername, uri, userLen);
		}
		if (pwdLen > 0)
		{
			ProxyPassword = calloc(pwdLen + 1, sizeof(char));
			strncpy(ProxyPassword, q + 1, pwdLen);
		}
	}
	else
		hostname = uri;
	pport = strchr(hostname, ':');

	if (pport)
	{
		long val;
		errno = 0;
		val = strtol(pport + 1, NULL, 0);

		if ((errno != 0) || (val <= 0) || (val > UINT16_MAX))
			return FALSE;

		ProxyPort = (UINT16)val;
	}
	else
	{
		/* The default is 80. Also for Proxys. */
		ProxyPort = 80;
		pport = strchr(hostname, '/');
	}

	if (pport)
	{
		hostnamelen = pport - hostname;
	}
	else
	{
		hostnamelen = strlen(hostname);
	}

	ProxyHostname = calloc(hostnamelen + 1, 1);

	if (!ProxyHostname)
	{
		WLog_ERR(TAG, "Not enough memory");
		return FALSE;
	}

	memcpy(ProxyHostname, hostname, hostnamelen);

	WLog_INFO(TAG, "Parsed proxy configuration: %s://%s:%d", protocol, ProxyHostname, ProxyPort);
	if (pPort)
		*pPort = ProxyPort;
	if (pProxyHostname)
		*pProxyHostname = ProxyHostname;
	else
		free(ProxyHostname);
	if (pProxyUsername)
		*pProxyUsername = ProxyUsername;
	else
		free(ProxyUsername);
	if (pProxyPassword)
		*pProxyPassword = ProxyPassword;
	else
		free(ProxyPassword);
	if (pProxyType)
		*pProxyType = ProxyType;
	return TRUE;
}

BOOL proxy_connect(DWORD ProxyType, BIO* bufferedBio, const char* proxyUsername,
                   const char* proxyPassword, const char* hostname, UINT16 port)
{
	switch (ProxyType)
	{
		case PROXY_TYPE_NONE:
		case PROXY_TYPE_IGNORE:
			return TRUE;

		case PROXY_TYPE_HTTP:
			return http_proxy_connect(bufferedBio, hostname, port);

		case PROXY_TYPE_SOCKS:
			return socks_proxy_connect(bufferedBio, proxyUsername, proxyPassword, hostname, port);

		default:
			WLog_ERR(TAG, "Invalid internal proxy configuration");
			return FALSE;
	}
}

static BOOL http_proxy_connect(BIO* bufferedBio, const char* hostname, UINT16 port)
{
	size_t pos;
	int status;
	wStream* s;
	char port_str[10], recv_buf[256] = { 0 }, *eol;
	size_t resultsize;
	/* URL hostname length limit is 255, don't forget port... */
	size_t hostLen = strnlen(hostname, MAX_PATH);
	size_t portLen;
	if (_itoa_s(port, port_str, sizeof(port_str), 10) != 0)
		return FALSE;
	portLen = strnlen(port_str, sizeof(port_str));

	s = Stream_New(NULL, 200 + 2 * hostLen + portLen);
	Stream_Write(s, "CONNECT ", 8);
	Stream_Write(s, hostname, hostLen);
	Stream_Write_UINT8(s, ':');
	Stream_Write(s, port_str, portLen);
	Stream_Write(s, " HTTP/1.1" CRLF "Host: ", 17);
	Stream_Write(s, hostname, hostLen);
	Stream_Write_UINT8(s, ':');
	Stream_Write(s, port_str, portLen);
	Stream_Write(s, CRLF CRLF, 4);
	pos = Stream_GetPosition(s);
	status = BIO_write(bufferedBio, Stream_Buffer(s), (int)pos);
	Stream_Free(s, TRUE);

	if ((status < 0) || ((size_t)status != pos))
	{
		WLog_ERR(TAG, "HTTP proxy: failed to write CONNECT request");
		return FALSE;
	}

	/* Read result until CR-LF-CR-LF.
	 * Keep recv_buf a null-terminated string. */
	resultsize = 0;

	while (strstr(recv_buf, CRLF CRLF) == NULL)
	{
		if (resultsize >= sizeof(recv_buf) - 1)
		{
			WLog_ERR(TAG, "HTTP Reply headers too long.");
			return FALSE;
		}

		status = BIO_read(bufferedBio, (BYTE*)recv_buf + resultsize,
		                  (int)(sizeof(recv_buf) - resultsize - 1));

		if (status < 0)
		{
			/* Error? */
			if (BIO_should_retry(bufferedBio))
			{
				USleep(100);
				continue;
			}

			WLog_ERR(TAG, "Failed reading reply from HTTP proxy (Status %d)", status);
			return FALSE;
		}
		else if (status == 0)
		{
			/* Error? */
			WLog_ERR(TAG, "Failed reading reply from HTTP proxy (BIO_read returned zero)");
			return FALSE;
		}

		resultsize += (size_t)status;
	}

	/* Extract HTTP status line */
	eol = strchr(recv_buf, '\r');

	if (!eol)
	{
		/* should never happen */
		return FALSE;
	}

	*eol = '\0';
	WLog_INFO(TAG, "HTTP Proxy: %s", recv_buf);

	if (strnlen(recv_buf, sizeof(recv_buf)) < 12)
	{
		return FALSE;
	}

	recv_buf[7] = 'X';

	if (strncmp(recv_buf, "HTTP/1.X 200", 12))
		return FALSE;

	return TRUE;
}

static int recv_socks_reply(BIO* bufferedBio, BYTE* buf, int len, char* reason, BYTE checkVer)
{
	int status;

	for (;;)
	{
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
	BYTE nauthMethods = 1;
	int writeLen = 3;
	BYTE buf[3 + 255 + 255]; /* biggest packet is user/pass auth */
	BYTE hostnlen = (BYTE)strnlen(hostname, 255);

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
				BYTE usernameLen = (BYTE)strnlen(proxyUsername, 255);
				BYTE userpassLen = (BYTE)strnlen(proxyPassword, 255);
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

BOOL proxy_resolve(rdpContext* context, DWORD ProxyType, const char* ProxyHostname,
                   UINT16 ProxyPort, const char* ProxyUsername, const char* ProxyPassword,
                   const char* hostname, UINT16 port)
{
	UINT32 p = port;
	BIO* bio = proxy_multi_connect(context, ProxyType, ProxyHostname, ProxyPort, ProxyUsername,
	                               ProxyPassword, &hostname, &p, 1, 5);
	if (bio)
		BIO_free_all(bio);
	return bio != NULL;
}

BIO* proxy_multi_connect(rdpContext* context, DWORD ProxyType, const char* ProxyHost,
                         UINT16 ProxyPort, const char* ProxyUsername, const char* ProxyPassword,
                         char const* const* hostnames, const UINT32* ports, size_t count,
                         int timeout)
{
	size_t x;
	int sockfd;
	BIO* bufferedBio;

	for (x = 0; x < count; x++)
	{
		const char* host = hostnames[x];
		const UINT16 port = (UINT16)ports[x];

		sockfd =
		    freerdp_tcp_connect(context, context->settings, ProxyHost, ProxyPort, timeout, TRUE);
		if (sockfd < 0)
			continue;
		bufferedBio = freerdp_tcp_to_buffered_bio(sockfd, FALSE);
		if (!bufferedBio)
			continue;
		if (!proxy_connect(ProxyType, bufferedBio, ProxyUsername, ProxyPassword, host, port))
		{
			BIO_free_all(bufferedBio);
			continue;
		}
		return bufferedBio;
	}

	return NULL;
}

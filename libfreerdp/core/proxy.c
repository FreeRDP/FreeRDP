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

#include "winpr/environment.h"	/* For GetEnvironmentVariableA */

#define CRLF "\r\n"
#define TAG FREERDP_TAG("core.proxy")

BOOL http_proxy_connect(BIO* bufferedBio, const char* hostname, UINT16 port);
void proxy_read_environment(rdpSettings* settings, char* envname);

BOOL proxy_prepare(rdpSettings* settings, const char** lpPeerHostname, UINT16* lpPeerPort,
                   BOOL isHTTPS)
{
	/* For TSGateway, find the system HTTPS proxy automatically */
	if (!settings->ProxyType)
		proxy_read_environment(settings, "https_proxy");

	if (!settings->ProxyType)
		proxy_read_environment(settings, "HTTPS_PROXY");

	if (settings->ProxyType)
	{
		*lpPeerHostname = settings->ProxyHostname;
		*lpPeerPort = settings->ProxyPort;
		return TRUE;
	}

	return FALSE;
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
		proxy_parse_uri(settings, env);

	free(env);
}

BOOL proxy_parse_uri(rdpSettings* settings, const char* uri)
{
	const char* hostname, *pport;
	const char* protocol;
	const char* p;
	UINT16 port;
	int hostnamelen;
	p = strstr(uri, "://");

	if (p)
	{
		if (p == uri + 4 && !strncmp("http", uri, 4))
		{
			settings->ProxyType = PROXY_TYPE_HTTP;
			protocol = "http";
		}
		else
		{
			WLog_ERR(TAG, "Only HTTP proxys supported by now");
			return FALSE;
		}

		uri = p + 3;
	}
	else
	{
		WLog_ERR(TAG, "No scheme in proxy URI");
		return FALSE;
	}

	hostname = uri;
	pport = strchr(hostname, ':');

	if (pport)
	{
		long val;
		errno = 0;
		val = strtol(pport + 1, NULL, 0);

		if ((errno != 0) || (val <= 0) || (val > UINT16_MAX))
			return FALSE;

		port = val;
	}
	else
	{
		/* The default is 80. Also for Proxys. */
		port = 80;
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

	settings->ProxyHostname = calloc(hostnamelen + 1, 1);

	if (!settings->ProxyHostname)
	{
		WLog_ERR(TAG, "Not enough memory");
		return FALSE;
	}

	memcpy(settings->ProxyHostname, hostname, hostnamelen);
	settings->ProxyPort = port;
	WLog_INFO(TAG, "Parsed proxy configuration: %s://%s:%d", protocol, settings->ProxyHostname,
	          settings->ProxyPort);
	return TRUE;
}

BOOL proxy_connect(rdpSettings* settings, BIO* bufferedBio, const char* hostname, UINT16 port)
{
	switch (settings->ProxyType)
	{
		case PROXY_TYPE_NONE:
			return TRUE;

		case PROXY_TYPE_HTTP:
			return http_proxy_connect(bufferedBio, hostname, port);

		default:
			WLog_ERR(TAG, "Invalid internal proxy configuration");
			return FALSE;
	}
}

BOOL http_proxy_connect(BIO* bufferedBio, const char* hostname, UINT16 port)
{
	int status;
	wStream* s;
	char port_str[10], recv_buf[256], *eol;
	int resultsize;
	_itoa_s(port, port_str, sizeof(port_str), 10);
	s = Stream_New(NULL, 200);
	Stream_Write(s, "CONNECT ", 8);
	Stream_Write(s, hostname, strlen(hostname));
	Stream_Write_UINT8(s, ':');
	Stream_Write(s, port_str, strlen(port_str));
	Stream_Write(s, " HTTP/1.1" CRLF "Host: ", 17);
	Stream_Write(s, hostname, strlen(hostname));
	Stream_Write_UINT8(s, ':');
	Stream_Write(s, port_str, strlen(port_str));
	Stream_Write(s, CRLF CRLF, 4);
	status = BIO_write(bufferedBio, Stream_Buffer(s), Stream_GetPosition(s));

	if (status != Stream_GetPosition(s))
	{
		WLog_ERR(TAG, "HTTP proxy: failed to write CONNECT request");
		return FALSE;
	}

	Stream_Free(s, TRUE);
	s = NULL;
	/* Read result until CR-LF-CR-LF.
	 * Keep recv_buf a null-terminated string. */
	memset(recv_buf, '\0', sizeof(recv_buf));
	resultsize = 0;

	while (strstr(recv_buf, CRLF CRLF) == NULL)
	{
		if (resultsize >= sizeof(recv_buf) - 1)
		{
			WLog_ERR(TAG, "HTTP Reply headers too long.");
			return FALSE;
		}

		status = BIO_read(bufferedBio, (BYTE*)recv_buf + resultsize, sizeof(recv_buf) - resultsize - 1);

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

		resultsize += status;
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

	if (strlen(recv_buf) < 12)
	{
		return FALSE;
	}

	recv_buf[7] = 'X';

	if (strncmp(recv_buf, "HTTP/1.X 200", 12))
		return FALSE;

	return TRUE;
}


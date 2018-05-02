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

static BOOL http_proxy_connect(BIO* bufferedBio, const char* hostname, UINT16 port);
static BOOL socks_proxy_connect(BIO* bufferedBio, const char* hostname, UINT16 port);
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

		case PROXY_TYPE_SOCKS:
			return socks_proxy_connect(bufferedBio, hostname, port);

		default:
			WLog_ERR(TAG, "Invalid internal proxy configuration");
			return FALSE;
	}
}

static BOOL http_proxy_connect(BIO* bufferedBio, const char* hostname, UINT16 port)
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

static int recv_socks_reply(BIO* bufferedBio, BYTE * buf, int len, char* reason)
{
	int status;

	ZeroMemory(buf, len);
	for(;;) {
	  status = BIO_read(bufferedBio, buf, len);
	  if (status > 0) break;
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
	  else if (status == 0)
	  {
	    /* Error? */
	    WLog_ERR(TAG, "Failed reading %s reply from SOCKS proxy (BIO_read returned zero)", reason);
	    return -1;
	  }
	}
	
	if (buf[0] != 5)
	{
	  WLog_ERR(TAG, "SOCKS Proxy version is not 5 (%s)", reason);
	  return -1;
	}

	return status;
}

/* SOCKS Proxy auth methods by rfc1928 */
#define AUTH_M_NO_AUTH   0
#define AUTH_M_GSSAPI    1
#define AUTH_M_USR_PASS  2

static BOOL socks_proxy_connect(BIO* bufferedBio, const char* hostname, UINT16 port)
{
	int status;
	BYTE buf[280], hostnlen = strlen(hostname) & 0xff;
	/* CONN REQ replies in enum. order */
	static const char *rplstat[] = {
	  "succeeded",
	  "general SOCKS server failure",
	  "connection not allowed by ruleset",
	  "Network unreachable",
	  "Host unreachable",
	  "Connection refused",
	  "TTL expired",
	  "Command not supported",
	  "Address type not supported"
	};

	/* select auth. method */
	memset(buf, '\0', sizeof(buf));
	buf[0] = 5; /* SOCKS version */
	buf[1] = 1; /* #of methods offered */
	buf[2] = AUTH_M_NO_AUTH;
	status = BIO_write(bufferedBio, buf, 3);
	if (status != 3)
	{
		WLog_ERR(TAG, "SOCKS proxy: failed to write AUTH METHOD request");
		return FALSE;
	}

	status = recv_socks_reply(bufferedBio, buf, sizeof(buf), "AUTH REQ");
	if (status < 0) return FALSE;

	if (buf[1] != AUTH_M_NO_AUTH)
	{
	  WLog_ERR(TAG, "SOCKS Proxy: (NO AUTH) method was not selected by proxy");
	  return FALSE;
	}

	/* CONN request */
	memset(buf, '\0', sizeof(buf));
	buf[0] = 5; /* SOCKS version */
	buf[1] = 1; /* command = connect */
	/* 3rd octet is reserved x00 */
	buf[3] = 3; /* addr.type = FQDN */
	buf[4] = hostnlen; /* DST.ADDR */
	memcpy(buf +5, hostname, hostnlen);
	/* follows DST.PORT in netw. format */
	buf[hostnlen +5] = 0xff & (port >> 8);
	buf[hostnlen +6] = 0xff & port;
	status = BIO_write(bufferedBio, buf, hostnlen +7);
	if (status != (hostnlen +7))
	{
		WLog_ERR(TAG, "SOCKS proxy: failed to write CONN REQ");
		return FALSE;
	}

	status = recv_socks_reply(bufferedBio, buf, sizeof(buf), "CONN REQ");
	if (status < 0) return FALSE;

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

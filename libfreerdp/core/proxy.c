/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * HTTP Proxy support
 *
 * Copyright 2014 Christian Plattner <ccpp@gmx.at>
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


#include "proxy.h"
#include "freerdp/settings.h"
#include "tcp.h"

#include "winpr/environment.h"
/* For GetEnvironmentVariableA */

void http_proxy_read_environment(rdpSettings *settings, char *envname)
{
	char env[256];
	DWORD envlen;
	char *hostname, *pport;

	envlen = GetEnvironmentVariableA(envname, env, sizeof(env));
	if(!envlen)
		return;

	if (strncmp(env, "http://", 7)) {
		fprintf(stderr, "Proxy url must have scheme http. Ignoring.\n");
		return;
	}

	settings->HTTPProxyEnabled = TRUE;

	hostname = env + 7;
	pport = strchr(hostname, ':');
	if (pport) {
		*pport = '\0';
		settings->HTTPProxyPort = atoi(pport+1);
	}
	else {
		/* The default is 80. Also for Proxys. */
		settings->HTTPProxyPort = 80;

		pport = strchr(hostname, '/');
		if(pport)
			*pport = '\0';
	}

	freerdp_set_param_string(settings, FreeRDP_HTTPProxyHostname, hostname);
}

BOOL http_proxy_connect(BIO* bio, const char* hostname, UINT16 port)
{
	int status;
	wStream* s;
	char str[256], *eol;
	int resultsize;
	int send_length;

	_itoa_s(port, str, sizeof(str), 10);

	s = Stream_New(NULL, 200);
	Stream_Write(s, "CONNECT ", 8);
	Stream_Write(s, hostname, strlen(hostname));
	Stream_Write_UINT8(s, ':');
	Stream_Write(s, str, strlen(str));
	Stream_Write(s, " HTTP/1.1\r\n\r\nHost: ", 19);
	Stream_Write(s, hostname, strlen(hostname));
	Stream_Write_UINT8(s, ':');
	Stream_Write(s, str, strlen(str));
	Stream_Write(s, "\r\n\r\n", 4);

	send_length = Stream_GetPosition(s);
	Stream_SetPosition(s, 0);
	while (send_length > 0) {
		status = BIO_write(bio, Stream_Pointer(s), send_length);
		if (status < 0) {
			fprintf(stderr, "HTTP Proxy connection: error while writing: %d\n", status);
			return FALSE;
		}
		if (status == 0) {
			fprintf(stderr, "HTTP Proxy blocking?\n");
			return FALSE;
		}
		fprintf(stderr, "HTTP Proxy: sent %d bytes\n", status);
		send_length -= status;
	}

	Stream_Free(s, TRUE);
	s = NULL;

	/* Read result until CR-LF-CR-LF.
	 * Keep str a null-terminated string. */

	memset(str, '\0', sizeof(str));
	resultsize = 0;
	while ( strstr(str, "\r\n\r\n") == NULL ) {
		if (resultsize >= sizeof(str)-1) {
			fprintf(stderr, "HTTP Reply headers too long.\n");
			return FALSE;
		}

		status = BIO_read(bio, (BYTE*)str + resultsize, sizeof(str)-resultsize-1);
		if (status < 0) {
			/* Error? */
			return FALSE;
		}
		else if (status == 0) {
			/* Error? */
			fprintf(stderr, "BIO_read() returned zero\n");
			return FALSE;
		}
		fprintf(stderr, "HTTP Proxy: received %d bytes\n", status);
		resultsize += status;
	}

	/* Extract HTTP status line */
	eol = strchr(str, '\r');
	if (!eol) {
		/* should never happen */
		return FALSE;
	}

	*eol = '\0';

	fprintf(stderr, "HTTP proxy: %s\n", str);

	if (strlen(str) < 12) {
		return FALSE;
	}

	str[7] = 'X';
	if (strncmp(str, "HTTP/1.X 200", 12))
		return FALSE;
	
	return TRUE;
}


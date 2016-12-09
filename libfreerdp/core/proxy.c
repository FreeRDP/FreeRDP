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


#include "proxy.h"
#include "freerdp/settings.h"
#include "tcp.h"

#include "winpr/environment.h"	/* For GetEnvironmentVariableA */

#define CRLF "\r\n"

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

	if (status != Stream_GetPosition(s)) {
		fprintf(stderr, "HTTP proxy: failed to write CONNECT request\n");
		return FALSE;
	}

	Stream_Free(s, TRUE);
	s = NULL;

	/* Read result until CR-LF-CR-LF.
	 * Keep recv_buf a null-terminated string. */

	memset(recv_buf, '\0', sizeof(recv_buf));
	resultsize = 0;
	while ( strstr(recv_buf, CRLF CRLF) == NULL ) {
		if (resultsize >= sizeof(recv_buf)-1) {
			fprintf(stderr, "HTTP Reply headers too long.\n");
			return FALSE;
		}

		status = BIO_read(bufferedBio, (BYTE*)recv_buf + resultsize, sizeof(recv_buf)-resultsize-1);
		if (status < 0) {
			/* Error? */
			if (BIO_should_retry(bufferedBio)) {
				USleep(100);
				continue;
			}
			return FALSE;
		}
		else if (status == 0) {
			/* Error? */
			fprintf(stderr, "BIO_read() returned zero\n");
			return FALSE;
		}
		resultsize += status;
	}

	/* Extract HTTP status line */
	eol = strchr(recv_buf, '\r');
	if (!eol) {
		/* should never happen */
		return FALSE;
	}

	*eol = '\0';

	fprintf(stderr, "HTTP proxy: %s\n", recv_buf);

	if (strlen(recv_buf) < 12) {
		return FALSE;
	}

	recv_buf[7] = 'X';
	if (strncmp(recv_buf, "HTTP/1.X 200", 12))
		return FALSE;
	
	return TRUE;
}


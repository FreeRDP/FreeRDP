/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * HTTP Proxy support
 *
 * Copyright 2015 Christian Plattner <ccpp@gmx.at>
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

/* Probably this is more portable */
#define TRIO_REPLACE_STDIO

#include "proxy.h"
#include "freerdp/settings.h"
#include "tcp.h"

#include "winpr/libwinpr/utils/trio/trio.h"	/* asprintf */
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
	char *send_buf, recv_buf[512], *eol;
	int resultsize;
	int send_length;

	send_length = trio_asprintf(&send_buf,
			"CONNECT %s:%d HTTP/1.1" CRLF
			"Host: %s:%d" CRLF CRLF,
			hostname, port,
			hostname, port);

	if (send_length <= 0) {
		fprintf(stderr, "HTTP proxy: failed to allocate memory\n");
		return FALSE;
	}

	status = BIO_write(bufferedBio, send_buf, send_length);
	free(send_buf);
	send_buf = NULL;

	if (status != send_length) {
		fprintf(stderr, "HTTP proxy: failed to write CONNECT request\n");
		return FALSE;
	}

	/* Read result until CR-LF-CR-LF.
	 * Keep recv_buf a null-terminated string. */

	memset(recv_buf, '\0', sizeof(recv_buf));
	resultsize = 0;
	while ( strstr(recv_buf, "\r\n\r\n") == NULL ) {
		if (resultsize >= sizeof(recv_buf)-1) {
			fprintf(stderr, "HTTP Reply headers too long.\n");
			return FALSE;
		}

		status = BIO_read(bufferedBio, (BYTE*)recv_buf + resultsize, sizeof(recv_buf)-resultsize-1);
		if (status < 0) {
			/* Error? */
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


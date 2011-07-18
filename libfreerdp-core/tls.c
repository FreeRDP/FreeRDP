/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Transport Layer Security
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>

#include "tls.h"

boolean tls_connect(rdpTls* tls)
{
	int connection_status;

	tls->ssl = SSL_new(tls->ctx);

	if (tls->ssl == NULL)
	{
		printf("SSL_new failed\n");
		return False;
	}

	if (SSL_set_fd(tls->ssl, tls->sockfd) < 1)
	{
		printf("SSL_set_fd failed\n");
		return False;
	}

	while (1)
	{
		connection_status = SSL_connect(tls->ssl);

		/*
		 * SSL_WANT_READ and SSL_WANT_WRITE errors are normal,
		 * just try again if it happens
		 */

		if (connection_status == SSL_ERROR_WANT_READ)
			continue;
		else if (connection_status == SSL_ERROR_WANT_WRITE)
			continue;
		else
			break;
	}

	if (connection_status < 0)
	{
		if (tls_print_error("SSL_connect", tls->ssl, connection_status))
			return False;
	}

	printf("TLS connection established\n");

	return True;
}

boolean tls_disconnect(rdpTls* tls)
{
	return True;
}

int tls_read(rdpTls* tls, uint8* data, int length)
{
	int status;

	while (True)
	{
		status = SSL_read(tls->ssl, data, length);

		switch (SSL_get_error(tls->ssl, status))
		{
			case SSL_ERROR_NONE:
				return status;
				break;

			case SSL_ERROR_WANT_READ:
				nanosleep(&tls->ts, NULL);
				break;

			default:
				tls_print_error("SSL_read", tls->ssl, status);
				return -1;
				break;
		}
	}

	return 0;
}

int tls_write(rdpTls* tls, uint8* data, int length)
{
	int status;
	int sent = 0;

	while (sent < length)
	{
		status = SSL_write(tls->ssl, data, length);

		switch (SSL_get_error(tls->ssl, status))
		{
			case SSL_ERROR_NONE:
				sent += status;
				data += status;
				break;

			case SSL_ERROR_WANT_WRITE:
				nanosleep(&tls->ts, NULL);
				break;

			default:
				tls_print_error("SSL_write", tls->ssl, status);
				return -1;
				break;
		}
	}

	return sent;
}

boolean tls_print_error(char *func, SSL *connection, int value)
{
	switch (SSL_get_error(connection, value))
	{
		case SSL_ERROR_ZERO_RETURN:
			printf("%s: Server closed TLS connection\n", func);
			return True;

		case SSL_ERROR_WANT_READ:
			printf("SSL_ERROR_WANT_READ\n");
			return False;

		case SSL_ERROR_WANT_WRITE:
			printf("SSL_ERROR_WANT_WRITE\n");
			return False;

		case SSL_ERROR_SYSCALL:
			printf("%s: I/O error\n", func);
			return True;

		case SSL_ERROR_SSL:
			printf("%s: Failure in SSL library (protocol error?)\n", func);
			return True;

		default:
			printf("%s: Unknown error\n", func);
			return True;
	}
}

CryptoCert tls_get_certificate(rdpTls * tls)
{
	CryptoCert cert;
	X509* server_cert;

	server_cert = SSL_get_peer_certificate(tls->ssl);

	if (!server_cert)
	{
		printf("ssl_verify: failed to get the server SSL certificate\n");
		cert = NULL;
	}
	else
	{
		cert = xmalloc(sizeof(*cert));
		cert->px509 = server_cert;
	}

	return cert;
}

rdpTls* tls_new()
{
	rdpTls* tls;

	tls = (rdpTls*) xzalloc(sizeof(rdpTls));

	if (tls != NULL)
	{
		tls->connect = tls_connect;
		tls->disconnect = tls_disconnect;

		SSL_load_error_strings();
		SSL_library_init();

		tls->ctx = SSL_CTX_new(TLSv1_client_method());

		if (tls->ctx == NULL)
		{
			printf("SSL_CTX_new failed\n");
			return NULL;
		}

		/*
		 * This is necessary, because the Microsoft TLS implementation is not perfect.
		 * SSL_OP_ALL enables a couple of workarounds for buggy TLS implementations,
		 * but the most important workaround being SSL_OP_TLS_BLOCK_PADDING_BUG.
		 * As the size of the encrypted payload may give hints about its contents,
		 * block padding is normally used, but the Microsoft TLS implementation
		 * won't recognize it and will disconnect you after sending a TLS alert.
		 */

		SSL_CTX_set_options(tls->ctx, SSL_OP_ALL);

		/* a small 0.1ms delay when network blocking happens. */
		tls->ts.tv_sec = 0;
		tls->ts.tv_nsec = 100000;
	}

	return tls;
}

void tls_free(rdpTls* tls)
{
	if (tls != NULL)
	{
		xfree(tls);
	}
}

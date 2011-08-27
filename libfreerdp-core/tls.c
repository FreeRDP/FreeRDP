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

	tls->ctx = SSL_CTX_new(TLSv1_client_method());

	if (tls->ctx == NULL)
	{
		printf("SSL_CTX_new failed\n");
		return False;
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

	connection_status = SSL_connect(tls->ssl);

	if (connection_status <= 0)
	{
		if (tls_print_error("SSL_connect", tls->ssl, connection_status))
			return False;
	}

	printf("TLS connection established\n");

	return True;
}

boolean tls_accept(rdpTls* tls, const char* cert_file, const char* privatekey_file)
{
	int connection_status;

	tls->ctx = SSL_CTX_new(TLSv1_server_method());

	if (tls->ctx == NULL)
	{
		printf("SSL_CTX_new failed\n");
		return False;
	}

	if (SSL_CTX_use_RSAPrivateKey_file(tls->ctx, privatekey_file, SSL_FILETYPE_PEM) <= 0)
	{
		printf("SSL_CTX_use_RSAPrivateKey_file failed\n");
		return False;
	}

	tls->ssl = SSL_new(tls->ctx);

	if (tls->ssl == NULL)
	{
		printf("SSL_new failed\n");
		return False;
	}

	if (SSL_use_certificate_file(tls->ssl, cert_file, SSL_FILETYPE_PEM) <= 0)
	{
		printf("SSL_use_certificate_file failed\n");
		return False;
	}

	if (SSL_set_fd(tls->ssl, tls->sockfd) < 1)
	{
		printf("SSL_set_fd failed\n");
		return False;
	}

	connection_status = SSL_accept(tls->ssl);

	if (connection_status <= 0)
	{
		if (tls_print_error("SSL_accept", tls->ssl, connection_status))
			return False;
	}

	printf("TLS connection accepted\n");

	return True;
}

boolean tls_disconnect(rdpTls* tls)
{
	SSL_shutdown(tls->ssl);
	return True;
}

int tls_read(rdpTls* tls, uint8* data, int length)
{
	int status;

	status = SSL_read(tls->ssl, data, length);

	switch (SSL_get_error(tls->ssl, status))
	{
		case SSL_ERROR_NONE:
			break;

		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			status = 0;
			break;

		default:
			tls_print_error("SSL_read", tls->ssl, status);
			status = -1;
			break;
	}

	return status;
}

int tls_write(rdpTls* tls, uint8* data, int length)
{
	int status;

	status = SSL_write(tls->ssl, data, length);

	switch (SSL_get_error(tls->ssl, status))
	{
		case SSL_ERROR_NONE:
			break;

		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			status = 0;
			break;

		default:
			tls_print_error("SSL_write", tls->ssl, status);
			status = -1;
			break;
	}

	return status;
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
		tls->accept = tls_accept;
		tls->disconnect = tls_disconnect;

		SSL_load_error_strings();
		SSL_library_init();
	}

	return tls;
}
boolean tls_verify_cert(CryptoCert cert)
{
    X509 *xcert=cert->px509;
    char dir_path[1024]="";
    int ret=0;
    X509_STORE *cert_ctx=NULL;
    X509_LOOKUP *lookup=NULL;
    X509_STORE_CTX *csc;
    cert_ctx=X509_STORE_new();
    if (cert_ctx == NULL)
        goto end;
    OpenSSL_add_all_algorithms();
    lookup=X509_STORE_add_lookup(cert_ctx,X509_LOOKUP_file());
    if (lookup == NULL)
        goto end;
    lookup=X509_STORE_add_lookup(cert_ctx,X509_LOOKUP_hash_dir());
    if (lookup == NULL)
        goto end;
    X509_LOOKUP_add_dir(lookup,NULL,X509_FILETYPE_DEFAULT);
    X509_LOOKUP_add_dir(lookup,CA_LOCAL_PATH,X509_FILETYPE_ASN1);
    csc = X509_STORE_CTX_new();
    if (csc == NULL)
        goto end;
    X509_STORE_set_flags(cert_ctx, 0);
    if(!X509_STORE_CTX_init(csc,cert_ctx,xcert,0))
        goto end;
    int i=X509_verify_cert(csc);
    X509_STORE_CTX_free(csc);
    X509_STORE_free(cert_ctx);
    ret=0;
    end:
    ret = (i > 0);
    return(ret);
}

void tls_free(rdpTls* tls)
{
	if (tls != NULL)
	{
		if (tls->ssl)
			SSL_free(tls->ssl);
		if (tls->ctx)
			SSL_CTX_free(tls->ctx);
		xfree(tls);
	}
}

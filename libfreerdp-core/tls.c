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
 *		 http://www.apache.org/licenses/LICENSE-2.0
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
		return false;
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
		return false;
	}

	if (SSL_set_fd(tls->ssl, tls->sockfd) < 1)
	{
		printf("SSL_set_fd failed\n");
		return false;
	}

	connection_status = SSL_connect(tls->ssl);

	if (connection_status <= 0)
	{
		if (tls_print_error("SSL_connect", tls->ssl, connection_status))
			return false;
	}

	return true;
}

boolean tls_accept(rdpTls* tls, const char* cert_file, const char* privatekey_file)
{
	int connection_status;

	tls->ctx = SSL_CTX_new(TLSv1_server_method());

	if (tls->ctx == NULL)
	{
		printf("SSL_CTX_new failed\n");
		return false;
	}

	if (SSL_CTX_use_RSAPrivateKey_file(tls->ctx, privatekey_file, SSL_FILETYPE_PEM) <= 0)
	{
		printf("SSL_CTX_use_RSAPrivateKey_file failed\n");
		return false;
	}

	tls->ssl = SSL_new(tls->ctx);

	if (tls->ssl == NULL)
	{
		printf("SSL_new failed\n");
		return false;
	}

	if (SSL_use_certificate_file(tls->ssl, cert_file, SSL_FILETYPE_PEM) <= 0)
	{
		printf("SSL_use_certificate_file failed\n");
		return false;
	}

	if (SSL_set_fd(tls->ssl, tls->sockfd) < 1)
	{
		printf("SSL_set_fd failed\n");
		return false;
	}

	connection_status = SSL_accept(tls->ssl);

	if (connection_status <= 0)
	{
		if (tls_print_error("SSL_accept", tls->ssl, connection_status))
			return false;
	}

	printf("TLS connection accepted\n");

	return true;
}

boolean tls_disconnect(rdpTls* tls)
{
	SSL_shutdown(tls->ssl);
	return true;
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
			return true;

		case SSL_ERROR_WANT_READ:
			printf("SSL_ERROR_WANT_READ\n");
			return false;

		case SSL_ERROR_WANT_WRITE:
			printf("SSL_ERROR_WANT_WRITE\n");
			return false;

		case SSL_ERROR_SYSCALL:
			printf("%s: I/O error\n", func);
			return true;

		case SSL_ERROR_SSL:
			printf("%s: Failure in SSL library (protocol error?)\n", func);
			return true;

		default:
			printf("%s: Unknown error\n", func);
			return true;
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
		SSL_load_error_strings();
		SSL_library_init();
	}

	return tls;
}

int tls_verify_certificate(CryptoCert cert, rdpSettings* settings, char* hostname)
{
	boolean status;
	rdpCertStore* certstore;
	status = x509_verify_cert(cert, settings);

	if (status != true)
	{
		rdpCertData* certdata;
		certdata = crypto_get_cert_data(cert->px509, hostname);
		certstore = certstore_new(certdata, settings->home_path);

		if (cert_data_match(certstore) == 0)
			goto end;

		if (certstore->match == 1)
		{
			boolean accept_certificate = settings->ignore_certificate;		
			if(!accept_certificate)
			{
				char* issuer = crypto_cert_issuer(cert->px509);
				char* subject = crypto_cert_subject(cert->px509);
				char* fingerprint = crypto_cert_fingerprint(cert->px509);

				freerdp* instance = (freerdp*)settings->instance;			
				if(instance->VerifyCertificate)
					accept_certificate = instance->VerifyCertificate(instance, subject, issuer, fingerprint);

				xfree(issuer);
				xfree(subject);
				xfree(fingerprint);
			}

			if(!accept_certificate)
				return 1;

			cert_data_print(certstore);
		}
		else if (certstore->match == -1)
		{
			tls_print_cert_error();
			certstore_free(certstore);
			return 1;
		}

end:
		certstore_free(certstore);
	}

	return 0;
}

void tls_print_cert_error()
{
	printf("#####################################\n");
	printf("##############WARNING################\n");
	printf("#####################################\n");
	printf("The thumbprint of certificate received\n");
	printf("did not match the stored thumbprint.You\n");
	printf("might be a victim of MAN in the MIDDLE\n");
	printf("ATTACK.It is also possible that server's\n");
	printf("certificate have been changed.In that case\n");
	printf("contact your server administrator\n");
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

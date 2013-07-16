/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Transport Layer Security
 *
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/sspi.h>

#include <winpr/stream.h>
#include <freerdp/utils/tcp.h>

#include <freerdp/crypto/tls.h>

static CryptoCert tls_get_certificate(rdpTls* tls, BOOL peer)
{
	CryptoCert cert;
	X509* server_cert;

	if (peer)
		server_cert = SSL_get_peer_certificate(tls->ssl);
	else
		server_cert = SSL_get_certificate(tls->ssl);

	if (!server_cert)
	{
		fprintf(stderr, "tls_get_certificate: failed to get the server TLS certificate\n");
		cert = NULL;
	}
	else
	{
		cert = malloc(sizeof(*cert));
		cert->px509 = server_cert;
	}

	return cert;
}

static void tls_free_certificate(CryptoCert cert)
{
	X509_free(cert->px509);
	free(cert);
}

#define TLS_SERVER_END_POINT	"tls-server-end-point:"

SecPkgContext_Bindings* tls_get_channel_bindings(X509* cert)
{
	int PrefixLength;
	BYTE CertificateHash[32];
	UINT32 CertificateHashLength;
	BYTE* ChannelBindingToken;
	UINT32 ChannelBindingTokenLength;
	SEC_CHANNEL_BINDINGS* ChannelBindings;
	SecPkgContext_Bindings* ContextBindings;

	ZeroMemory(CertificateHash, sizeof(CertificateHash));
	X509_digest(cert, EVP_sha256(), CertificateHash, &CertificateHashLength);

	PrefixLength = strlen(TLS_SERVER_END_POINT);
	ChannelBindingTokenLength = PrefixLength + CertificateHashLength;

	ContextBindings = (SecPkgContext_Bindings*) malloc(sizeof(SecPkgContext_Bindings));
	ZeroMemory(ContextBindings, sizeof(SecPkgContext_Bindings));

	ContextBindings->BindingsLength = sizeof(SEC_CHANNEL_BINDINGS) + ChannelBindingTokenLength;
	ChannelBindings = (SEC_CHANNEL_BINDINGS*) malloc(ContextBindings->BindingsLength);
	ZeroMemory(ChannelBindings, ContextBindings->BindingsLength);
	ContextBindings->Bindings = ChannelBindings;

	ChannelBindings->cbApplicationDataLength = ChannelBindingTokenLength;
	ChannelBindings->dwApplicationDataOffset = sizeof(SEC_CHANNEL_BINDINGS);
	ChannelBindingToken = &((BYTE*) ChannelBindings)[ChannelBindings->dwApplicationDataOffset];

	strcpy((char*) ChannelBindingToken, TLS_SERVER_END_POINT);
	CopyMemory(&ChannelBindingToken[PrefixLength], CertificateHash, CertificateHashLength);

	return ContextBindings;
}

BOOL tls_connect(rdpTls* tls)
{
	CryptoCert cert;
	long options = 0;
	int connection_status;
	char *hostname;

	tls->ctx = SSL_CTX_new(TLSv1_client_method());

	if (tls->ctx == NULL)
	{
		fprintf(stderr, "SSL_CTX_new failed\n");
		return FALSE;
	}

	//SSL_CTX_set_mode(tls->ctx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER | SSL_MODE_ENABLE_PARTIAL_WRITE);

	/**
	 * SSL_OP_NO_COMPRESSION:
	 *
	 * The Microsoft RDP server does not advertise support
	 * for TLS compression, but alternative servers may support it.
	 * This was observed between early versions of the FreeRDP server
	 * and the FreeRDP client, and caused major performance issues,
	 * which is why we're disabling it.
	 */
#ifdef SSL_OP_NO_COMPRESSION
	options |= SSL_OP_NO_COMPRESSION;
#endif
	 
	/**
	 * SSL_OP_TLS_BLOCK_PADDING_BUG:
	 *
	 * The Microsoft RDP server does *not* support TLS padding.
	 * It absolutely needs to be disabled otherwise it won't work.
	 */
	options |= SSL_OP_TLS_BLOCK_PADDING_BUG;

	/**
	 * SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS:
	 *
	 * Just like TLS padding, the Microsoft RDP server does not
	 * support empty fragments. This needs to be disabled.
	 */
	options |= SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS;

	SSL_CTX_set_options(tls->ctx, options);

	tls->ssl = SSL_new(tls->ctx);

	if (tls->ssl == NULL)
	{
		fprintf(stderr, "SSL_new failed\n");
		return FALSE;
	}

	if (SSL_set_fd(tls->ssl, tls->sockfd) < 1)
	{
		fprintf(stderr, "SSL_set_fd failed\n");
		return FALSE;
	}

	connection_status = SSL_connect(tls->ssl);

	if (connection_status <= 0)
	{
		if (tls_print_error("SSL_connect", tls->ssl, connection_status))
		{
			return FALSE;
		}
	}

	cert = tls_get_certificate(tls, TRUE);

	if (cert == NULL)
	{
		fprintf(stderr, "tls_connect: tls_get_certificate failed to return the server certificate.\n");
		return FALSE;
	}

	tls->Bindings = tls_get_channel_bindings(cert->px509);

	if (!crypto_cert_get_public_key(cert, &tls->PublicKey, &tls->PublicKeyLength))
	{
		fprintf(stderr, "tls_connect: crypto_cert_get_public_key failed to return the server public key.\n");
		tls_free_certificate(cert);
		return FALSE;
	}

	if (tls->settings->GatewayEnabled)
		hostname = tls->settings->GatewayHostname;
	else
		hostname = tls->settings->ServerHostname;

	if (!tls_verify_certificate(tls, cert, hostname))
	{
		fprintf(stderr, "tls_connect: certificate not trusted, aborting.\n");
		tls_disconnect(tls);
		tls_free_certificate(cert);
		return FALSE;
	}

	tls_free_certificate(cert);

	return TRUE;
}

BOOL tls_accept(rdpTls* tls, const char* cert_file, const char* privatekey_file)
{
	CryptoCert cert;
	long options = 0;
	int connection_status;

	tls->ctx = SSL_CTX_new(SSLv23_server_method());

	if (tls->ctx == NULL)
	{
		fprintf(stderr, "SSL_CTX_new failed\n");
		return FALSE;
	}

	/*
	 * SSL_OP_NO_SSLv2:
	 *
	 * We only want SSLv3 and TLSv1, so disable SSLv2.
	 * SSLv3 is used by, eg. Microsoft RDC for Mac OS X.
	 */
	options |= SSL_OP_NO_SSLv2;

	/**
	 * SSL_OP_NO_COMPRESSION:
	 *
	 * The Microsoft RDP server does not advertise support
	 * for TLS compression, but alternative servers may support it.
	 * This was observed between early versions of the FreeRDP server
	 * and the FreeRDP client, and caused major performance issues,
	 * which is why we're disabling it.
	 */
#ifdef SSL_OP_NO_COMPRESSION
	options |= SSL_OP_NO_COMPRESSION;
#endif
	 
	/**
	 * SSL_OP_TLS_BLOCK_PADDING_BUG:
	 *
	 * The Microsoft RDP server does *not* support TLS padding.
	 * It absolutely needs to be disabled otherwise it won't work.
	 */
	options |= SSL_OP_TLS_BLOCK_PADDING_BUG;

	/**
	 * SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS:
	 *
	 * Just like TLS padding, the Microsoft RDP server does not
	 * support empty fragments. This needs to be disabled.
	 */
	options |= SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS;

	SSL_CTX_set_options(tls->ctx, options);

	if (SSL_CTX_use_RSAPrivateKey_file(tls->ctx, privatekey_file, SSL_FILETYPE_PEM) <= 0)
	{
		fprintf(stderr, "SSL_CTX_use_RSAPrivateKey_file failed\n");
		fprintf(stderr, "PrivateKeyFile: %s\n", privatekey_file);
		return FALSE;
	}

	tls->ssl = SSL_new(tls->ctx);

	if (!tls->ssl)
	{
		fprintf(stderr, "SSL_new failed\n");
		return FALSE;
	}

	if (SSL_use_certificate_file(tls->ssl, cert_file, SSL_FILETYPE_PEM) <= 0)
	{
		fprintf(stderr, "SSL_use_certificate_file failed\n");
		return FALSE;
	}

	if (SSL_set_fd(tls->ssl, tls->sockfd) < 1)
	{
		fprintf(stderr, "SSL_set_fd failed\n");
		return FALSE;
	}

	while (1)
	{
		connection_status = SSL_accept(tls->ssl);

		if (connection_status <= 0)
		{
			switch (SSL_get_error(tls->ssl, connection_status))
			{
				case SSL_ERROR_WANT_READ:
				case SSL_ERROR_WANT_WRITE:
					break;

				default:
					if (tls_print_error("SSL_accept", tls->ssl, connection_status))
						return FALSE;
					break;

			}
		}
		else
		{
			break;
		}
	}

	cert = tls_get_certificate(tls, FALSE);

	if (!cert)
	{
		fprintf(stderr, "tls_connect: tls_get_certificate failed to return the server certificate.\n");
		return FALSE;
	}

	if (!crypto_cert_get_public_key(cert, &tls->PublicKey, &tls->PublicKeyLength))
	{
		fprintf(stderr, "tls_connect: crypto_cert_get_public_key failed to return the server public key.\n");
		tls_free_certificate(cert);
		return FALSE;
	}

	free(cert);

	fprintf(stderr, "TLS connection accepted\n");

	return TRUE;
}

BOOL tls_disconnect(rdpTls* tls)
{
	if (tls->ssl)
		SSL_shutdown(tls->ssl);

	return TRUE;
}

int tls_read(rdpTls* tls, BYTE* data, int length)
{
	int error;
	int status;

	status = SSL_read(tls->ssl, data, length);

	if (status <= 0)
	{
		error = SSL_get_error(tls->ssl, status);

		//fprintf(stderr, "tls_read: length: %d status: %d error: 0x%08X\n",
		//		length, status, error);

		switch (error)
		{
			case SSL_ERROR_NONE:
				break;

			case SSL_ERROR_WANT_READ:
			case SSL_ERROR_WANT_WRITE:
				status = 0;
				break;

			case SSL_ERROR_SYSCALL:
				if (errno == EAGAIN)
				{
					status = 0;
				}
				else
				{
					tls_print_error("SSL_read", tls->ssl, status);
					status = -1;
				}
				break;

			default:
				tls_print_error("SSL_read", tls->ssl, status);
				status = -1;
				break;
		}
	}

	return status;
}

int tls_read_all(rdpTls* tls, BYTE* data, int length)
{
	int status;

	do
	{
		status = tls_read(tls, data, length);
		if (status == 0)
			tls_wait_read(tls);
	}
	while (status == 0);

	return status;
}

int tls_write(rdpTls* tls, BYTE* data, int length)
{
	int error;
	int status;

	status = SSL_write(tls->ssl, data, length);

	if (status <= 0)
	{
		error = SSL_get_error(tls->ssl, status);

		//fprintf(stderr, "tls_write: length: %d status: %d error: 0x%08X\n", length, status, error);

		switch (error)
		{
			case SSL_ERROR_NONE:
				break;

			case SSL_ERROR_WANT_READ:
			case SSL_ERROR_WANT_WRITE:
				status = 0;
				break;

			case SSL_ERROR_SYSCALL:
				if (errno == EAGAIN)
				{
					status = 0;
				}
				else
				{
					tls_print_error("SSL_write", tls->ssl, status);
					status = -1;
				}
				break;

			default:
				tls_print_error("SSL_write", tls->ssl, status);
				status = -1;
				break;
		}
	}

	return status;
}

int tls_write_all(rdpTls* tls, BYTE* data, int length)
{
	int status;
	int sent = 0;

	do
	{
		status = tls_write(tls, &data[sent], length - sent);

		if (status > 0)
			sent += status;
		else if (status == 0)
			tls_wait_write(tls);

		if (sent >= length)
			break;
	}
	while (status >= 0);

	if (status > 0)
		return length;
	else
		return status;
}

int tls_wait_read(rdpTls* tls)
{
	return freerdp_tcp_wait_read(tls->sockfd);
}

int tls_wait_write(rdpTls* tls)
{
	return freerdp_tcp_wait_write(tls->sockfd);
}

static void tls_errors(const char *prefix)
{
	unsigned long error;

	while ((error = ERR_get_error()) != 0)
		fprintf(stderr, "%s: %s\n", prefix, ERR_error_string(error, NULL));
}

BOOL tls_print_error(char* func, SSL* connection, int value)
{
	switch (SSL_get_error(connection, value))
	{
		case SSL_ERROR_ZERO_RETURN:
			fprintf(stderr, "%s: Server closed TLS connection\n", func);
			return TRUE;

		case SSL_ERROR_WANT_READ:
			fprintf(stderr, "%s: SSL_ERROR_WANT_READ\n", func);
			return FALSE;

		case SSL_ERROR_WANT_WRITE:
			fprintf(stderr, "%s: SSL_ERROR_WANT_WRITE\n", func);
			return FALSE;

		case SSL_ERROR_SYSCALL:
			fprintf(stderr, "%s: I/O error: %s (%d)\n", func, strerror(errno), errno);
			tls_errors(func);
			return TRUE;

		case SSL_ERROR_SSL:
			fprintf(stderr, "%s: Failure in SSL library (protocol error?)\n", func);
			tls_errors(func);
			return TRUE;

		default:
			fprintf(stderr, "%s: Unknown error\n", func);
			tls_errors(func);
			return TRUE;
	}
}

BOOL tls_match_hostname(char *pattern, int pattern_length, char *hostname)
{
	if (strlen(hostname) == pattern_length)
	{
		if (memcmp((void*) hostname, (void*) pattern, pattern_length) == 0)
			return TRUE;
	}

	if (pattern_length > 2 && pattern[0] == '*' && pattern[1] == '.' && strlen(hostname) >= pattern_length)
	{
		char *check_hostname = &hostname[ strlen(hostname) - pattern_length+1 ];
		if (memcmp((void*) check_hostname, (void*) &pattern[1], pattern_length - 1) == 0 )
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOL tls_verify_certificate(rdpTls* tls, CryptoCert cert, char* hostname)
{
	int match;
	int index;
	char* common_name = NULL;
	int common_name_length = 0;
	char** alt_names = NULL;
	int alt_names_count = 0;
	int* alt_names_lengths = NULL;
	BOOL certificate_status;
	BOOL hostname_match = FALSE;
	BOOL verification_status = FALSE;
	rdpCertificateData* certificate_data;

	/* ignore certificate verification if user explicitly required it (discouraged) */
	if (tls->settings->IgnoreCertificate)
		return TRUE;  /* success! */

	/* if user explicitly specified a certificate name, use it instead of the hostname */
	if (tls->settings->CertificateName)
		hostname = tls->settings->CertificateName;

	/* attempt verification using OpenSSL and the ~/.freerdp/certs certificate store */
	certificate_status = x509_verify_certificate(cert, tls->certificate_store->path);

	/* verify certificate name match */
	certificate_data = crypto_get_certificate_data(cert->px509, hostname);

	/* extra common name and alternative names */
	common_name = crypto_cert_subject_common_name(cert->px509, &common_name_length);
	alt_names = crypto_cert_subject_alt_name(cert->px509, &alt_names_count, &alt_names_lengths);

	/* compare against common name */

	if (common_name != NULL)
	{
		if (tls_match_hostname(common_name, common_name_length, hostname))
			hostname_match = TRUE;
	}

	/* compare against alternative names */

	if (alt_names != NULL)
	{
		for (index = 0; index < alt_names_count; index++)
		{
			if (tls_match_hostname(alt_names[index], alt_names_lengths[index], hostname))
			{
				hostname_match = TRUE;
				break;
			}
		}
	}

	/* if the certificate is valid and the certificate name matches, verification succeeds */
	if (certificate_status && hostname_match)
	{
		if (common_name)
		{
			free(common_name);
			common_name = NULL;
		}

		verification_status = TRUE; /* success! */
	}

	/* if the certificate is valid but the certificate name does not match, warn user, do not accept */
	if (certificate_status && !hostname_match)
		tls_print_certificate_name_mismatch_error(hostname, common_name, alt_names, alt_names_count);

	/* verification could not succeed with OpenSSL, use known_hosts file and prompt user for manual verification */

	if (!certificate_status)
	{
		char* issuer;
		char* subject;
		char* fingerprint;
		freerdp* instance = (freerdp*) tls->settings->instance;
		BOOL accept_certificate = FALSE;

		issuer = crypto_cert_issuer(cert->px509);
		subject = crypto_cert_subject(cert->px509);
		fingerprint = crypto_cert_fingerprint(cert->px509);

		/* search for matching entry in known_hosts file */
		match = certificate_data_match(tls->certificate_store, certificate_data);

		if (match == 1)
		{
			/* no entry was found in known_hosts file, prompt user for manual verification */
			if (!hostname_match)
				tls_print_certificate_name_mismatch_error(hostname, common_name, alt_names, alt_names_count);

			if (instance->VerifyCertificate)
				accept_certificate = instance->VerifyCertificate(instance, subject, issuer, fingerprint);

			if (!accept_certificate)
			{
				/* user did not accept, abort and do not add entry in known_hosts file */
				verification_status = FALSE; /* failure! */
			}
			else
			{
				/* user accepted certificate, add entry in known_hosts file */
				certificate_data_print(tls->certificate_store, certificate_data);
				verification_status = TRUE; /* success! */
			}
		}
		else if (match == -1)
		{
			/* entry was found in known_hosts file, but fingerprint does not match. ask user to use it */
			tls_print_certificate_error(hostname, fingerprint, tls->certificate_store->file);
			
			if (instance->VerifyChangedCertificate)
				accept_certificate = instance->VerifyChangedCertificate(instance, subject, issuer, fingerprint, "");

			if (!accept_certificate)
			{
				/* user did not accept, abort and do not change known_hosts file */
				verification_status = FALSE;  /* failure! */
			}
			else
			{
				/* user accepted new certificate, add replace fingerprint for this host in known_hosts file */
				certificate_data_replace(tls->certificate_store, certificate_data);
				verification_status = TRUE; /* success! */
			}
		}
		else if (match == 0)
		{
			verification_status = TRUE; /* success! */
		}

		free(issuer);
		free(subject);
		free(fingerprint);
	}

	if (certificate_data)
	{
		free(certificate_data->fingerprint);
		free(certificate_data->hostname);
		free(certificate_data);
	}

#ifndef _WIN32
	if (common_name)
		free(common_name);
#endif

	return verification_status;
}

void tls_print_certificate_error(char* hostname, char* fingerprint, char *hosts_file)
{
	fprintf(stderr, "The host key for %s has changed\n", hostname);
	fprintf(stderr, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
	fprintf(stderr, "@    WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!     @\n");
	fprintf(stderr, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
	fprintf(stderr, "IT IS POSSIBLE THAT SOMEONE IS DOING SOMETHING NASTY!\n");
	fprintf(stderr, "Someone could be eavesdropping on you right now (man-in-the-middle attack)!\n");
	fprintf(stderr, "It is also possible that a host key has just been changed.\n");
	fprintf(stderr, "The fingerprint for the host key sent by the remote host is\n%s\n", fingerprint);
	fprintf(stderr, "Please contact your system administrator.\n");
	fprintf(stderr, "Add correct host key in %s to get rid of this message.\n", hosts_file);
	fprintf(stderr, "Host key for %s has changed and you have requested strict checking.\n", hostname);
	fprintf(stderr, "Host key verification failed.\n");
}

void tls_print_certificate_name_mismatch_error(char* hostname, char* common_name, char** alt_names, int alt_names_count)
{
	int index;

	fprintf(stderr, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
	fprintf(stderr, "@           WARNING: CERTIFICATE NAME MISMATCH!           @\n");
	fprintf(stderr, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
	fprintf(stderr, "The hostname used for this connection (%s) \n", hostname);
	fprintf(stderr, "does not match %s given in the certificate:\n", alt_names_count < 1 ? "the name" : "any of the names");
	fprintf(stderr, "Common Name (CN):\n");
	fprintf(stderr, "\t%s\n", common_name ? common_name : "no CN found in certificate");
	if (alt_names_count > 1)
	{
		fprintf(stderr, "Alternative names:\n");
		if (alt_names_count > 1)
		{
			for (index = 0; index < alt_names_count; index++)
			{
				fprintf(stderr, "\t %s\n", alt_names[index]);
			}
		}
	}
	fprintf(stderr, "A valid certificate for the wrong name should NOT be trusted!\n");
}

rdpTls* tls_new(rdpSettings* settings)
{
	rdpTls* tls;

	tls = (rdpTls*) malloc(sizeof(rdpTls));

	if (tls != NULL)
	{
		ZeroMemory(tls, sizeof(rdpTls));

		SSL_load_error_strings();
		SSL_library_init();

		tls->settings = settings;
		tls->certificate_store = certificate_store_new(settings);
	}

	return tls;
}

void tls_free(rdpTls* tls)
{
	if (tls != NULL)
	{
		if (tls->ssl)
			SSL_free(tls->ssl);

		if (tls->ctx)
			SSL_CTX_free(tls->ctx);

		if (tls->PublicKey)
			free(tls->PublicKey);

		if (tls->Bindings)
		{
			free(tls->Bindings->Bindings);
			free(tls->Bindings);
		}

		certificate_store_free(tls->certificate_store);

		free(tls);
	}
}

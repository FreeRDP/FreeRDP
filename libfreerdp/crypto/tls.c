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

#include <assert.h>
#include <string.h>
#include <errno.h>

#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/ssl.h>

#include <winpr/stream.h>
#include <freerdp/utils/ringbuffer.h>

#include <freerdp/log.h>
#include <freerdp/crypto/tls.h>
#include "../core/tcp.h"
#include "opensslcompat.h"

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#ifdef HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#endif

#define TAG FREERDP_TAG("crypto")



/**
 * Earlier Microsoft iOS RDP clients have sent a null or even double null
 * terminated hostname in the SNI TLS extension.
 * If the length indicator does not equal the hostname strlen OpenSSL
 * will abort (see openssl:ssl/t1_lib.c).
 * Here is a tcpdump segment of Microsoft Remote Desktop Client Version
 * 8.1.7 running on an iPhone 4 with iOS 7.1.2 showing the transmitted
 * SNI hostname TLV blob when connection to server "abcd":
 * 00                  name_type 0x00 (host_name)
 * 00 06               length_in_bytes 0x0006
 * 61 62 63 64 00 00   host_name "abcd\0\0"
 *
 * Currently the only (runtime) workaround is setting an openssl tls
 * extension debug callback that sets the SSL context's servername_done
 * to 1 which effectively disables the parsing of that extension type.
 *
 * Nowadays this workaround is not required anymore but still can be
 * activated by adding the following define:
 *
 * #define MICROSOFT_IOS_SNI_BUG
 */

struct _BIO_RDP_TLS
{
	SSL* ssl;
	CRITICAL_SECTION lock;
};
typedef struct _BIO_RDP_TLS BIO_RDP_TLS;

long bio_rdp_tls_callback(BIO* bio, int mode, const char* argp, int argi,
                          long argl, long ret)
{
	return 1;
}

static int bio_rdp_tls_write(BIO* bio, const char* buf, int size)
{
	int error;
	int status;
	BIO_RDP_TLS* tls = (BIO_RDP_TLS*) BIO_get_data(bio);

	if (!buf || !tls)
		return 0;

	BIO_clear_flags(bio, BIO_FLAGS_WRITE | BIO_FLAGS_READ | BIO_FLAGS_IO_SPECIAL);
	EnterCriticalSection(&tls->lock);
	status = SSL_write(tls->ssl, buf, size);
	error = SSL_get_error(tls->ssl, status);
	LeaveCriticalSection(&tls->lock);

	if (status <= 0)
	{
		switch (error)
		{
			case SSL_ERROR_NONE:
				BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
				break;

			case SSL_ERROR_WANT_WRITE:
				BIO_set_flags(bio, BIO_FLAGS_WRITE | BIO_FLAGS_SHOULD_RETRY);
				break;

			case SSL_ERROR_WANT_READ:
				BIO_set_flags(bio, BIO_FLAGS_READ | BIO_FLAGS_SHOULD_RETRY);
				break;

			case SSL_ERROR_WANT_X509_LOOKUP:
				BIO_set_flags(bio, BIO_FLAGS_IO_SPECIAL);
				BIO_set_retry_reason(bio, BIO_RR_SSL_X509_LOOKUP);
				break;

			case SSL_ERROR_WANT_CONNECT:
				BIO_set_flags(bio, BIO_FLAGS_IO_SPECIAL);
				BIO_set_retry_reason(bio, BIO_RR_CONNECT);
				break;

			case SSL_ERROR_SYSCALL:
				BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
				break;

			case SSL_ERROR_SSL:
				BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
				break;
		}
	}

	return status;
}

static int bio_rdp_tls_read(BIO* bio, char* buf, int size)
{
	int error;
	int status;
	BIO_RDP_TLS* tls = (BIO_RDP_TLS*) BIO_get_data(bio);

	if (!buf || !tls)
		return 0;

	BIO_clear_flags(bio, BIO_FLAGS_WRITE | BIO_FLAGS_READ | BIO_FLAGS_IO_SPECIAL);
	EnterCriticalSection(&tls->lock);
	status = SSL_read(tls->ssl, buf, size);
	error = SSL_get_error(tls->ssl, status);
	LeaveCriticalSection(&tls->lock);

	if (status <= 0)
	{
		switch (error)
		{
			case SSL_ERROR_NONE:
				BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
				break;

			case SSL_ERROR_WANT_READ:
				BIO_set_flags(bio, BIO_FLAGS_READ | BIO_FLAGS_SHOULD_RETRY);
				break;

			case SSL_ERROR_WANT_WRITE:
				BIO_set_flags(bio, BIO_FLAGS_WRITE | BIO_FLAGS_SHOULD_RETRY);
				break;

			case SSL_ERROR_WANT_X509_LOOKUP:
				BIO_set_flags(bio, BIO_FLAGS_IO_SPECIAL);
				BIO_set_retry_reason(bio, BIO_RR_SSL_X509_LOOKUP);
				break;

			case SSL_ERROR_WANT_ACCEPT:
				BIO_set_flags(bio, BIO_FLAGS_IO_SPECIAL);
				BIO_set_retry_reason(bio, BIO_RR_ACCEPT);
				break;

			case SSL_ERROR_WANT_CONNECT:
				BIO_set_flags(bio, BIO_FLAGS_IO_SPECIAL);
				BIO_set_retry_reason(bio, BIO_RR_CONNECT);
				break;

			case SSL_ERROR_SSL:
				BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
				break;

			case SSL_ERROR_ZERO_RETURN:
				BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
				break;

			case SSL_ERROR_SYSCALL:
				BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
				break;
		}
	}

#ifdef HAVE_VALGRIND_MEMCHECK_H

	if (status > 0)
	{
		VALGRIND_MAKE_MEM_DEFINED(buf, status);
	}

#endif
	return status;
}

static int bio_rdp_tls_puts(BIO* bio, const char* str)
{
	int size;
	int status;

	if (!str)
		return 0;

	size = strlen(str);
	status = BIO_write(bio, str, size);
	return status;
}

static int bio_rdp_tls_gets(BIO* bio, char* str, int size)
{
	return 1;
}

static long bio_rdp_tls_ctrl(BIO* bio, int cmd, long num, void* ptr)
{
	BIO* ssl_rbio;
	BIO* ssl_wbio;
	BIO* next_bio;
	int status = -1;
	BIO_RDP_TLS* tls = (BIO_RDP_TLS*) BIO_get_data(bio);

	if (!tls)
		return 0;

	if (!tls->ssl && (cmd != BIO_C_SET_SSL))
		return 0;

	next_bio = BIO_next(bio);
	ssl_rbio = tls->ssl ? SSL_get_rbio(tls->ssl) : NULL;
	ssl_wbio = tls->ssl ? SSL_get_wbio(tls->ssl) : NULL;

	switch (cmd)
	{
		case BIO_CTRL_RESET:
			SSL_shutdown(tls->ssl);

			if (SSL_in_connect_init(tls->ssl))
				SSL_set_connect_state(tls->ssl);
			else if (SSL_in_accept_init(tls->ssl))
				SSL_set_accept_state(tls->ssl);

			SSL_clear(tls->ssl);

			if (next_bio)
				status = BIO_ctrl(next_bio, cmd, num, ptr);
			else if (ssl_rbio)
				status = BIO_ctrl(ssl_rbio, cmd, num, ptr);
			else
				status = 1;

			break;

		case BIO_C_GET_FD:
			status = BIO_ctrl(ssl_rbio, cmd, num, ptr);
			break;

		case BIO_CTRL_INFO:
			status = 0;
			break;

		case BIO_CTRL_SET_CALLBACK:
			status = 0;
			break;

		case BIO_CTRL_GET_CALLBACK:
			*((ULONG_PTR*) ptr) = (ULONG_PTR) SSL_get_info_callback(tls->ssl);
			status = 1;
			break;

		case BIO_C_SSL_MODE:
			if (num)
				SSL_set_connect_state(tls->ssl);
			else
				SSL_set_accept_state(tls->ssl);

			status = 1;
			break;

		case BIO_CTRL_GET_CLOSE:
			status = BIO_get_shutdown(bio);
			break;

		case BIO_CTRL_SET_CLOSE:
			BIO_set_shutdown(bio, (int) num);
			status = 1;
			break;

		case BIO_CTRL_WPENDING:
			status = BIO_ctrl(ssl_wbio, cmd, num, ptr);
			break;

		case BIO_CTRL_PENDING:
			status = SSL_pending(tls->ssl);

			if (status == 0)
				status = BIO_pending(ssl_rbio);

			break;

		case BIO_CTRL_FLUSH:
			BIO_clear_retry_flags(bio);
			status = BIO_ctrl(ssl_wbio, cmd, num, ptr);
			BIO_copy_next_retry(bio);
			status = 1;
			break;

		case BIO_CTRL_PUSH:
			if (next_bio && (next_bio != ssl_rbio))
			{
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
				SSL_set_bio(tls->ssl, next_bio, next_bio);
				CRYPTO_add(&(bio->next_bio->references), 1, CRYPTO_LOCK_BIO);
#else
				/*
				 * We are going to pass ownership of next to the SSL object...but
				 * we don't own a reference to pass yet - so up ref
				 */
				BIO_up_ref(next_bio);
				SSL_set_bio(tls->ssl, next_bio, next_bio);
#endif
			}

			status = 1;
			break;

		case BIO_CTRL_POP:

			/* Only detach if we are the BIO explicitly being popped */
			if (bio == ptr)
			{
				if (ssl_rbio != ssl_wbio)
					BIO_free_all(ssl_wbio);

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)

				if (next_bio)
					CRYPTO_add(&(bio->next_bio->references), -1, CRYPTO_LOCK_BIO);

				tls->ssl->wbio = tls->ssl->rbio = NULL;
#else
				/* OpenSSL 1.1: This will also clear the reference we obtained during push */
				SSL_set_bio(tls->ssl, NULL, NULL);
#endif
			}

			status = 1;
			break;

		case BIO_C_GET_SSL:
			if (ptr)
			{
				*((SSL**) ptr) = tls->ssl;
				status = 1;
			}

			break;

		case BIO_C_SET_SSL:
			BIO_set_shutdown(bio, (int) num);

			if (ptr)
			{
				tls->ssl = (SSL*) ptr;
				ssl_rbio = SSL_get_rbio(tls->ssl);
				ssl_wbio = SSL_get_wbio(tls->ssl);
			}

			if (ssl_rbio)
			{
				if (next_bio)
					BIO_push(ssl_rbio, next_bio);

				BIO_set_next(bio, ssl_rbio);
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
				CRYPTO_add(&(ssl_rbio->references), 1, CRYPTO_LOCK_BIO);
#else
				BIO_up_ref(ssl_rbio);
#endif
			}

			BIO_set_init(bio, 1);
			status = 1;
			break;

		case BIO_C_DO_STATE_MACHINE:
			BIO_clear_flags(bio, BIO_FLAGS_READ | BIO_FLAGS_WRITE | BIO_FLAGS_IO_SPECIAL);
			BIO_set_retry_reason(bio, 0);
			status = SSL_do_handshake(tls->ssl);

			if (status <= 0)
			{
				switch (SSL_get_error(tls->ssl, status))
				{
					case SSL_ERROR_WANT_READ:
						BIO_set_flags(bio, BIO_FLAGS_READ | BIO_FLAGS_SHOULD_RETRY);
						break;

					case SSL_ERROR_WANT_WRITE:
						BIO_set_flags(bio, BIO_FLAGS_WRITE | BIO_FLAGS_SHOULD_RETRY);
						break;

					case SSL_ERROR_WANT_CONNECT:
						BIO_set_flags(bio, BIO_FLAGS_IO_SPECIAL | BIO_FLAGS_SHOULD_RETRY);
						BIO_set_retry_reason(bio, BIO_get_retry_reason(next_bio));
						break;

					default:
						BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
						break;
				}
			}

			break;

		default:
			status = BIO_ctrl(ssl_rbio, cmd, num, ptr);
			break;
	}

	return status;
}

static int bio_rdp_tls_new(BIO* bio)
{
	BIO_RDP_TLS* tls;
	BIO_set_flags(bio, BIO_FLAGS_SHOULD_RETRY);

	if (!(tls = calloc(1, sizeof(BIO_RDP_TLS))))
		return 0;

	InitializeCriticalSectionAndSpinCount(&tls->lock, 4000);
	BIO_set_data(bio, (void*) tls);
	return 1;
}

static int bio_rdp_tls_free(BIO* bio)
{
	BIO_RDP_TLS* tls;

	if (!bio)
		return 0;

	tls = (BIO_RDP_TLS*) BIO_get_data(bio);

	if (!tls)
		return 0;

	if (BIO_get_shutdown(bio))
	{
		if (BIO_get_init(bio) && tls->ssl)
		{
			SSL_shutdown(tls->ssl);
			SSL_free(tls->ssl);
		}

		BIO_set_init(bio, 0);
		BIO_set_flags(bio, 0);
	}

	DeleteCriticalSection(&tls->lock);
	free(tls);
	return 1;
}

static long bio_rdp_tls_callback_ctrl(BIO* bio, int cmd, bio_info_cb* fp)
{
	int status = 0;
	BIO_RDP_TLS* tls;

	if (!bio)
		return 0;

	tls = (BIO_RDP_TLS*) BIO_get_data(bio);

	if (!tls)
		return 0;

	switch (cmd)
	{
		case BIO_CTRL_SET_CALLBACK:
			SSL_set_info_callback(tls->ssl, (void (*)(const SSL*, int, int)) fp);
			status = 1;
			break;

		default:
			status = BIO_callback_ctrl(SSL_get_rbio(tls->ssl), cmd, fp);
			break;
	}

	return status;
}

#define BIO_TYPE_RDP_TLS	68

BIO_METHOD* BIO_s_rdp_tls(void)
{
	static BIO_METHOD* bio_methods = NULL;

	if (bio_methods == NULL)
	{
		if (!(bio_methods = BIO_meth_new(BIO_TYPE_RDP_TLS, "RdpTls")))
			return NULL;

		BIO_meth_set_write(bio_methods, bio_rdp_tls_write);
		BIO_meth_set_read(bio_methods, bio_rdp_tls_read);
		BIO_meth_set_puts(bio_methods, bio_rdp_tls_puts);
		BIO_meth_set_gets(bio_methods, bio_rdp_tls_gets);
		BIO_meth_set_ctrl(bio_methods, bio_rdp_tls_ctrl);
		BIO_meth_set_create(bio_methods, bio_rdp_tls_new);
		BIO_meth_set_destroy(bio_methods, bio_rdp_tls_free);
		BIO_meth_set_callback_ctrl(bio_methods, bio_rdp_tls_callback_ctrl);
	}

	return bio_methods;
}

BIO* BIO_new_rdp_tls(SSL_CTX* ctx, int client)
{
	BIO* bio;
	SSL* ssl;
	bio = BIO_new(BIO_s_rdp_tls());

	if (!bio)
		return NULL;

	ssl = SSL_new(ctx);

	if (!ssl)
	{
		BIO_free(bio);
		return NULL;
	}

	if (client)
		SSL_set_connect_state(ssl);
	else
		SSL_set_accept_state(ssl);

	BIO_set_ssl(bio, ssl, BIO_CLOSE);
	return bio;
}

static CryptoCert tls_get_certificate(rdpTls* tls, BOOL peer)
{
	CryptoCert cert;
	X509* remote_cert;
	STACK_OF(X509) *chain;

	if (peer)
		remote_cert = SSL_get_peer_certificate(tls->ssl);
	else
		remote_cert = X509_dup(SSL_get_certificate(tls->ssl));

	if (!remote_cert)
	{
		WLog_ERR(TAG, "failed to get the server TLS certificate");
		return NULL;
	}

	cert = malloc(sizeof(*cert));

	if (!cert)
	{
		X509_free(remote_cert);
		return NULL;
	}

	cert->px509 = remote_cert;
	/* Get the peer's chain. If it does not exist, we're setting NULL (clean data either way) */
	chain = SSL_get_peer_cert_chain(tls->ssl);
	cert->px509chain = chain;
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
	ContextBindings = (SecPkgContext_Bindings*) calloc(1,
	                  sizeof(SecPkgContext_Bindings));

	if (!ContextBindings)
		return NULL;

	ContextBindings->BindingsLength = sizeof(SEC_CHANNEL_BINDINGS) +
	                                  ChannelBindingTokenLength;
	ChannelBindings = (SEC_CHANNEL_BINDINGS*) calloc(1,
	                  ContextBindings->BindingsLength);

	if (!ChannelBindings)
		goto out_free;

	ContextBindings->Bindings = ChannelBindings;
	ChannelBindings->cbApplicationDataLength = ChannelBindingTokenLength;
	ChannelBindings->dwApplicationDataOffset = sizeof(SEC_CHANNEL_BINDINGS);
	ChannelBindingToken = &((BYTE*)
	                        ChannelBindings)[ChannelBindings->dwApplicationDataOffset];
	strcpy((char*) ChannelBindingToken, TLS_SERVER_END_POINT);
	CopyMemory(&ChannelBindingToken[PrefixLength], CertificateHash,
	           CertificateHashLength);
	return ContextBindings;
out_free:
	free(ContextBindings);
	return NULL;
}

#if OPENSSL_VERSION_NUMBER >= 0x010000000L
static BOOL tls_prepare(rdpTls* tls, BIO* underlying, const SSL_METHOD* method,
                        int options, BOOL clientMode)
#else
static BOOL tls_prepare(rdpTls* tls, BIO* underlying, SSL_METHOD* method,
                        int options, BOOL clientMode)
#endif
{
	rdpSettings* settings = tls->settings;
	tls->ctx = SSL_CTX_new(method);

	if (!tls->ctx)
	{
		WLog_ERR(TAG, "SSL_CTX_new failed");
		return FALSE;
	}

	SSL_CTX_set_mode(tls->ctx,
	                 SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER | SSL_MODE_ENABLE_PARTIAL_WRITE);
	SSL_CTX_set_options(tls->ctx, options);
	SSL_CTX_set_read_ahead(tls->ctx, 1);

	if (settings->AllowedTlsCiphers)
	{
		if (!SSL_CTX_set_cipher_list(tls->ctx, settings->AllowedTlsCiphers))
		{
			WLog_ERR(TAG, "SSL_CTX_set_cipher_list %s failed", settings->AllowedTlsCiphers);
			return FALSE;
		}
	}

	tls->bio = BIO_new_rdp_tls(tls->ctx, clientMode);

	if (BIO_get_ssl(tls->bio, &tls->ssl) < 0)
	{
		WLog_ERR(TAG, "unable to retrieve the SSL of the connection");
		return FALSE;
	}

	BIO_push(tls->bio, underlying);
	tls->underlying = underlying;
	return TRUE;
}

int tls_do_handshake(rdpTls* tls, BOOL clientMode)
{
	CryptoCert cert;
	int verify_status;

	do
	{
#ifdef HAVE_POLL_H
		int fd;
		int status;
		struct pollfd pollfds;
#elif !defined(_WIN32)
		int fd;
		int status;
		fd_set rset;
		struct timeval tv;
#else
		HANDLE event;
		DWORD status;
#endif
		status = BIO_do_handshake(tls->bio);

		if (status == 1)
			break;

		if (!BIO_should_retry(tls->bio))
			return -1;

#ifndef _WIN32
		/* we select() only for read even if we should test both read and write
		 * depending of what have blocked */
		fd = BIO_get_fd(tls->bio, NULL);

		if (fd < 0)
		{
			WLog_ERR(TAG, "unable to retrieve BIO fd");
			return -1;
		}

#else
		BIO_get_event(tls->bio, &event);

		if (!event)
		{
			WLog_ERR(TAG, "unable to retrieve BIO event");
			return -1;
		}

#endif
#ifdef HAVE_POLL_H
		pollfds.fd = fd;
		pollfds.events = POLLIN;
		pollfds.revents = 0;

		do
		{
			status = poll(&pollfds, 1, 10 * 1000);
		}
		while ((status < 0) && (errno == EINTR));

#elif !defined(_WIN32)
		FD_ZERO(&rset);
		FD_SET(fd, &rset);
		tv.tv_sec = 0;
		tv.tv_usec = 10 * 1000; /* 10ms */
		status = _select(fd + 1, &rset, NULL, NULL, &tv);
#else
		status = WaitForSingleObject(event, 10);
#endif
#ifndef _WIN32

		if (status < 0)
		{
			WLog_ERR(TAG, "error during select()");
			return -1;
		}

#else

		if ((status != WAIT_OBJECT_0) && (status != WAIT_TIMEOUT))
		{
			WLog_ERR(TAG, "error during WaitForSingleObject(): 0x%08"PRIX32"", status);
			return -1;
		}

#endif
	}
	while (TRUE);

	cert = tls_get_certificate(tls, clientMode);

	if (!cert)
	{
		WLog_ERR(TAG, "tls_get_certificate failed to return the server certificate.");
		return -1;
	}

	tls->Bindings = tls_get_channel_bindings(cert->px509);

	if (!tls->Bindings)
	{
		WLog_ERR(TAG, "unable to retrieve bindings");
		verify_status = -1;
		goto out;
	}

	if (!crypto_cert_get_public_key(cert, &tls->PublicKey, &tls->PublicKeyLength))
	{
		WLog_ERR(TAG,
		         "crypto_cert_get_public_key failed to return the server public key.");
		verify_status = -1;
		goto out;
	}

	/* server-side NLA needs public keys (keys from us, the server) but no certificate verify */
	verify_status = 1;

	if (clientMode)
	{
		verify_status = tls_verify_certificate(tls, cert, tls->hostname, tls->port);

		if (verify_status < 1)
		{
			WLog_ERR(TAG, "certificate not trusted, aborting.");
			tls_send_alert(tls);
			verify_status = 0;
		}
	}

out:
	tls_free_certificate(cert);
	return verify_status;
}

int tls_connect(rdpTls* tls, BIO* underlying)
{
	int options = 0;
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
	/**
	 * disable SSLv2 and SSLv3
	 */
	options |= SSL_OP_NO_SSLv2;
	options |= SSL_OP_NO_SSLv3;

	if (!tls_prepare(tls, underlying, SSLv23_client_method(), options, TRUE))
		return FALSE;

#ifndef OPENSSL_NO_TLSEXT
	SSL_set_tlsext_host_name(tls->ssl, tls->hostname);
#endif
	return tls_do_handshake(tls, TRUE);
}

#if defined(MICROSOFT_IOS_SNI_BUG) && !defined(OPENSSL_NO_TLSEXT)
static void tls_openssl_tlsext_debug_callback(SSL* s, int client_server,
        int type, unsigned char* data, int len, void* arg)
{
	if (type == TLSEXT_TYPE_server_name)
	{
		WLog_DBG(TAG, "Client uses SNI (extension disabled)");
		s->servername_done = 2;
	}
}
#endif

BOOL tls_accept(rdpTls* tls, BIO* underlying, rdpSettings* settings)
{
	long options = 0;
	BIO* bio;
	RSA* rsa;
	X509* x509;
	/**
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

	if (!tls_prepare(tls, underlying, SSLv23_server_method(), options, FALSE))
		return FALSE;

	if (settings->PrivateKeyFile)
	{
		bio = BIO_new_file(settings->PrivateKeyFile, "rb");

		if (!bio)
		{
			WLog_ERR(TAG, "BIO_new_file failed for private key %s",
			         settings->PrivateKeyFile);
			return FALSE;
		}
	}
	else if (settings->PrivateKeyContent)
	{
		bio = BIO_new_mem_buf(settings->PrivateKeyContent,
		                      strlen(settings->PrivateKeyContent));

		if (!bio)
		{
			WLog_ERR(TAG, "BIO_new_mem_buf failed for private key");
			return FALSE;
		}
	}
	else
	{
		WLog_ERR(TAG, "no private key defined");
		return FALSE;
	}

	rsa = PEM_read_bio_RSAPrivateKey(bio, NULL, NULL, NULL);
	BIO_free(bio);

	if (!rsa)
	{
		WLog_ERR(TAG, "invalid private key");
		return FALSE;
	}

	if (SSL_use_RSAPrivateKey(tls->ssl, rsa) <= 0)
	{
		WLog_ERR(TAG, "SSL_CTX_use_RSAPrivateKey_file failed");
		RSA_free(rsa);
		return FALSE;
	}

	if (settings->CertificateFile)
	{
		bio = BIO_new_file(settings->CertificateFile, "rb");

		if (!bio)
		{
			WLog_ERR(TAG, "BIO_new_file failed for certificate %s",
			         settings->CertificateFile);
			return FALSE;
		}
	}
	else if (settings->CertificateContent)
	{
		bio = BIO_new_mem_buf(settings->CertificateContent,
		                      strlen(settings->CertificateContent));

		if (!bio)
		{
			WLog_ERR(TAG, "BIO_new_mem_buf failed for certificate");
			return FALSE;
		}
	}
	else
	{
		WLog_ERR(TAG, "no certificate defined");
		return FALSE;
	}

	x509 = PEM_read_bio_X509(bio, NULL, NULL, 0);
	BIO_free(bio);

	if (!x509)
	{
		WLog_ERR(TAG, "invalid certificate");
		return FALSE;
	}

	if (SSL_use_certificate(tls->ssl, x509) <= 0)
	{
		WLog_ERR(TAG, "SSL_use_certificate_file failed");
		X509_free(x509);
		return FALSE;
	}

#if defined(MICROSOFT_IOS_SNI_BUG) && !defined(OPENSSL_NO_TLSEXT)
	SSL_set_tlsext_debug_callback(tls->ssl, tls_openssl_tlsext_debug_callback);
#endif
	return tls_do_handshake(tls, FALSE) > 0;
}

BOOL tls_send_alert(rdpTls* tls)
{
	if (!tls)
		return FALSE;

	if (!tls->ssl)
		return TRUE;

	/**
	 * FIXME: The following code does not work on OpenSSL > 1.1.0 because the
	 *        SSL struct is opaqe now
	 */
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)

	if (tls->alertDescription != TLS_ALERT_DESCRIPTION_CLOSE_NOTIFY)
	{
		/**
		 * OpenSSL doesn't really expose an API for sending a TLS alert manually.
		 *
		 * The following code disables the sending of the default "close notify"
		 * and then proceeds to force sending a custom TLS alert before shutting down.
		 *
		 * Manually sending a TLS alert is necessary in certain cases,
		 * like when server-side NLA results in an authentication failure.
		 */
		SSL_SESSION* ssl_session = SSL_get_session(tls->ssl);
		SSL_CTX* ssl_ctx = SSL_get_SSL_CTX(tls->ssl);
		SSL_set_quiet_shutdown(tls->ssl, 1);

		if ((tls->alertLevel == TLS_ALERT_LEVEL_FATAL) && (ssl_session))
			SSL_CTX_remove_session(ssl_ctx, ssl_session);

		tls->ssl->s3->alert_dispatch = 1;
		tls->ssl->s3->send_alert[0] = tls->alertLevel;
		tls->ssl->s3->send_alert[1] = tls->alertDescription;

		if (tls->ssl->s3->wbuf.left == 0)
			tls->ssl->method->ssl_dispatch_alert(tls->ssl);
	}

#endif
	return TRUE;
}

BIO* findBufferedBio(BIO* front)
{
	BIO* ret = front;

	while (ret)
	{
		if (BIO_method_type(ret) == BIO_TYPE_BUFFERED)
			return ret;

		ret = BIO_next(ret);
	}

	return ret;
}

int tls_write_all(rdpTls* tls, const BYTE* data, int length)
{
	int status;
	int offset = 0;
	BIO* bio = tls->bio;

	while (offset < length)
	{
		status = BIO_write(bio, &data[offset], length - offset);

		if (status > 0)
		{
			offset += status;
		}
		else
		{
			if (!BIO_should_retry(bio))
				return -1;

			if (BIO_write_blocked(bio))
				status = BIO_wait_write(bio, 100);
			else if (BIO_read_blocked(bio))
				status = BIO_wait_read(bio, 100);
			else
				USleep(100);

			if (status < 0)
				return -1;
		}
	}

	return length;
}

int tls_set_alert_code(rdpTls* tls, int level, int description)
{
	tls->alertLevel = level;
	tls->alertDescription = description;
	return 0;
}

BOOL tls_match_hostname(char* pattern, int pattern_length, char* hostname)
{
	if (strlen(hostname) == pattern_length)
	{
		if (_strnicmp(hostname, pattern, pattern_length) == 0)
			return TRUE;
	}

	if ((pattern_length > 2) && (pattern[0] == '*') && (pattern[1] == '.')
	    && (((int) strlen(hostname)) >= pattern_length))
	{
		char* check_hostname = &hostname[strlen(hostname) - pattern_length + 1];

		if (_strnicmp(check_hostname, &pattern[1], pattern_length - 1) == 0)
		{
			return TRUE;
		}
	}

	return FALSE;
}

int tls_verify_certificate(rdpTls* tls, CryptoCert cert, char* hostname,
                           int port)
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

	if (tls->settings->ExternalCertificateManagement)
	{
		BIO* bio;
		int status;
		int length;
		int offset;
		BYTE* pemCert;
		freerdp* instance = (freerdp*) tls->settings->instance;
		/**
		 * Don't manage certificates internally, leave it up entirely to the external client implementation
		 */
		bio = BIO_new(BIO_s_mem());

		if (!bio)
		{
			WLog_ERR(TAG, "BIO_new() failure");
			return -1;
		}

		status = PEM_write_bio_X509(bio, cert->px509);

		if (status < 0)
		{
			WLog_ERR(TAG, "PEM_write_bio_X509 failure: %d", status);
			return -1;
		}

		offset = 0;
		length = 2048;
		pemCert = (BYTE*) malloc(length + 1);

		if (!pemCert)
		{
			WLog_ERR(TAG, "error allocating pemCert");
			return -1;
		}

		status = BIO_read(bio, pemCert, length);

		if (status < 0)
		{
			WLog_ERR(TAG, "failed to read certificate");
			return -1;
		}

		offset += status;

		while (offset >= length)
		{
			int new_len;
			BYTE* new_cert;
			new_len = length * 2;
			new_cert = (BYTE*) realloc(pemCert, new_len + 1);

			if (!new_cert)
				return -1;

			length = new_len;
			pemCert = new_cert;
			status = BIO_read(bio, &pemCert[offset], length);

			if (status < 0)
				break;

			offset += status;
		}

		if (status < 0)
		{
			WLog_ERR(TAG, "failed to read certificate");
			return -1;
		}

		length = offset;
		pemCert[length] = '\0';
		status = -1;

		if (instance->VerifyX509Certificate)
			status = instance->VerifyX509Certificate(instance, pemCert, length, hostname,
			         port, tls->isGatewayTransport);
		else
			WLog_ERR(TAG, "No VerifyX509Certificate callback registered!");

		free(pemCert);
		BIO_free(bio);

		if (status < 0)
		{
			WLog_ERR(TAG, "VerifyX509Certificate failed: (length = %d) status: [%d] %s",
			         length, status, pemCert);
			return -1;
		}

		return (status == 0) ? 0 : 1;
	}

	/* ignore certificate verification if user explicitly required it (discouraged) */
	if (tls->settings->IgnoreCertificate)
		return 1;  /* success! */

	/* if user explicitly specified a certificate name, use it instead of the hostname */
	if (tls->settings->CertificateName)
		hostname = tls->settings->CertificateName;

	/* attempt verification using OpenSSL and the ~/.freerdp/certs certificate store */
	certificate_status = x509_verify_certificate(cert,
	                     tls->certificate_store->path);
	/* verify certificate name match */
	certificate_data = crypto_get_certificate_data(cert->px509, hostname, port);
	/* extra common name and alternative names */
	common_name = crypto_cert_subject_common_name(cert->px509, &common_name_length);
	alt_names = crypto_cert_subject_alt_name(cert->px509, &alt_names_count,
	            &alt_names_lengths);

	/* compare against common name */

	if (common_name)
	{
		if (tls_match_hostname(common_name, common_name_length, hostname))
			hostname_match = TRUE;
	}

	/* compare against alternative names */

	if (alt_names)
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
		verification_status = TRUE; /* success! */

	/* verification could not succeed with OpenSSL, use known_hosts file and prompt user for manual verification */
	if (!certificate_status || !hostname_match)
	{
		char* issuer;
		char* subject;
		char* fingerprint;
		freerdp* instance = (freerdp*) tls->settings->instance;
		DWORD accept_certificate = 0;
		issuer = crypto_cert_issuer(cert->px509);
		subject = crypto_cert_subject(cert->px509);
		fingerprint = crypto_cert_fingerprint(cert->px509);
		/* search for matching entry in known_hosts file */
		match = certificate_data_match(tls->certificate_store, certificate_data);

		if (match == 1)
		{
			/* no entry was found in known_hosts file, prompt user for manual verification */
			if (!hostname_match)
				tls_print_certificate_name_mismatch_error(
				    hostname, port,
				    common_name, alt_names,
				    alt_names_count);

			/* Automatically accept certificate on first use */
			if (tls->settings->AutoAcceptCertificate)
			{
				WLog_INFO(TAG, "No certificate stored, automatically accepting.");
				accept_certificate = 1;
			}
			else if (instance->VerifyCertificate)
			{
				accept_certificate = instance->VerifyCertificate(
				                         instance, common_name,
				                         subject, issuer,
				                         fingerprint, !hostname_match);
			}

			switch (accept_certificate)
			{
				case 1:
					/* user accepted certificate, add entry in known_hosts file */
					verification_status = certificate_data_print(tls->certificate_store,
					                      certificate_data);
					break;

				case 2:
					/* user did accept temporaty, do not add to known hosts file */
					verification_status = TRUE;
					break;

				default:
					/* user did not accept, abort and do not add entry in known_hosts file */
					verification_status = FALSE; /* failure! */
					break;
			}
		}
		else if (match == -1)
		{
			char* old_subject = NULL;
			char* old_issuer = NULL;
			char* old_fingerprint = NULL;
			/* entry was found in known_hosts file, but fingerprint does not match. ask user to use it */
			tls_print_certificate_error(hostname, port, fingerprint,
			                            tls->certificate_store->file);

			if (!certificate_get_stored_data(tls->certificate_store,
			                                 certificate_data, &old_subject,
			                                 &old_issuer, &old_fingerprint))
				WLog_WARN(TAG, "Failed to get certificate entry for %s:%d",
				          hostname, port);

			if (instance->VerifyChangedCertificate)
			{
				accept_certificate = instance->VerifyChangedCertificate(
				                         instance, common_name, subject, issuer,
				                         fingerprint, old_subject, old_issuer,
				                         old_fingerprint);
			}

			free(old_subject);
			free(old_issuer);
			free(old_fingerprint);

			switch (accept_certificate)
			{
				case 1:
					/* user accepted certificate, add entry in known_hosts file */
					verification_status = certificate_data_replace(tls->certificate_store,
					                      certificate_data);
					break;

				case 2:
					/* user did accept temporaty, do not add to known hosts file */
					verification_status = TRUE;
					break;

				default:
					/* user did not accept, abort and do not add entry in known_hosts file */
					verification_status = FALSE; /* failure! */
					break;
			}
		}
		else if (match == 0)
			verification_status = TRUE; /* success! */

		free(issuer);
		free(subject);
		free(fingerprint);
	}

	certificate_data_free(certificate_data);
	free(common_name);

	if (alt_names)
		crypto_cert_subject_alt_name_free(alt_names_count, alt_names_lengths,
		                                  alt_names);

	return (verification_status == 0) ? 0 : 1;
}

void tls_print_certificate_error(char* hostname, UINT16 port, char* fingerprint,
                                 char* hosts_file)
{
	WLog_ERR(TAG, "The host key for %s:%"PRIu16" has changed", hostname, port);
	WLog_ERR(TAG, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	WLog_ERR(TAG, "@    WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!     @");
	WLog_ERR(TAG, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	WLog_ERR(TAG, "IT IS POSSIBLE THAT SOMEONE IS DOING SOMETHING NASTY!");
	WLog_ERR(TAG,
	         "Someone could be eavesdropping on you right now (man-in-the-middle attack)!");
	WLog_ERR(TAG, "It is also possible that a host key has just been changed.");
	WLog_ERR(TAG, "The fingerprint for the host key sent by the remote host is%s",
	         fingerprint);
	WLog_ERR(TAG, "Please contact your system administrator.");
	WLog_ERR(TAG, "Add correct host key in %s to get rid of this message.",
	         hosts_file);
	WLog_ERR(TAG,
	         "Host key for %s has changed and you have requested strict checking.",
	         hostname);
	WLog_ERR(TAG, "Host key verification failed.");
}

void tls_print_certificate_name_mismatch_error(char* hostname, UINT16 port,
        char* common_name, char** alt_names,
        int alt_names_count)
{
	int index;
	assert(NULL != hostname);
	WLog_ERR(TAG, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	WLog_ERR(TAG, "@           WARNING: CERTIFICATE NAME MISMATCH!           @");
	WLog_ERR(TAG, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	WLog_ERR(TAG, "The hostname used for this connection (%s:%"PRIu16") ",
	         hostname, port);
	WLog_ERR(TAG, "does not match %s given in the certificate:",
	         alt_names_count < 1 ? "the name" : "any of the names");
	WLog_ERR(TAG, "Common Name (CN):");
	WLog_ERR(TAG, "\t%s", common_name ? common_name : "no CN found in certificate");

	if (alt_names_count > 0)
	{
		assert(NULL != alt_names);
		WLog_ERR(TAG, "Alternative names:");

		for (index = 0; index < alt_names_count; index++)
		{
			assert(alt_names[index]);
			WLog_ERR(TAG, "\t %s", alt_names[index]);
		}
	}

	WLog_ERR(TAG, "A valid certificate for the wrong name should NOT be trusted!");
}

rdpTls* tls_new(rdpSettings* settings)
{
	rdpTls* tls;
	tls = (rdpTls*) calloc(1, sizeof(rdpTls));

	if (!tls)
		return NULL;

	tls->settings = settings;

	if (!settings->ServerMode)
	{
		tls->certificate_store = certificate_store_new(settings);

		if (!tls->certificate_store)
			goto out_free;
	}

	tls->alertLevel = TLS_ALERT_LEVEL_WARNING;
	tls->alertDescription = TLS_ALERT_DESCRIPTION_CLOSE_NOTIFY;
	return tls;
out_free:
	free(tls);
	return NULL;
}

void tls_free(rdpTls* tls)
{
	if (!tls)
		return;

	if (tls->ctx)
	{
		SSL_CTX_free(tls->ctx);
		tls->ctx = NULL;
	}

	if (tls->bio)
	{
		BIO_free(tls->bio);
		tls->bio = NULL;
	}

	if (tls->underlying)
	{
		BIO_free(tls->underlying);
		tls->underlying = NULL;
	}

	if (tls->PublicKey)
	{
		free(tls->PublicKey);
		tls->PublicKey = NULL;
	}

	if (tls->Bindings)
	{
		free(tls->Bindings->Bindings);
		free(tls->Bindings);
		tls->Bindings = NULL;
	}

	if (tls->certificate_store)
	{
		certificate_store_free(tls->certificate_store);
		tls->certificate_store = NULL;
	}

	free(tls);
}

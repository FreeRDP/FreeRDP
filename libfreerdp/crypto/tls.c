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

#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/ssl.h>

#include <winpr/stream.h>
#include <freerdp/utils/ringbuffer.h>

#include <freerdp/log.h>
#include <freerdp/crypto/tls.h>
#include "../core/tcp.h"

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#define TAG FREERDP_TAG("crypto")

struct _BIO_RDP_TLS
{
	SSL* ssl;
};
typedef struct _BIO_RDP_TLS BIO_RDP_TLS;

long bio_rdp_tls_callback(BIO* bio, int mode, const char* argp, int argi, long argl, long ret)
{
	return 1;
}

static int bio_rdp_tls_write(BIO* bio, const char* buf, int size)
{
	int error;
	int status;
	BIO_RDP_TLS* tls = (BIO_RDP_TLS*) bio->ptr;

	if (!buf || !tls)
		return 0;

	BIO_clear_flags(bio, BIO_FLAGS_WRITE | BIO_FLAGS_READ | BIO_FLAGS_IO_SPECIAL);

	status = SSL_write(tls->ssl, buf, size);

	if (status <= 0)
	{
		switch (SSL_get_error(tls->ssl, status))
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
				bio->retry_reason = BIO_RR_SSL_X509_LOOKUP;
				break;

			case SSL_ERROR_WANT_CONNECT:
				BIO_set_flags(bio, BIO_FLAGS_IO_SPECIAL);
				bio->retry_reason = BIO_RR_CONNECT;
				break;

			case SSL_ERROR_SYSCALL:
				error = WSAGetLastError();
				if ((error == WSAEWOULDBLOCK) || (error == WSAEINTR) ||
					(error == WSAEINPROGRESS) || (error == WSAEALREADY))
				{
					BIO_set_flags(bio, (BIO_FLAGS_WRITE | BIO_FLAGS_SHOULD_RETRY));
				}
				else
				{
					BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
				}
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
	BIO_RDP_TLS* tls = (BIO_RDP_TLS*) bio->ptr;

	if (!buf || !tls)
		return 0;

	BIO_clear_flags(bio, BIO_FLAGS_WRITE | BIO_FLAGS_READ | BIO_FLAGS_IO_SPECIAL);

	status = SSL_read(tls->ssl, buf, size);

	if (status <= 0)
	{
		switch (SSL_get_error(tls->ssl, status))
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
				bio->retry_reason = BIO_RR_SSL_X509_LOOKUP;
				break;

			case SSL_ERROR_WANT_ACCEPT:
				BIO_set_flags(bio, BIO_FLAGS_IO_SPECIAL);
				bio->retry_reason = BIO_RR_ACCEPT;
				break;

			case SSL_ERROR_WANT_CONNECT:
				BIO_set_flags(bio, BIO_FLAGS_IO_SPECIAL);
				bio->retry_reason = BIO_RR_CONNECT;
				break;

			case SSL_ERROR_SSL:
				BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
				break;

			case SSL_ERROR_ZERO_RETURN:
				BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
				break;

			case SSL_ERROR_SYSCALL:
				error = WSAGetLastError();
				if ((error == WSAEWOULDBLOCK) || (error == WSAEINTR) ||
					(error == WSAEINPROGRESS) || (error == WSAEALREADY))
				{
					BIO_set_flags(bio, (BIO_FLAGS_READ | BIO_FLAGS_SHOULD_RETRY));
				}
				else
				{
					BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
				}
				break;
		}
	}

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
	BIO* rbio;
	int status = -1;
	BIO_RDP_TLS* tls = (BIO_RDP_TLS*) bio->ptr;

	if (!tls)
		return 0;

	if (!tls->ssl && (cmd != BIO_C_SET_SSL))
		return 0;

	switch (cmd)
	{
		case BIO_CTRL_RESET:
			SSL_shutdown(tls->ssl);

			if (tls->ssl->handshake_func == tls->ssl->method->ssl_connect)
				SSL_set_connect_state(tls->ssl);
			else if (tls->ssl->handshake_func == tls->ssl->method->ssl_accept)
				SSL_set_accept_state(tls->ssl);

			SSL_clear(tls->ssl);

			if (bio->next_bio)
				status = BIO_ctrl(bio->next_bio, cmd, num, ptr);
			else if (tls->ssl->rbio)
				status = BIO_ctrl(tls->ssl->rbio, cmd, num, ptr);
			else
				status = 1;
			break;

		case BIO_C_GET_FD:
			status = BIO_ctrl(tls->ssl->rbio, cmd, num, ptr);
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
			status = bio->shutdown;
			break;

		case BIO_CTRL_SET_CLOSE:
			bio->shutdown = (int) num;
			status = 1;
			break;

		case BIO_CTRL_WPENDING:
			status = BIO_ctrl(tls->ssl->wbio, cmd, num, ptr);
			break;

		case BIO_CTRL_PENDING:
			status = SSL_pending(tls->ssl);
			if (status == 0)
				status = BIO_pending(tls->ssl->rbio);
			break;

		case BIO_CTRL_FLUSH:
			BIO_clear_retry_flags(bio);
			status = BIO_ctrl(tls->ssl->wbio, cmd, num, ptr);
			BIO_copy_next_retry(bio);
			status = 1;
			break;

		case BIO_CTRL_PUSH:
			if (bio->next_bio && (bio->next_bio != tls->ssl->rbio))
			{
				SSL_set_bio(tls->ssl, bio->next_bio, bio->next_bio);
				CRYPTO_add(&(bio->next_bio->references), 1, CRYPTO_LOCK_BIO);
			}
			status = 1;
			break;

		case BIO_CTRL_POP:
			if (bio == ptr)
			{
				if (tls->ssl->rbio != tls->ssl->wbio)
					BIO_free_all(tls->ssl->wbio);

				if (bio->next_bio)
					CRYPTO_add(&(bio->next_bio->references), -1, CRYPTO_LOCK_BIO);

				tls->ssl->wbio = tls->ssl->rbio = NULL;
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
			bio->shutdown = (int) num;

			if (ptr)
				tls->ssl = (SSL*) ptr;

			rbio = SSL_get_rbio(tls->ssl);

			if (rbio)
			{
				if (bio->next_bio)
					BIO_push(rbio, bio->next_bio);

				bio->next_bio = rbio;
				CRYPTO_add(&(rbio->references), 1, CRYPTO_LOCK_BIO);
			}

			bio->init = 1;
			status = 1;
			break;

		case BIO_C_DO_STATE_MACHINE:
			BIO_clear_flags(bio, BIO_FLAGS_READ | BIO_FLAGS_WRITE | BIO_FLAGS_IO_SPECIAL);
			bio->retry_reason = 0;

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
						bio->retry_reason = bio->next_bio->retry_reason;
						break;

					default:
						BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY);
						break;
				}
			}
			break;

		default:
			status = BIO_ctrl(tls->ssl->rbio, cmd, num, ptr);
			break;
	}

	return status;
}

static int bio_rdp_tls_new(BIO* bio)
{
	BIO_RDP_TLS* tls;

	bio->init = 0;
	bio->num = 0;
	bio->flags = BIO_FLAGS_SHOULD_RETRY;
	bio->next_bio = NULL;

	tls = calloc(1, sizeof(BIO_RDP_TLS));

	if (!tls)
		return 0;

	bio->ptr = (void*) tls;

	return 1;
}

static int bio_rdp_tls_free(BIO* bio)
{
	BIO_RDP_TLS* tls;

	if (!bio)
		return 0;

	tls = (BIO_RDP_TLS*) bio->ptr;

	if (!tls)
		return 0;

	if (bio->shutdown)
	{
		if (bio->init && tls->ssl)
		{
			SSL_shutdown(tls->ssl);
			SSL_free(tls->ssl);
		}

		bio->init = 0;
		bio->flags = 0;
	}

	free(tls);

	return 1;
}

static long bio_rdp_tls_callback_ctrl(BIO* bio, int cmd, bio_info_cb* fp)
{
	int status = 0;
	BIO_RDP_TLS* tls;

	if (!bio)
		return 0;

	tls = (BIO_RDP_TLS*) bio->ptr;

	if (!tls)
		return 0;

	switch (cmd)
	{
		case BIO_CTRL_SET_CALLBACK:
			SSL_set_info_callback(tls->ssl, (void (*)(const SSL *, int, int)) fp);
			status = 1;
			break;

		default:
			status = BIO_callback_ctrl(tls->ssl->rbio, cmd, fp);
			break;
	}

	return status;
}

#define BIO_TYPE_RDP_TLS	68

static BIO_METHOD bio_rdp_tls_methods =
{
	BIO_TYPE_RDP_TLS,
	"RdpTls",
	bio_rdp_tls_write,
	bio_rdp_tls_read,
	bio_rdp_tls_puts,
	bio_rdp_tls_gets,
	bio_rdp_tls_ctrl,
	bio_rdp_tls_new,
	bio_rdp_tls_free,
	bio_rdp_tls_callback_ctrl,
};

BIO_METHOD* BIO_s_rdp_tls(void)
{
	return &bio_rdp_tls_methods;
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

	if (peer)
		remote_cert = SSL_get_peer_certificate(tls->ssl);
	else
		remote_cert = X509_dup( SSL_get_certificate(tls->ssl) );

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

	ContextBindings = (SecPkgContext_Bindings*) calloc(1, sizeof(SecPkgContext_Bindings));
	if (!ContextBindings)
		return NULL;

	ContextBindings->BindingsLength = sizeof(SEC_CHANNEL_BINDINGS) + ChannelBindingTokenLength;
	ChannelBindings = (SEC_CHANNEL_BINDINGS*) calloc(1, ContextBindings->BindingsLength);
	if (!ChannelBindings)
		goto out_free;
	ContextBindings->Bindings = ChannelBindings;

	ChannelBindings->cbApplicationDataLength = ChannelBindingTokenLength;
	ChannelBindings->dwApplicationDataOffset = sizeof(SEC_CHANNEL_BINDINGS);
	ChannelBindingToken = &((BYTE*) ChannelBindings)[ChannelBindings->dwApplicationDataOffset];

	strcpy((char*) ChannelBindingToken, TLS_SERVER_END_POINT);
	CopyMemory(&ChannelBindingToken[PrefixLength], CertificateHash, CertificateHashLength);

	return ContextBindings;

out_free:
	free(ContextBindings);
	return NULL;
}


#if defined(__APPLE__)
BOOL tls_prepare(rdpTls* tls, BIO *underlying, SSL_METHOD *method, int options, BOOL clientMode)
#else
BOOL tls_prepare(rdpTls* tls, BIO *underlying, const SSL_METHOD *method, int options, BOOL clientMode)
#endif
{
	rdpSettings* settings = tls->settings;

	tls->ctx = SSL_CTX_new(method);

	if (!tls->ctx)
	{
		WLog_ERR(TAG, "SSL_CTX_new failed");
		return FALSE;
	}

	SSL_CTX_set_mode(tls->ctx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER | SSL_MODE_ENABLE_PARTIAL_WRITE);

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

	return TRUE;
}

int tls_do_handshake(rdpTls* tls, BOOL clientMode)
{
	CryptoCert cert;
	int verify_status, status;

	do
	{
#ifdef HAVE_POLL_H
		struct pollfd pollfds;
#else
		struct timeval tv;
		fd_set rset;
#endif
		int fd;

		status = BIO_do_handshake(tls->bio);

		if (status == 1)
			break;

		if (!BIO_should_retry(tls->bio))
			return -1;

		/* we select() only for read even if we should test both read and write
		 * depending of what have blocked */
		fd = BIO_get_fd(tls->bio, NULL);

		if (fd < 0)
		{
			WLog_ERR(TAG, "unable to retrieve BIO fd");
			return -1;
		}

#ifdef HAVE_POLL_H
		pollfds.fd = fd;
		pollfds.events = POLLIN;
		pollfds.revents = 0;

		do
		{
			status = poll(&pollfds, 1, 10 * 1000);
		}
		while ((status < 0) && (errno == EINTR));
#else
		FD_ZERO(&rset);
		FD_SET(fd, &rset);
		tv.tv_sec = 0;
		tv.tv_usec = 10 * 1000; /* 10ms */

		status = _select(fd + 1, &rset, NULL, NULL, &tv);
#endif
		if (status < 0)
		{
			WLog_ERR(TAG, "error during select()");
			return -1;
		}
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
		WLog_ERR(TAG, "crypto_cert_get_public_key failed to return the server public key.");
		verify_status = -1;
		goto out;
	}

	/* Note: server-side NLA needs public keys (keys from us, the server) but no
	 * 		certificate verify
	 */
	verify_status = 1;
	if (clientMode)
	{
		verify_status = tls_verify_certificate(tls, cert, tls->hostname, tls->port);

		if (verify_status < 1)
		{
			WLog_ERR(TAG, "certificate not trusted, aborting.");
			tls_disconnect(tls);
			verify_status = 0;
		}
	}

out:
	tls_free_certificate(cert);

	return verify_status;
}

int tls_connect(rdpTls* tls, BIO *underlying)
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

	if (!tls_prepare(tls, underlying, TLSv1_client_method(), options, TRUE))
		return FALSE;

	return tls_do_handshake(tls, TRUE);
}

BOOL tls_accept(rdpTls* tls, BIO *underlying, const char* cert_file, const char* privatekey_file)
{
	long options = 0;

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

	if (SSL_use_RSAPrivateKey_file(tls->ssl, privatekey_file, SSL_FILETYPE_PEM) <= 0)
	{
		WLog_ERR(TAG, "SSL_CTX_use_RSAPrivateKey_file failed");
		WLog_ERR(TAG, "PrivateKeyFile: %s", privatekey_file);
		return FALSE;
	}

	if (SSL_use_certificate_file(tls->ssl, cert_file, SSL_FILETYPE_PEM) <= 0)
	{
		WLog_ERR(TAG, "SSL_use_certificate_file failed");
		return FALSE;
	}

	return tls_do_handshake(tls, FALSE) > 0;
}

BOOL tls_disconnect(rdpTls* tls)
{
	if (!tls)
		return FALSE;

	if (!tls->ssl)
		return TRUE;

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

		SSL_set_quiet_shutdown(tls->ssl, 1);

		if ((tls->alertLevel == TLS_ALERT_LEVEL_FATAL) && (tls->ssl->session))
			SSL_CTX_remove_session(tls->ssl->ctx, tls->ssl->session);

		tls->ssl->s3->alert_dispatch = 1;
		tls->ssl->s3->send_alert[0] = tls->alertLevel;
		tls->ssl->s3->send_alert[1] = tls->alertDescription;

		if (tls->ssl->s3->wbuf.left == 0)
			tls->ssl->method->ssl_dispatch_alert(tls->ssl);

		SSL_shutdown(tls->ssl);
	}
	else
	{
		SSL_shutdown(tls->ssl);
	}

	return TRUE;
}


BIO *findBufferedBio(BIO *front)
{
	BIO *ret = front;

	while (ret)
	{
		if (BIO_method_type(ret) == BIO_TYPE_BUFFERED)
			return ret;
		ret = ret->next_bio;
	}

	return ret;
}

int tls_write_all(rdpTls* tls, const BYTE* data, int length)
{
	int i;
	int status;
	int nchunks;
	int committedBytes;
	rdpTcp *tcp;
#ifdef HAVE_POLL_H
	struct pollfd pollfds;
#else
	fd_set rset, wset;
	fd_set *rsetPtr, *wsetPtr;
	struct timeval tv;
#endif
	BIO* bio = tls->bio;
	DataChunk chunks[2];

	BIO* bufferedBio = findBufferedBio(bio);

	if (!bufferedBio)
	{
		WLog_ERR(TAG, "error unable to retrieve the bufferedBio in the BIO chain");
		return -1;
	}

	tcp = (rdpTcp*) bufferedBio->ptr;

	do
	{
		status = BIO_write(bio, data, length);

		if (status > 0)
			break;

		if (!BIO_should_retry(bio))
			return -1;
		
#ifdef HAVE_POLL_H
		pollfds.fd = tcp->sockfd;
		pollfds.revents = 0;
		pollfds.events = 0;

		if (tcp->writeBlocked)
		{
			pollfds.events |= POLLOUT;
		}
		else if (tcp->readBlocked)
		{
			pollfds.events |= POLLIN;
		}
		else
		{
			WLog_ERR(TAG, "weird we're blocked but the underlying is not read or write blocked !");
			USleep(10);
			continue;
		}

		do
		{
			status = poll(&pollfds, 1, 100);
		}
		while ((status < 0) && (errno == EINTR));
#else
		/* we try to handle SSL want_read and want_write nicely */
		rsetPtr = wsetPtr = NULL;

		if (tcp->writeBlocked)
		{
			wsetPtr = &wset;
			FD_ZERO(&wset);
			FD_SET(tcp->sockfd, &wset);
		}
		else if (tcp->readBlocked)
		{
			rsetPtr = &rset;
			FD_ZERO(&rset);
			FD_SET(tcp->sockfd, &rset);
		}
		else
		{
			WLog_ERR(TAG, "weird we're blocked but the underlying is not read or write blocked !");
			USleep(10);
			continue;
		}

		tv.tv_sec = 0;
		tv.tv_usec = 100 * 1000;

		status = _select(tcp->sockfd + 1, rsetPtr, wsetPtr, NULL, &tv);
#endif
		if (status < 0)
			return -1;
	}
	while (TRUE);

	/* make sure the output buffer is empty */
	
	do
	{
		committedBytes = 0;
		
		if (ringbuffer_used(&tcp->xmitBuffer) < 1)
			break;
		
		nchunks = ringbuffer_peek(&tcp->xmitBuffer, chunks, ringbuffer_used(&tcp->xmitBuffer));
		
		if (nchunks < 1)
			break;
		
		for (i = 0; i < nchunks; i++)
		{
			while (chunks[i].size)
			{
				status = BIO_write(tcp->socketBio, chunks[i].data, chunks[i].size);

				if (status > 0)
				{
					chunks[i].size -= status;
					chunks[i].data += status;
					committedBytes += status;
					continue;
				}

				if (!BIO_should_retry(tcp->socketBio))
					goto out_fail;

#ifdef HAVE_POLL_H
				pollfds.fd = tcp->sockfd;
				pollfds.events = POLLIN;
				pollfds.revents = 0;

				do
				{
					status = poll(&pollfds, 1, 100);
				}
				while ((status < 0) && (errno == EINTR));
#else
				FD_ZERO(&rset);
				FD_SET(tcp->sockfd, &rset);
				tv.tv_sec = 0;
				tv.tv_usec = 100 * 1000;

				status = _select(tcp->sockfd + 1, &rset, NULL, NULL, &tv);
#endif
				if (status < 0)
					goto out_fail;
			}

		}
		
		ringbuffer_commit_read_bytes(&tcp->xmitBuffer, committedBytes);
		continue;
		
out_fail:
		ringbuffer_commit_read_bytes(&tcp->xmitBuffer, committedBytes);
		return -1;
	}
	while (TRUE);

	return length;
}

int tls_set_alert_code(rdpTls* tls, int level, int description)
{
	tls->alertLevel = level;
	tls->alertDescription = description;

	return 0;
}

BOOL tls_match_hostname(char *pattern, int pattern_length, char *hostname)
{
	if (strlen(hostname) == pattern_length)
	{
		if (memcmp((void*) hostname, (void*) pattern, pattern_length) == 0)
			return TRUE;
	}

	if ((pattern_length > 2) && (pattern[0] == '*') && (pattern[1] == '.') && (((int) strlen(hostname)) >= pattern_length))
	{
		char* check_hostname = &hostname[strlen(hostname) - pattern_length + 1];

		if (memcmp((void*) check_hostname, (void*) &pattern[1], pattern_length - 1) == 0)
		{
			return TRUE;
		}
	}

	return FALSE;
}

int tls_verify_certificate(rdpTls* tls, CryptoCert cert, char* hostname, int port)
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

		status = BIO_read(bio, pemCert, length);
		
		if (status < 0)
		{
			WLog_ERR(TAG, "failed to read certificate");
			return -1;
		}
		
		offset += status;

		while (offset >= length)
		{
			length *= 2;
			pemCert = (BYTE*) realloc(pemCert, length + 1);

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
		{
			status = instance->VerifyX509Certificate(instance, pemCert, length, hostname, port, tls->isGatewayTransport);
		}
		
		WLog_ERR(TAG, "(length = %d) status: %d%s",	length, status, pemCert);

		free(pemCert);
		BIO_free(bio);

		if (status < 0)
			return -1;

		return (status == 0) ? 0 : 1;
	}

	/* ignore certificate verification if user explicitly required it (discouraged) */
	if (tls->settings->IgnoreCertificate)
		return 1;  /* success! */

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
			{
				accept_certificate = instance->VerifyCertificate(instance, subject, issuer, fingerprint);
			}

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
			{
				accept_certificate = instance->VerifyChangedCertificate(instance, subject, issuer, fingerprint, "");
			}

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

	if (alt_names)
		crypto_cert_subject_alt_name_free(alt_names_count, alt_names_lengths,
				alt_names);

	return (verification_status == 0) ? 0 : 1;
}

void tls_print_certificate_error(char* hostname, char* fingerprint, char *hosts_file)
{
	WLog_ERR(TAG, "The host key for %s has changed", hostname);
	WLog_ERR(TAG, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	WLog_ERR(TAG, "@    WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!     @");
	WLog_ERR(TAG, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	WLog_ERR(TAG, "IT IS POSSIBLE THAT SOMEONE IS DOING SOMETHING NASTY!");
	WLog_ERR(TAG, "Someone could be eavesdropping on you right now (man-in-the-middle attack)!");
	WLog_ERR(TAG, "It is also possible that a host key has just been changed.");
	WLog_ERR(TAG, "The fingerprint for the host key sent by the remote host is%s", fingerprint);
	WLog_ERR(TAG, "Please contact your system administrator.");
	WLog_ERR(TAG, "Add correct host key in %s to get rid of this message.", hosts_file);
	WLog_ERR(TAG, "Host key for %s has changed and you have requested strict checking.", hostname);
	WLog_ERR(TAG, "Host key verification failed.");
}

void tls_print_certificate_name_mismatch_error(char* hostname, char* common_name, char** alt_names, int alt_names_count)
{
	int index;

	assert(NULL != hostname);
	WLog_ERR(TAG, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	WLog_ERR(TAG, "@           WARNING: CERTIFICATE NAME MISMATCH!           @");
	WLog_ERR(TAG, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	WLog_ERR(TAG, "The hostname used for this connection (%s) ", hostname);
	WLog_ERR(TAG, "does not match %s given in the certificate:", alt_names_count < 1 ? "the name" : "any of the names");
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

	winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);

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

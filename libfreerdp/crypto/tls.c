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

#include <freerdp/config.h>

#include <winpr/assert.h>
#include <string.h>
#include <errno.h>

#include <winpr/crt.h>
#include <winpr/string.h>
#include <winpr/sspi.h>
#include <winpr/ssl.h>

#include <winpr/stream.h>
#include <freerdp/utils/ringbuffer.h>

#include <freerdp/log.h>
#include <freerdp/crypto/tls.h>
#include "../core/tcp.h"
#include "opensslcompat.h"

#ifdef WINPR_HAVE_POLL_H
#include <poll.h>
#endif

#ifdef FREERDP_HAVE_VALGRIND_MEMCHECK_H
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

typedef struct
{
	SSL* ssl;
	CRITICAL_SECTION lock;
} BIO_RDP_TLS;

static int tls_verify_certificate(rdpTls* tls, CryptoCert cert, const char* hostname, UINT16 port);
static void tls_print_certificate_name_mismatch_error(const char* hostname, UINT16 port,
                                                      const char* common_name, char** alt_names,
                                                      int alt_names_count);
static void tls_print_certificate_error(const char* hostname, UINT16 port, const char* fingerprint,
                                        const char* hosts_file);

static int bio_rdp_tls_write(BIO* bio, const char* buf, int size)
{
	int error;
	int status;
	BIO_RDP_TLS* tls = (BIO_RDP_TLS*)BIO_get_data(bio);

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
	BIO_RDP_TLS* tls = (BIO_RDP_TLS*)BIO_get_data(bio);

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

#ifdef FREERDP_HAVE_VALGRIND_MEMCHECK_H

	if (status > 0)
	{
		VALGRIND_MAKE_MEM_DEFINED(buf, status);
	}

#endif
	return status;
}

static int bio_rdp_tls_puts(BIO* bio, const char* str)
{
	size_t size;
	int status;

	if (!str)
		return 0;

	size = strlen(str);
	ERR_clear_error();
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
	BIO_RDP_TLS* tls = (BIO_RDP_TLS*)BIO_get_data(bio);

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
			*((ULONG_PTR*)ptr) = (ULONG_PTR)SSL_get_info_callback(tls->ssl);
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
			BIO_set_shutdown(bio, (int)num);
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
			if (status != 1)
				WLog_DBG(TAG, "BIO_ctrl returned %d", status);
			BIO_copy_next_retry(bio);
			status = 1;
			break;

		case BIO_CTRL_PUSH:
			if (next_bio && (next_bio != ssl_rbio))
			{
#if OPENSSL_VERSION_NUMBER < 0x10100000L || \
    (defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER < 0x2070000fL)
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

#if OPENSSL_VERSION_NUMBER < 0x10100000L || \
    (defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER < 0x2070000fL)

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
				*((SSL**)ptr) = tls->ssl;
				status = 1;
			}

			break;

		case BIO_C_SET_SSL:
			BIO_set_shutdown(bio, (int)num);

			if (ptr)
			{
				tls->ssl = (SSL*)ptr;
				ssl_rbio = SSL_get_rbio(tls->ssl);
			}

			if (ssl_rbio)
			{
				if (next_bio)
					BIO_push(ssl_rbio, next_bio);

				BIO_set_next(bio, ssl_rbio);
#if OPENSSL_VERSION_NUMBER < 0x10100000L || \
    (defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER < 0x2070000fL)
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
	BIO_set_data(bio, (void*)tls);
	return 1;
}

static int bio_rdp_tls_free(BIO* bio)
{
	BIO_RDP_TLS* tls;

	if (!bio)
		return 0;

	tls = (BIO_RDP_TLS*)BIO_get_data(bio);

	if (!tls)
		return 0;

	BIO_set_data(bio, NULL);
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
	long status = 0;
	BIO_RDP_TLS* tls;

	if (!bio)
		return 0;

	tls = (BIO_RDP_TLS*)BIO_get_data(bio);

	if (!tls)
		return 0;

	switch (cmd)
	{
		case BIO_CTRL_SET_CALLBACK:
		{
			typedef void (*fkt_t)(const SSL*, int, int);
			/* Documented since https://www.openssl.org/docs/man1.1.1/man3/BIO_set_callback.html
			 * the argument is not really of type bio_info_cb* and must be cast
			 * to the required type */
			fkt_t fkt = (fkt_t)(void*)fp;
			SSL_set_info_callback(tls->ssl, fkt);
			status = 1;
		}
		break;

		default:
			status = BIO_callback_ctrl(SSL_get_rbio(tls->ssl), cmd, fp);
			break;
	}

	return status;
}

#define BIO_TYPE_RDP_TLS 68

static BIO_METHOD* BIO_s_rdp_tls(void)
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

static BIO* BIO_new_rdp_tls(SSL_CTX* ctx, int client)
{
	BIO* bio;
	SSL* ssl;
	bio = BIO_new(BIO_s_rdp_tls());

	if (!bio)
		return NULL;

	ssl = SSL_new(ctx);

	if (!ssl)
	{
		BIO_free_all(bio);
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
	STACK_OF(X509) * chain;

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

#define TLS_SERVER_END_POINT "tls-server-end-point:"

static SecPkgContext_Bindings* tls_get_channel_bindings(X509* cert)
{
	UINT32 CertificateHashLength;
	BYTE* ChannelBindingToken;
	UINT32 ChannelBindingTokenLength;
	SEC_CHANNEL_BINDINGS* ChannelBindings;
	SecPkgContext_Bindings* ContextBindings;
	const size_t PrefixLength = strnlen(TLS_SERVER_END_POINT, ARRAYSIZE(TLS_SERVER_END_POINT));

	/* See https://www.rfc-editor.org/rfc/rfc5929 for details about hashes */
	WINPR_MD_TYPE alg = crypto_cert_get_signature_alg(cert);
	const char* hash;
	switch (alg)
	{

		case WINPR_MD_MD5:
		case WINPR_MD_SHA1:
			hash = winpr_md_type_to_string(WINPR_MD_SHA256);
			break;
		default:
			hash = winpr_md_type_to_string(alg);
			break;
	}
	if (!hash)
		return NULL;

	BYTE* CertificateHash = crypto_cert_hash(cert, hash, &CertificateHashLength);
	if (!CertificateHash)
		return NULL;

	ChannelBindingTokenLength = PrefixLength + CertificateHashLength;
	ContextBindings = (SecPkgContext_Bindings*)calloc(1, sizeof(SecPkgContext_Bindings));

	if (!ContextBindings)
		goto out_free;

	ContextBindings->BindingsLength = sizeof(SEC_CHANNEL_BINDINGS) + ChannelBindingTokenLength;
	ChannelBindings = (SEC_CHANNEL_BINDINGS*)calloc(1, ContextBindings->BindingsLength);

	if (!ChannelBindings)
		goto out_free;

	ContextBindings->Bindings = ChannelBindings;
	ChannelBindings->cbApplicationDataLength = ChannelBindingTokenLength;
	ChannelBindings->dwApplicationDataOffset = sizeof(SEC_CHANNEL_BINDINGS);
	ChannelBindingToken = &((BYTE*)ChannelBindings)[ChannelBindings->dwApplicationDataOffset];
	memcpy(ChannelBindingToken, TLS_SERVER_END_POINT, PrefixLength);
	memcpy(ChannelBindingToken + PrefixLength, CertificateHash, CertificateHashLength);
	free(CertificateHash);
	return ContextBindings;
out_free:
	free(CertificateHash);
	free(ContextBindings);
	return NULL;
}

static INIT_ONCE secrets_file_idx_once = INIT_ONCE_STATIC_INIT;
static int secrets_file_idx = -1;

static BOOL CALLBACK secrets_file_init_cb(PINIT_ONCE once, PVOID param, PVOID* context)
{
	secrets_file_idx = SSL_get_ex_new_index(0, NULL, NULL, NULL, NULL);

	return (secrets_file_idx != -1);
}

static void SSLCTX_keylog_cb(const SSL* ssl, const char* line)
{
	char* dfile;

	if (secrets_file_idx == -1)
		return;

	dfile = SSL_get_ex_data(ssl, secrets_file_idx);
	if (dfile)
	{
		FILE* f = fopen(dfile, "a+");
		fwrite(line, strlen(line), 1, f);
		fwrite("\n", 1, 1, f);
		fclose(f);
	}
}

#if OPENSSL_VERSION_NUMBER >= 0x010000000L
static BOOL tls_prepare(rdpTls* tls, BIO* underlying, const SSL_METHOD* method, int options,
                        BOOL clientMode)
#else
static BOOL tls_prepare(rdpTls* tls, BIO* underlying, SSL_METHOD* method, int options,
                        BOOL clientMode)
#endif
{
	rdpSettings* settings = tls->settings;
	tls->ctx = SSL_CTX_new(method);

	tls->underlying = underlying;

	if (!tls->ctx)
	{
		WLog_ERR(TAG, "SSL_CTX_new failed");
		return FALSE;
	}

	SSL_CTX_set_mode(tls->ctx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER | SSL_MODE_ENABLE_PARTIAL_WRITE);
	SSL_CTX_set_options(tls->ctx, options);
	SSL_CTX_set_read_ahead(tls->ctx, 1);
#if OPENSSL_VERSION_NUMBER >= 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
	UINT16 version = freerdp_settings_get_uint16(settings, FreeRDP_TLSMinVersion);
	if (!SSL_CTX_set_min_proto_version(tls->ctx, version))
	{
		WLog_ERR(TAG, "SSL_CTX_set_min_proto_version %s failed", version);
		return FALSE;
	}
	version = freerdp_settings_get_uint16(settings, FreeRDP_TLSMaxVersion);
	if (!SSL_CTX_set_max_proto_version(tls->ctx, version))
	{
		WLog_ERR(TAG, "SSL_CTX_set_max_proto_version %s failed", version);
		return FALSE;
	}
#endif
#if OPENSSL_VERSION_NUMBER >= 0x10100000L && !defined(LIBRESSL_VERSION_NUMBER)
	SSL_CTX_set_security_level(tls->ctx, settings->TlsSecLevel);
#endif

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

	if (settings->TlsSecretsFile)
	{
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
		InitOnceExecuteOnce(&secrets_file_idx_once, secrets_file_init_cb, NULL, NULL);

		if (secrets_file_idx != -1)
		{
			SSL_set_ex_data(tls->ssl, secrets_file_idx, settings->TlsSecretsFile);
			SSL_CTX_set_keylog_callback(tls->ctx, SSLCTX_keylog_cb);
		}
#else
		WLog_WARN(TAG, "Key-Logging not available - requires OpenSSL 1.1.1 or higher");
#endif
	}

	BIO_push(tls->bio, underlying);
	return TRUE;
}

static void adjustSslOptions(int* options)
{
	WINPR_ASSERT(options);
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
	*options |= SSL_OP_NO_SSLv2;
	*options |= SSL_OP_NO_SSLv3;
#endif
}

const SSL_METHOD* tls_get_ssl_method(BOOL isDtls, BOOL isClient)
{
	if (isClient)
	{
		if (isDtls)
			return (const SSL_METHOD*)DTLS_client_method();
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
		return (const SSL_METHOD*)SSLv23_client_method();
#else
		return (const SSL_METHOD*)TLS_client_method();
#endif
	}

	if (isDtls)
		return (const SSL_METHOD*)DTLS_server_method();

	return (const SSL_METHOD*)SSLv23_server_method();
}

TlsHandshakeResult tls_connect_ex(rdpTls* tls, BIO* underlying, const SSL_METHOD* methods)
{
	WINPR_ASSERT(tls);

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

	tls->isClientMode = TRUE;
	adjustSslOptions(&options);

	if (!tls_prepare(tls, underlying, methods, options, TRUE))
		return 0;

#if !defined(OPENSSL_NO_TLSEXT) && !defined(LIBRESSL_VERSION_NUMBER)
	SSL_set_tlsext_host_name(tls->ssl, tls->hostname);
#endif

	return tls_handshake(tls);
}

TlsHandshakeResult tls_handshake(rdpTls* tls)
{
	TlsHandshakeResult ret = TLS_HANDSHAKE_ERROR;

	WINPR_ASSERT(tls);
	int status = BIO_do_handshake(tls->bio);
	if (status != 1)
	{
		if (!BIO_should_retry(tls->bio))
			return TLS_HANDSHAKE_ERROR;

		return TLS_HANDSHAKE_CONTINUE;
	}

	int verify_status;
	CryptoCert cert = tls_get_certificate(tls, tls->isClientMode);

	if (!cert)
	{
		WLog_ERR(TAG, "tls_get_certificate failed to return the server certificate.");
		return TLS_HANDSHAKE_ERROR;
	}

	do
	{
		tls->Bindings = tls_get_channel_bindings(cert->px509);
		if (!tls->Bindings)
		{
			WLog_ERR(TAG, "unable to retrieve bindings");
			break;
		}

		if (!crypto_cert_get_public_key(cert, &tls->PublicKey, &tls->PublicKeyLength))
		{
			WLog_ERR(TAG, "crypto_cert_get_public_key failed to return the server public key.");
			break;
		}

		/* server-side NLA needs public keys (keys from us, the server) but no certificate verify */
		ret = TLS_HANDSHAKE_SUCCESS;

		if (tls->isClientMode)
		{
			verify_status = tls_verify_certificate(tls, cert, tls->hostname, tls->port);

			if (verify_status < 1)
			{
				WLog_ERR(TAG, "certificate not trusted, aborting.");
				tls_send_alert(tls);
				ret = TLS_HANDSHAKE_VERIFY_ERROR;
			}
		}
	} while (0);

	tls_free_certificate(cert);
	return ret;
}

static int pollAndHandshake(rdpTls* tls)
{
	WINPR_ASSERT(tls);

	do
	{
		HANDLE event;
		DWORD status;
		if (BIO_get_event(tls->bio, &event) < 0)
		{
			WLog_ERR(TAG, "unable to retrieve BIO associated event");
			return -1;
		}

		if (!event)
		{
			WLog_ERR(TAG, "unable to retrieve BIO event");
			return -1;
		}

		status = WaitForSingleObjectEx(event, 50, TRUE);
		switch (status)
		{
			case WAIT_OBJECT_0:
				break;
			case WAIT_TIMEOUT:
				continue;
			default:
				WLog_ERR(TAG, "error during WaitForSingleObject(): 0x%08" PRIX32 "", status);
				return -1;
		}

		TlsHandshakeResult result = tls_handshake(tls);
		switch (result)
		{
			case TLS_HANDSHAKE_CONTINUE:
				break;
			case TLS_HANDSHAKE_SUCCESS:
				return 1;
			case TLS_HANDSHAKE_ERROR:
			case TLS_HANDSHAKE_VERIFY_ERROR:
			default:
				return -1;
		}
	} while (TRUE);
}

int tls_connect(rdpTls* tls, BIO* underlying)
{
	const SSL_METHOD* method = tls_get_ssl_method(FALSE, TRUE);

	WINPR_ASSERT(tls);
	TlsHandshakeResult result = tls_connect_ex(tls, underlying, method);
	switch (result)
	{
		case TLS_HANDSHAKE_SUCCESS:
			return 1;
		case TLS_HANDSHAKE_CONTINUE:
			break;
		case TLS_HANDSHAKE_ERROR:
		case TLS_HANDSHAKE_VERIFY_ERROR:
			return -1;
	}

	return pollAndHandshake(tls);
}

#if defined(MICROSOFT_IOS_SNI_BUG) && !defined(OPENSSL_NO_TLSEXT) && \
    !defined(LIBRESSL_VERSION_NUMBER)
static void tls_openssl_tlsext_debug_callback(SSL* s, int client_server, int type,
                                              unsigned char* data, int len, void* arg)
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
	WINPR_ASSERT(tls);
	TlsHandshakeResult res =
	    tls_accept_ex(tls, underlying, settings, tls_get_ssl_method(FALSE, FALSE));
	switch (res)
	{
		case TLS_HANDSHAKE_SUCCESS:
			return TRUE;
		case TLS_HANDSHAKE_CONTINUE:
			break;
		case TLS_HANDSHAKE_ERROR:
		case TLS_HANDSHAKE_VERIFY_ERROR:
		default:
			return FALSE;
	}

	return pollAndHandshake(tls) > 0;
}

TlsHandshakeResult tls_accept_ex(rdpTls* tls, BIO* underlying, rdpSettings* settings,
                                 const SSL_METHOD* methods)
{
	WINPR_ASSERT(tls);

	long options = 0;
	BIO* bio;
	EVP_PKEY* privkey;
	int status;
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

	/**
	 * SSL_OP_NO_RENEGOTIATION
	 *
	 * Disable SSL client site renegotiation.
	 */

#if (OPENSSL_VERSION_NUMBER >= 0x10100000L && OPENSSL_VERSION_NUMBER < 0x30000000L) || \
    defined(LIBRESSL_VERSION_NUMBER)
	options |= SSL_OP_NO_RENEGOTIATION;
#endif

	if (!tls_prepare(tls, underlying, methods, options, FALSE))
		return TLS_HANDSHAKE_ERROR;

	if (settings->PrivateKeyFile)
	{
		bio = BIO_new_file(settings->PrivateKeyFile, "rb");

		if (!bio)
		{
			WLog_ERR(TAG, "BIO_new_file failed for private key %s", settings->PrivateKeyFile);
			return TLS_HANDSHAKE_ERROR;
		}
	}
	else if (settings->PrivateKeyContent)
	{
		bio = BIO_new_mem_buf(settings->PrivateKeyContent, strlen(settings->PrivateKeyContent));

		if (!bio)
		{
			WLog_ERR(TAG, "BIO_new_mem_buf failed for private key");
			return TLS_HANDSHAKE_ERROR;
		}
	}
	else
	{
		WLog_ERR(TAG, "no private key defined");
		return TLS_HANDSHAKE_ERROR;
	}

	privkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
	BIO_free_all(bio);

	if (!privkey)
	{
		WLog_ERR(TAG, "invalid private key");
		return TLS_HANDSHAKE_ERROR;
	}

	status = SSL_use_PrivateKey(tls->ssl, privkey);
	/* The local reference to the private key will anyway go out of
	 * scope; so the reference count should be decremented weither
	 * SSL_use_PrivateKey succeeds or fails.
	 */
	EVP_PKEY_free(privkey);

	if (status <= 0)
	{
		WLog_ERR(TAG, "SSL_CTX_use_PrivateKey_file failed");
		return TLS_HANDSHAKE_ERROR;
	}

	if (settings->CertificateFile)
		x509 = crypto_cert_from_pem(settings->CertificateFile, strlen(settings->CertificateFile),
		                            TRUE);
	else if (settings->CertificateContent)
		x509 = crypto_cert_from_pem(settings->CertificateContent,
		                            strlen(settings->CertificateContent), FALSE);
	else
	{
		WLog_ERR(TAG, "no certificate defined");
		return TLS_HANDSHAKE_ERROR;
	}

	if (!x509)
	{
		WLog_ERR(TAG, "invalid certificate");
		return TLS_HANDSHAKE_ERROR;
	}

	status = SSL_use_certificate(tls->ssl, x509);
	/* The local reference to the X509 certificate will anyway go out
	 * of scope; so the reference count should be decremented weither
	 * SSL_use_certificate succeeds or fails.
	 */
	X509_free(x509);

	if (status <= 0)
	{
		WLog_ERR(TAG, "SSL_use_certificate_file failed");
		return TLS_HANDSHAKE_ERROR;
	}

#if defined(MICROSOFT_IOS_SNI_BUG) && !defined(OPENSSL_NO_TLSEXT) && \
    !defined(LIBRESSL_VERSION_NUMBER)
	SSL_set_tlsext_debug_callback(tls->ssl, tls_openssl_tlsext_debug_callback);
#endif

	return tls_handshake(tls);
}

BOOL tls_send_alert(rdpTls* tls)
{
	WINPR_ASSERT(tls);

	if (!tls)
		return FALSE;

	if (!tls->ssl)
		return TRUE;

		/**
		 * FIXME: The following code does not work on OpenSSL > 1.1.0 because the
		 *        SSL struct is opaqe now
		 */
#if (!defined(LIBRESSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER < 0x10100000L)) || \
    (defined(LIBRESSL_VERSION_NUMBER) && (LIBRESSL_VERSION_NUMBER <= 0x2080300fL))

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

int tls_write_all(rdpTls* tls, const BYTE* data, int length)
{
	WINPR_ASSERT(tls);
	int status;
	int offset = 0;
	BIO* bio = tls->bio;

	while (offset < length)
	{
		ERR_clear_error();
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
				return -2; /* Abort write, there is data that must be read */
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
	WINPR_ASSERT(tls);
	tls->alertLevel = level;
	tls->alertDescription = description;
	return 0;
}

static BOOL tls_match_hostname(const char* pattern, const size_t pattern_length,
                               const char* hostname)
{
	if (strlen(hostname) == pattern_length)
	{
		if (_strnicmp(hostname, pattern, pattern_length) == 0)
			return TRUE;
	}

	if ((pattern_length > 2) && (pattern[0] == '*') && (pattern[1] == '.') &&
	    ((strlen(hostname)) >= pattern_length))
	{
		const char* check_hostname = &hostname[strlen(hostname) - pattern_length + 1];

		if (_strnicmp(check_hostname, &pattern[1], pattern_length - 1) == 0)
		{
			return TRUE;
		}
	}

	return FALSE;
}

static BOOL is_redirected(rdpTls* tls)
{
	rdpSettings* settings = tls->settings;

	if (LB_NOREDIRECT & settings->RedirectionFlags)
		return FALSE;

	return settings->RedirectionFlags != 0;
}

static BOOL is_accepted(rdpTls* tls, const BYTE* pem, size_t length)
{
	rdpSettings* settings = tls->settings;
	char* AccpetedKey;
	UINT32 AcceptedKeyLength;

	if (tls->isGatewayTransport)
	{
		AccpetedKey = settings->GatewayAcceptedCert;
		AcceptedKeyLength = settings->GatewayAcceptedCertLength;
	}
	else if (is_redirected(tls))
	{
		AccpetedKey = settings->RedirectionAcceptedCert;
		AcceptedKeyLength = settings->RedirectionAcceptedCertLength;
	}
	else
	{
		AccpetedKey = settings->AcceptedCert;
		AcceptedKeyLength = settings->AcceptedCertLength;
	}

	if (AcceptedKeyLength > 0)
	{
		if (AcceptedKeyLength == length)
		{
			if (memcmp(AccpetedKey, pem, AcceptedKeyLength) == 0)
				return TRUE;
		}
	}

	if (tls->isGatewayTransport)
	{
		free(settings->GatewayAcceptedCert);
		settings->GatewayAcceptedCert = NULL;
		settings->GatewayAcceptedCertLength = 0;
	}
	else if (is_redirected(tls))
	{
		free(settings->RedirectionAcceptedCert);
		settings->RedirectionAcceptedCert = NULL;
		settings->RedirectionAcceptedCertLength = 0;
	}
	else
	{
		free(settings->AcceptedCert);
		settings->AcceptedCert = NULL;
		settings->AcceptedCertLength = 0;
	}

	return FALSE;
}

static BOOL compare_fingerprint(const char* fp, const char* hash, CryptoCert cert, BOOL separator)
{
	BOOL equal;
	char* strhash;

	WINPR_ASSERT(fp);
	WINPR_ASSERT(hash);
	WINPR_ASSERT(cert);

	strhash = crypto_cert_fingerprint_by_hash_ex(cert->px509, hash, separator);
	if (!strhash)
		return FALSE;

	equal = (_stricmp(strhash, fp) == 0);
	free(strhash);
	return equal;
}

static BOOL compare_fingerprint_all(const char* fp, const char* hash, CryptoCert cert)
{
	if (compare_fingerprint(fp, hash, cert, FALSE))
		return TRUE;
	if (compare_fingerprint(fp, hash, cert, TRUE))
		return TRUE;
	return FALSE;
}

static BOOL is_accepted_fingerprint(CryptoCert cert, const char* CertificateAcceptedFingerprints)
{
	BOOL rc = FALSE;
	if (CertificateAcceptedFingerprints)
	{
		char* context = NULL;
		char* copy = _strdup(CertificateAcceptedFingerprints);
		char* cur = strtok_s(copy, ",", &context);
		while (cur)
		{
			char* subcontext = NULL;
			const char* h = strtok_s(cur, ":", &subcontext);
			const char* fp;

			if (!h)
				goto next;

			fp = h + strlen(h) + 1;
			if (!fp)
				goto next;

			if (compare_fingerprint_all(fp, h, cert))
			{
				rc = TRUE;
				break;
			}
		next:
			cur = strtok_s(NULL, ",", &context);
		}
		free(copy);
	}

	return rc;
}

static BOOL accept_cert(rdpTls* tls, const BYTE* pem, UINT32 length)
{
	rdpSettings* settings = tls->settings;
	char* dupPem = _strdup((const char*)pem);

	if (!dupPem)
		return FALSE;

	if (tls->isGatewayTransport)
	{
		settings->GatewayAcceptedCert = dupPem;
		settings->GatewayAcceptedCertLength = length;
	}
	else if (is_redirected(tls))
	{
		settings->RedirectionAcceptedCert = dupPem;
		settings->RedirectionAcceptedCertLength = length;
	}
	else
	{
		settings->AcceptedCert = dupPem;
		settings->AcceptedCertLength = length;
	}

	return TRUE;
}

static BOOL tls_extract_pem(CryptoCert cert, BYTE** PublicKey, size_t* PublicKeyLength)
{
	if (!cert || !PublicKey)
		return FALSE;
	*PublicKey = crypto_cert_pem(cert->px509, cert->px509chain, PublicKeyLength);
	return *PublicKey != NULL;
}

int tls_verify_certificate(rdpTls* tls, CryptoCert cert, const char* hostname, UINT16 port)
{
	int match;
	int index;
	size_t length;
	BOOL certificate_status;
	char* common_name = NULL;
	int common_name_length = 0;
	char** dns_names = 0;
	int dns_names_count = 0;
	int* dns_names_lengths = NULL;
	int verification_status = -1;
	BOOL hostname_match = FALSE;
	rdpCertificateData* certificate_data = NULL;
	BYTE* pemCert = NULL;
	DWORD flags = VERIFY_CERT_FLAG_NONE;
	freerdp* instance;

	WINPR_ASSERT(tls);
	WINPR_ASSERT(tls->settings);

	instance = (freerdp*)tls->settings->instance;
	WINPR_ASSERT(instance);

	if (freerdp_shall_disconnect_context(instance->context))
		return -1;

	if (!tls_extract_pem(cert, &pemCert, &length))
		goto end;

	/* Check, if we already accepted this key. */
	if (is_accepted(tls, pemCert, length))
	{
		verification_status = 1;
		goto end;
	}

	if (is_accepted_fingerprint(cert, tls->settings->CertificateAcceptedFingerprints))
	{
		verification_status = 1;
		goto end;
	}

	if (tls->isGatewayTransport || is_redirected(tls))
		flags |= VERIFY_CERT_FLAG_LEGACY;

	if (tls->isGatewayTransport)
		flags |= VERIFY_CERT_FLAG_GATEWAY;

	if (is_redirected(tls))
		flags |= VERIFY_CERT_FLAG_REDIRECT;

	/* Certificate management is done by the application */
	if (tls->settings->ExternalCertificateManagement)
	{
		if (instance->VerifyX509Certificate)
			verification_status =
			    instance->VerifyX509Certificate(instance, pemCert, length, hostname, port, flags);
		else
			WLog_ERR(TAG, "No VerifyX509Certificate callback registered!");

		if (verification_status > 0)
			accept_cert(tls, pemCert, length);
		else if (verification_status < 0)
		{
			WLog_ERR(TAG, "VerifyX509Certificate failed: (length = %" PRIuz ") status: [%d] %s",
			         length, verification_status, pemCert);
			goto end;
		}
	}
	/* ignore certificate verification if user explicitly required it (discouraged) */
	else if (tls->settings->IgnoreCertificate)
		verification_status = 1; /* success! */
	else if (!tls->isGatewayTransport && (tls->settings->AuthenticationLevel == 0))
		verification_status = 1; /* success! */
	else
	{
		/* if user explicitly specified a certificate name, use it instead of the hostname */
		if (!tls->isGatewayTransport && tls->settings->CertificateName)
			hostname = tls->settings->CertificateName;

		/* attempt verification using OpenSSL and the ~/.freerdp/certs certificate store */
		certificate_status =
		    x509_verify_certificate(cert, certificate_store_get_certs_path(tls->certificate_store));
		/* verify certificate name match */
		certificate_data = crypto_get_certificate_data(cert->px509, hostname, port);
		if (!certificate_data)
			goto end;
		/* extra common name and alternative names */
		common_name = crypto_cert_subject_common_name(cert->px509, &common_name_length);
		dns_names = crypto_cert_get_dns_names(cert->px509, &dns_names_count, &dns_names_lengths);

		/* compare against common name */

		if (common_name)
		{
			if (tls_match_hostname(common_name, common_name_length, hostname))
				hostname_match = TRUE;
		}

		/* compare against alternative names */

		if (dns_names)
		{
			for (index = 0; index < dns_names_count; index++)
			{
				if (tls_match_hostname(dns_names[index], dns_names_lengths[index], hostname))
				{
					hostname_match = TRUE;
					break;
				}
			}
		}

		/* if the certificate is valid and the certificate name matches, verification succeeds
		 */
		if (certificate_status && hostname_match)
			verification_status = 1; /* success! */

		if (!hostname_match)
			flags |= VERIFY_CERT_FLAG_MISMATCH;

		/* verification could not succeed with OpenSSL, use known_hosts file and prompt user for
		 * manual verification */
		if (!certificate_status || !hostname_match)
		{
			DWORD accept_certificate = 0;
			size_t pem_length = 0;
			char* issuer = crypto_cert_issuer(cert->px509);
			char* subject = crypto_cert_subject(cert->px509);
			char* pem = (char*)crypto_cert_pem(cert->px509, NULL, &pem_length);

			if (!pem)
				goto end;

			/* search for matching entry in known_hosts file */
			match = certificate_store_contains_data(tls->certificate_store, certificate_data);

			if (match == 1)
			{
				/* no entry was found in known_hosts file, prompt user for manual verification
				 */
				if (!hostname_match)
					tls_print_certificate_name_mismatch_error(hostname, port, common_name,
					                                          dns_names, dns_names_count);

				/* Automatically accept certificate on first use */
				if (tls->settings->AutoAcceptCertificate)
				{
					WLog_INFO(TAG, "No certificate stored, automatically accepting.");
					accept_certificate = 1;
				}
				else if (tls->settings->AutoDenyCertificate)
				{
					WLog_INFO(TAG, "No certificate stored, automatically denying.");
					accept_certificate = 0;
				}
				else if (instance->VerifyX509Certificate)
				{
					int rc = instance->VerifyX509Certificate(instance, pemCert, pem_length,
					                                         hostname, port, flags);

					if (rc == 1)
						accept_certificate = 1;
					else if (rc > 1)
						accept_certificate = 2;
					else
						accept_certificate = 0;
				}
				else if (instance->VerifyCertificateEx)
				{
					const BOOL use_pem = freerdp_settings_get_bool(
					    tls->settings, FreeRDP_CertificateCallbackPreferPEM);
					char* fp = NULL;
					DWORD cflags = flags;
					if (use_pem)
					{
						cflags |= VERIFY_CERT_FLAG_FP_IS_PEM;
						fp = pem;
					}
					else
						fp = crypto_cert_fingerprint(cert->px509);
					accept_certificate = instance->VerifyCertificateEx(
					    instance, hostname, port, common_name, subject, issuer, fp, cflags);
					if (!use_pem)
						free(fp);
				}
#if defined(WITH_FREERDP_DEPRECATED)
				else if (instance->VerifyCertificate)
				{
					char* fp = crypto_cert_fingerprint(cert->px509);
					WLog_WARN(TAG, "The VerifyCertificate callback is deprecated, migrate your "
					               "application to VerifyCertificateEx");
					accept_certificate = instance->VerifyCertificate(instance, common_name, subject,
					                                                 issuer, fp, !hostname_match);
					free(fp);
				}
#endif
			}
			else if (match == -1)
			{
				rdpCertificateData* stored_data =
				    certificate_store_load_data(tls->certificate_store, hostname, port);
				/* entry was found in known_hosts file, but fingerprint does not match. ask user
				 * to use it */
				tls_print_certificate_error(
				    hostname, port, pem, certificate_store_get_hosts_file(tls->certificate_store));

				if (!stored_data)
					WLog_WARN(TAG, "Failed to get certificate entry for %s:%" PRIu16 "", hostname,
					          port);

				if (tls->settings->AutoDenyCertificate)
				{
					WLog_INFO(TAG, "No certificate stored, automatically denying.");
					accept_certificate = 0;
				}
				else if (instance->VerifyX509Certificate)
				{
					const int rc =
					    instance->VerifyX509Certificate(instance, pemCert, pem_length, hostname,
					                                    port, flags | VERIFY_CERT_FLAG_CHANGED);

					if (rc == 1)
						accept_certificate = 1;
					else if (rc > 1)
						accept_certificate = 2;
					else
						accept_certificate = 0;
				}
				else if (instance->VerifyChangedCertificateEx)
				{
					DWORD cflags = flags | VERIFY_CERT_FLAG_CHANGED;
					const char* old_subject = certificate_data_get_subject(stored_data);
					const char* old_issuer = certificate_data_get_issuer(stored_data);
					const char* old_fp = certificate_data_get_fingerprint(stored_data);
					const char* old_pem = certificate_data_get_pem(stored_data);
					const BOOL fpIsAllocated =
					    !old_pem || !freerdp_settings_get_bool(
					                    tls->settings, FreeRDP_CertificateCallbackPreferPEM);
					char* fp;
					if (!fpIsAllocated)
					{
						cflags |= VERIFY_CERT_FLAG_FP_IS_PEM;
						fp = pem;
						old_fp = old_pem;
					}
					else
					{
						fp = crypto_cert_fingerprint(cert->px509);
					}
					accept_certificate = instance->VerifyChangedCertificateEx(
					    instance, hostname, port, common_name, subject, issuer, fp, old_subject,
					    old_issuer, old_fp, cflags);
					if (fpIsAllocated)
						free(fp);
				}
#if defined(WITH_FREERDP_DEPRECATED)
				else if (instance->VerifyChangedCertificate)
				{
					char* fp = crypto_cert_fingerprint(cert->px509);
					const char* old_subject = certificate_data_get_subject(stored_data);
					const char* old_issuer = certificate_data_get_issuer(stored_data);
					const char* old_fingerprint = certificate_data_get_fingerprint(stored_data);

					WLog_WARN(TAG, "The VerifyChangedCertificate callback is deprecated, migrate "
					               "your application to VerifyChangedCertificateEx");
					accept_certificate = instance->VerifyChangedCertificate(
					    instance, common_name, subject, issuer, fp, old_subject, old_issuer,
					    old_fingerprint);
					free(fp);
				}
#endif

				certificate_data_free(stored_data);
			}
			else if (match == 0)
				accept_certificate = 2; /* success! */

			/* Save certificate or do a simple accept / reject */
			switch (accept_certificate)
			{
				case 1:

					/* user accepted certificate, add entry in known_hosts file */
					verification_status =
					    certificate_store_save_data(tls->certificate_store, certificate_data) ? 1
					                                                                          : -1;
					break;

				case 2:
					/* user did accept temporaty, do not add to known hosts file */
					verification_status = 1;
					break;

				default:
					/* user did not accept, abort and do not add entry in known_hosts file */
					verification_status = -1; /* failure! */
					break;
			}

			free(issuer);
			free(subject);
			free(pem);
		}

		if (verification_status > 0)
			accept_cert(tls, pemCert, length);
	}

end:
	certificate_data_free(certificate_data);
	free(common_name);

	if (dns_names)
		crypto_cert_dns_names_free(dns_names_count, dns_names_lengths, dns_names);

	free(pemCert);
	return verification_status;
}

void tls_print_certificate_error(const char* hostname, UINT16 port, const char* fingerprint,
                                 const char* hosts_file)
{
	WLog_ERR(TAG, "The host key for %s:%" PRIu16 " has changed", hostname, port);
	WLog_ERR(TAG, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	WLog_ERR(TAG, "@    WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!     @");
	WLog_ERR(TAG, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	WLog_ERR(TAG, "IT IS POSSIBLE THAT SOMEONE IS DOING SOMETHING NASTY!");
	WLog_ERR(TAG, "Someone could be eavesdropping on you right now (man-in-the-middle attack)!");
	WLog_ERR(TAG, "It is also possible that a host key has just been changed.");
	WLog_ERR(TAG, "The fingerprint for the host key sent by the remote host is %s", fingerprint);
	WLog_ERR(TAG, "Please contact your system administrator.");
	WLog_ERR(TAG, "Add correct host key in %s to get rid of this message.", hosts_file);
	WLog_ERR(TAG, "Host key for %s has changed and you have requested strict checking.", hostname);
	WLog_ERR(TAG, "Host key verification failed.");
}

void tls_print_certificate_name_mismatch_error(const char* hostname, UINT16 port,
                                               const char* common_name, char** alt_names,
                                               int alt_names_count)
{
	int index;
	WINPR_ASSERT(NULL != hostname);
	WLog_ERR(TAG, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	WLog_ERR(TAG, "@           WARNING: CERTIFICATE NAME MISMATCH!           @");
	WLog_ERR(TAG, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	WLog_ERR(TAG, "The hostname used for this connection (%s:%" PRIu16 ") ", hostname, port);
	WLog_ERR(TAG, "does not match %s given in the certificate:",
	         alt_names_count < 1 ? "the name" : "any of the names");
	WLog_ERR(TAG, "Common Name (CN):");
	WLog_ERR(TAG, "\t%s", common_name ? common_name : "no CN found in certificate");

	if (alt_names_count > 0)
	{
		WINPR_ASSERT(NULL != alt_names);
		WLog_ERR(TAG, "Alternative names:");

		for (index = 0; index < alt_names_count; index++)
		{
			WINPR_ASSERT(alt_names[index]);
			WLog_ERR(TAG, "\t %s", alt_names[index]);
		}
	}

	WLog_ERR(TAG, "A valid certificate for the wrong name should NOT be trusted!");
}

rdpTls* tls_new(rdpSettings* settings)
{
	rdpTls* tls;
	tls = (rdpTls*)calloc(1, sizeof(rdpTls));

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

	/* tls->underlying is a stacked BIO under tls->bio.
	 * BIO_free_all will free recursivly. */
	if (tls->bio)
		BIO_free_all(tls->bio);
	else if (tls->underlying)
		BIO_free_all(tls->underlying);
	tls->bio = NULL;
	tls->underlying = NULL;

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

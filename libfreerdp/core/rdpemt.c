/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Multitransport tunnel ([MS-RDPEMT]) over reliable RDP-UDP
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

#include <freerdp/config.h>

#include <string.h>

#include <winpr/assert.h>
#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/sysinfo.h>
#include <winpr/winsock.h>

#if !defined(_WIN32)
#include <sys/select.h>
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/x509.h>

#include <freerdp/crypto/certificate.h>

#include "../crypto/certificate.h"
#include "rdpudp.h"
#include "rdpemt.h"

/* [MS-RDPEMT] 2.2.1.1 RDP_TUNNEL_HEADER Action */
#define RDPTUNNEL_ACTION_CREATEREQUEST 0x0
#define RDPTUNNEL_ACTION_CREATERESPONSE 0x1
#define RDPTUNNEL_ACTION_DATA 0x2

#define RDPTUNNEL_HEADER_LENGTH 4

/* Time allowed from the first SYN until the server accepts the tunnel. */
#define RDPEMT_SETUP_TIMEOUT_MS 20000

/* TLS record header: type, version, length */
#define TLS_RECORD_HEADER_LENGTH 5

/* Room per datagram for the TLS record header, padding and authentication tag. */
#define TLS_RECORD_OVERHEAD 64

struct rdp_emt
{
	wLog* log;
	CRITICAL_SECTION lock;
	HANDLE thread;
	volatile BOOL stop;

	UINT32 requestId;
	BYTE cookie[16];
	char* hostname;
	BYTE* publicKey;
	size_t publicKeyLength;

	rdpEmtEventFn callback;
	void* custom;

	rdpUdp* udp;
	SSL_CTX* ctx;
	SSL* ssl;
	BIO* rbio;
	BIO* wbio;

	BOOL tlsStarted;
	BOOL handshakeDone;
	BOOL ready;
	BOOL failed;
	UINT64 started;

	BYTE* out;
	size_t outLength;
	size_t outCapacity;

	BYTE* in;
	size_t inLength;
	size_t inCapacity;
};

static BOOL ensure_capacity(BYTE** buffer, size_t* capacity, size_t needed)
{
	if (needed <= *capacity)
		return TRUE;
	size_t size = (*capacity == 0) ? 4096 : *capacity;
	while (size < needed)
		size *= 2;
	BYTE* tmp = realloc(*buffer, size);
	if (!tmp)
		return FALSE;
	*buffer = tmp;
	*capacity = size;
	return TRUE;
}

static void emt_fail(rdpEmt* emt, const char* why)
{
	if (emt->failed)
		return;
	emt->failed = TRUE;
	emt->ready = FALSE;
	WLog_Print(emt->log, WLOG_WARN, "UDP multitransport tunnel failed: %s", why);
	emt->callback(emt->custom, emt, RDPEMT_EVENT_FAILED, nullptr, 0);
}

static void log_ssl_errors(rdpEmt* emt, const char* what)
{
	unsigned long err = 0;
	while ((err = ERR_get_error()) != 0)
	{
		char buffer[256] = WINPR_C_ARRAY_INIT;
		ERR_error_string_n(err, buffer, sizeof(buffer));
		WLog_Print(emt->log, WLOG_WARN, "%s: %s", what, buffer);
	}
}

/* Moves whatever TLS produced onto the RDP-UDP connection. Records are packed whole into
 * datagrams, so a peer that reads one record per datagram works as well as one that reads the
 * connection as a stream. */
static BOOL emt_flush_tls(rdpEmt* emt)
{
	while (TRUE)
	{
		const size_t pending = BIO_ctrl_pending(emt->wbio);
		if (pending == 0)
			break;
		if (!ensure_capacity(&emt->out, &emt->outCapacity, emt->outLength + pending))
			return FALSE;
		const int rc = BIO_read(emt->wbio, &emt->out[emt->outLength], (int)pending);
		if (rc <= 0)
			break;
		emt->outLength += (size_t)rc;
	}

	const size_t max = rdpudp_get_max_payload(emt->udp);
	size_t pos = 0;
	size_t chunk = 0;
	while (pos + TLS_RECORD_HEADER_LENGTH <= emt->outLength)
	{
		const size_t record =
		    TLS_RECORD_HEADER_LENGTH + ((size_t)emt->out[pos + 3] << 8) + emt->out[pos + 4];
		if (pos + record > emt->outLength)
			break;

		if ((pos > chunk) && (pos + record - chunk > max))
		{
			if (!rdpudp_send(emt->udp, &emt->out[chunk], pos - chunk))
				return FALSE;
			chunk = pos;
		}

		if (record > max)
		{
			/* should not happen with max_send_fragment set, the connection is a byte stream so
			 * a record split over datagrams still arrives intact */
			for (size_t offset = 0; offset < record; offset += max)
			{
				const size_t length = (record - offset > max) ? max : record - offset;
				if (!rdpudp_send(emt->udp, &emt->out[pos + offset], length))
					return FALSE;
			}
			pos += record;
			chunk = pos;
			continue;
		}

		pos += record;
	}

	if (pos > chunk)
	{
		if (!rdpudp_send(emt->udp, &emt->out[chunk], pos - chunk))
			return FALSE;
	}

	memmove(emt->out, &emt->out[pos], emt->outLength - pos);
	emt->outLength -= pos;
	return TRUE;
}

static BOOL emt_write_pdu(rdpEmt* emt, BYTE action, const BYTE* payload, size_t length)
{
	if (length > UINT16_MAX)
		return FALSE;

	BYTE* pdu = malloc(RDPTUNNEL_HEADER_LENGTH + length);
	if (!pdu)
		return FALSE;

	/* [MS-RDPEMT] 2.2.1.1: Action (4 bits) and Flags (4 bits), PayloadLength, HeaderLength */
	pdu[0] = (BYTE)(action & 0x0F);
	pdu[1] = (BYTE)length;
	pdu[2] = (BYTE)(length >> 8);
	pdu[3] = RDPTUNNEL_HEADER_LENGTH;
	if (length > 0)
		memcpy(&pdu[RDPTUNNEL_HEADER_LENGTH], payload, length);

	const int total = (int)(RDPTUNNEL_HEADER_LENGTH + length);
	const int rc = SSL_write(emt->ssl, pdu, total);
	free(pdu);
	if (rc != total)
	{
		log_ssl_errors(emt, "SSL_write");
		return FALSE;
	}
	return emt_flush_tls(emt);
}

static BOOL emt_send_create_request(rdpEmt* emt)
{
	/* [MS-RDPEMT] 2.2.2.1 RDP_TUNNEL_CREATEREQUEST */
	BYTE request[24] = WINPR_C_ARRAY_INIT;
	request[0] = (BYTE)emt->requestId;
	request[1] = (BYTE)(emt->requestId >> 8);
	request[2] = (BYTE)(emt->requestId >> 16);
	request[3] = (BYTE)(emt->requestId >> 24);
	/* reserved (4 bytes) stays zero */
	memcpy(&request[8], emt->cookie, sizeof(emt->cookie));

	WLog_Print(emt->log, WLOG_DEBUG, "sending Tunnel Create Request for request id %" PRIu32,
	           emt->requestId);
	return emt_write_pdu(emt, RDPTUNNEL_ACTION_CREATEREQUEST, request, sizeof(request));
}

/* The TLS session inside the tunnel ends at the same server as the main connection, so it has
 * to present the certificate that one was accepted with. */
static BOOL emt_verify_peer(rdpEmt* emt)
{
	X509* x509 = SSL_get_peer_certificate(emt->ssl);
	if (!x509)
	{
		WLog_Print(emt->log, WLOG_WARN, "the tunnel's TLS session has no server certificate");
		return FALSE;
	}

	BOOL rc = FALSE;
	BYTE* key = nullptr;
	DWORD keyLength = 0;
	rdpCertificate* cert = freerdp_certificate_new_from_x509(x509, nullptr);
	if (cert && freerdp_certificate_get_public_key(cert, &key, &keyLength))
	{
		rc = (keyLength == emt->publicKeyLength) && (memcmp(key, emt->publicKey, keyLength) == 0);
	}
	if (!rc)
		WLog_Print(emt->log, WLOG_WARN,
		           "the tunnel's TLS certificate differs from the main connection's");

	free(key);
	freerdp_certificate_free(cert);
	X509_free(x509);
	return rc;
}

static BOOL emt_process_pdus(rdpEmt* emt)
{
	size_t pos = 0;
	while (emt->inLength - pos >= RDPTUNNEL_HEADER_LENGTH)
	{
		const BYTE* pdu = &emt->in[pos];
		const BYTE action = pdu[0] & 0x0F;
		const size_t payloadLength = (size_t)pdu[1] | ((size_t)pdu[2] << 8);
		const size_t headerLength = pdu[3];
		if (headerLength < RDPTUNNEL_HEADER_LENGTH)
		{
			emt_fail(emt, "invalid tunnel PDU header");
			return FALSE;
		}

		/* HeaderLength includes any subheaders, PayloadLength does not */
		const size_t total = headerLength + payloadLength;
		if (emt->inLength - pos < total)
			break;

		switch (action)
		{
			case RDPTUNNEL_ACTION_CREATERESPONSE:
			{
				if (payloadLength < 4)
				{
					emt_fail(emt, "short Tunnel Create Response");
					return FALSE;
				}
				const BYTE* p = &pdu[headerLength];
				const UINT32 hr = (UINT32)p[0] | ((UINT32)p[1] << 8) | ((UINT32)p[2] << 16) |
				                  ((UINT32)p[3] << 24);
				if (FAILED((HRESULT)hr))
				{
					WLog_Print(emt->log, WLOG_WARN,
					           "the server refused the tunnel, hrResponse 0x%08" PRIx32, hr);
					emt_fail(emt, "Tunnel Create Response reported a failure");
					return FALSE;
				}
				if (!emt->ready)
				{
					WLog_Print(emt->log, WLOG_INFO,
					           "UDP multitransport tunnel ready (RDP-UDP version 0x%04" PRIx16 ")",
					           rdpudp_get_version(emt->udp));
					emt->ready = TRUE;
					emt->callback(emt->custom, emt, RDPEMT_EVENT_READY, nullptr, 0);
				}
			}
			break;

			case RDPTUNNEL_ACTION_DATA:
				/* Subheaders (auto-detect requests) are skipped, the payload is a DVC PDU. */
				if (payloadLength > 0)
					emt->callback(emt->custom, emt, RDPEMT_EVENT_DATA, &pdu[headerLength],
					              payloadLength);
				break;

			default:
				WLog_Print(emt->log, WLOG_DEBUG, "ignoring tunnel PDU with action %" PRIu8, action);
				break;
		}

		pos += total;
		if (emt->failed)
			return FALSE;
	}

	memmove(emt->in, &emt->in[pos], emt->inLength - pos);
	emt->inLength -= pos;
	return TRUE;
}

static BOOL emt_run_tls(rdpEmt* emt)
{
	if (!emt->handshakeDone)
	{
		ERR_clear_error();
		const int rc = SSL_do_handshake(emt->ssl);
		if (!emt_flush_tls(emt))
			return FALSE;
		if (rc != 1)
		{
			const int err = SSL_get_error(emt->ssl, rc);
			if ((err == SSL_ERROR_WANT_READ) || (err == SSL_ERROR_WANT_WRITE))
				return TRUE;
			log_ssl_errors(emt, "TLS handshake");
			emt_fail(emt, "TLS handshake failed");
			return FALSE;
		}

		emt->handshakeDone = TRUE;
		WLog_Print(emt->log, WLOG_DEBUG, "tunnel TLS session established (%s, %s)",
		           SSL_get_version(emt->ssl), SSL_get_cipher_name(emt->ssl));
		if (!emt_verify_peer(emt))
		{
			emt_fail(emt, "certificate mismatch");
			return FALSE;
		}
		if (!emt_send_create_request(emt))
		{
			emt_fail(emt, "failed to send the Tunnel Create Request");
			return FALSE;
		}
	}

	while (TRUE)
	{
		if (!ensure_capacity(&emt->in, &emt->inCapacity, emt->inLength + 16384))
			return FALSE;

		ERR_clear_error();
		const int rc = SSL_read(emt->ssl, &emt->in[emt->inLength], 16384);
		if (rc > 0)
		{
			emt->inLength += (size_t)rc;
			continue;
		}

		const int err = SSL_get_error(emt->ssl, rc);
		if (err == SSL_ERROR_WANT_READ)
			break;
		log_ssl_errors(emt, "SSL_read");
		emt_fail(emt, (err == SSL_ERROR_ZERO_RETURN) ? "the server closed the tunnel"
		                                             : "TLS error inside the tunnel");
		return FALSE;
	}

	/* post handshake messages and alerts */
	if (!emt_flush_tls(emt))
		return FALSE;

	return emt_process_pdus(emt);
}

static BOOL emt_receive(void* custom, const BYTE* data, size_t length)
{
	rdpEmt* emt = custom;
	WINPR_ASSERT(emt);

	if (!emt->tlsStarted || emt->failed)
		return TRUE;
	if (BIO_write(emt->rbio, data, (int)length) != (int)length)
		return FALSE;
	return emt_run_tls(emt);
}

static BOOL emt_start_tls(rdpEmt* emt)
{
	emt->ctx = SSL_CTX_new(TLS_client_method());
	if (!emt->ctx)
		return FALSE;

	(void)SSL_CTX_set_min_proto_version(emt->ctx, TLS1_2_VERSION);
	SSL_CTX_set_options(emt->ctx, SSL_OP_NO_TICKET | SSL_OP_NO_COMPRESSION);
	/* the certificate is checked against the main connection's in emt_verify_peer */
	SSL_CTX_set_verify(emt->ctx, SSL_VERIFY_NONE, nullptr);
	(void)SSL_CTX_set_cipher_list(emt->ctx, "ECDHE-RSA-AES256-GCM-SHA384:"
	                                        "ECDHE-RSA-AES128-GCM-SHA256:"
	                                        "ECDHE-ECDSA-AES256-GCM-SHA384:"
	                                        "ECDHE-ECDSA-AES128-GCM-SHA256:"
	                                        "AES256-GCM-SHA384:AES128-GCM-SHA256");
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
	(void)SSL_CTX_set_ciphersuites(emt->ctx, "TLS_AES_256_GCM_SHA384:TLS_AES_128_GCM_SHA256");
#endif
	/* The ClientHello has to fit in one datagram. Default groups with post quantum key shares
	 * add more than a kilobyte. */
	(void)SSL_CTX_set1_groups_list(emt->ctx, "X25519:P-256:P-384");
	(void)SSL_CTX_set1_sigalgs_list(emt->ctx, "rsa_pss_rsae_sha256:rsa_pss_rsae_sha384:"
	                                          "rsa_pkcs1_sha256:rsa_pkcs1_sha384:"
	                                          "ecdsa_secp256r1_sha256:ecdsa_secp384r1_sha384");
	SSL_CTX_set_read_ahead(emt->ctx, 0);

	emt->ssl = SSL_new(emt->ctx);
	if (!emt->ssl)
		return FALSE;

	emt->rbio = BIO_new(BIO_s_mem());
	emt->wbio = BIO_new(BIO_s_mem());
	if (!emt->rbio || !emt->wbio)
	{
		BIO_free(emt->rbio);
		BIO_free(emt->wbio);
		emt->rbio = nullptr;
		emt->wbio = nullptr;
		return FALSE;
	}
	SSL_set_bio(emt->ssl, emt->rbio, emt->wbio);
	SSL_set_connect_state(emt->ssl);
	if (emt->hostname)
		(void)SSL_set_tlsext_host_name(emt->ssl, emt->hostname);

	/* every record fits in one datagram */
	const size_t max = rdpudp_get_max_payload(emt->udp);
	if (max > TLS_RECORD_OVERHEAD + 512)
		(void)SSL_set_max_send_fragment(emt->ssl, (long)(max - TLS_RECORD_OVERHEAD));

	emt->tlsStarted = TRUE;
	return emt_run_tls(emt);
}

static BOOL emt_poll(rdpEmt* emt)
{
	if (emt->failed)
		return FALSE;

	if (!rdpudp_check(emt->udp))
	{
		emt_fail(emt, "the RDP-UDP connection failed");
		return FALSE;
	}

	if (!emt->tlsStarted && (rdpudp_get_state(emt->udp) == RDPUDP_STATE_ESTABLISHED))
	{
		if (!emt_start_tls(emt))
		{
			log_ssl_errors(emt, "TLS setup");
			emt_fail(emt, "failed to start TLS");
			return FALSE;
		}
	}

	if (!emt->ready && (GetTickCount64() - emt->started > RDPEMT_SETUP_TIMEOUT_MS))
	{
		emt_fail(emt, "timed out");
		return FALSE;
	}

	return !emt->failed;
}

static DWORD WINAPI emt_thread(LPVOID arg)
{
	rdpEmt* emt = arg;
	WINPR_ASSERT(emt);

	while (!emt->stop)
	{
		EnterCriticalSection(&emt->lock);
		const UINT32 timeout = rdpudp_get_timeout(emt->udp);
		const SOCKET sockfd = rdpudp_get_socket(emt->udp);
		LeaveCriticalSection(&emt->lock);

		fd_set rset;
		FD_ZERO(&rset);
		FD_SET(sockfd, &rset);
		struct timeval tv = { 0, (long)timeout * 1000 };
		(void)select((int)sockfd + 1, &rset, nullptr, nullptr, &tv);

		if (emt->stop)
			break;

		EnterCriticalSection(&emt->lock);
		const BOOL rc = emt_poll(emt);
		LeaveCriticalSection(&emt->lock);
		if (!rc)
			break;
	}

	ExitThread(0);
	return 0;
}

rdpEmt* rdpemt_new(wLog* log, UINT32 requestId, const BYTE* cookie, const char* hostname,
                   const BYTE* publicKey, size_t publicKeyLength, rdpEmtEventFn callback,
                   void* custom)
{
	WINPR_ASSERT(log);
	WINPR_ASSERT(cookie);
	WINPR_ASSERT(publicKey);
	WINPR_ASSERT(callback);

	rdpEmt* emt = calloc(1, sizeof(rdpEmt));
	if (!emt)
		return nullptr;

	emt->log = log;
	emt->requestId = requestId;
	memcpy(emt->cookie, cookie, sizeof(emt->cookie));
	emt->callback = callback;
	emt->custom = custom;
	if (!InitializeCriticalSectionAndSpinCount(&emt->lock, 4000))
	{
		free(emt);
		return nullptr;
	}

	if (hostname)
	{
		emt->hostname = _strdup(hostname);
		if (!emt->hostname)
			goto fail;
	}

	emt->publicKey = malloc(publicKeyLength);
	if (!emt->publicKey)
		goto fail;
	memcpy(emt->publicKey, publicKey, publicKeyLength);
	emt->publicKeyLength = publicKeyLength;

	/* [MS-RDPEUDP] 2.2.2.9: cookieHash is the SHA-256 hash of the securityCookie. It is sent
	 * with each 32 bit word byte swapped, which is what Windows accepts. */
	BYTE digest[32] = WINPR_C_ARRAY_INIT;
	if (!winpr_Digest(WINPR_MD_SHA256, emt->cookie, sizeof(emt->cookie), digest, sizeof(digest)))
		goto fail;
	BYTE cookieHash[32] = WINPR_C_ARRAY_INIT;
	for (size_t x = 0; x < sizeof(cookieHash); x += 4)
	{
		cookieHash[x] = digest[x + 3];
		cookieHash[x + 1] = digest[x + 2];
		cookieHash[x + 2] = digest[x + 1];
		cookieHash[x + 3] = digest[x];
	}

	emt->udp = rdpudp_new(log, cookieHash, emt_receive, emt);
	if (!emt->udp)
		goto fail;

	return emt;
fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	rdpemt_free(emt);
	WINPR_PRAGMA_DIAG_POP
	return nullptr;
}

BOOL rdpemt_start(rdpEmt* emt, const struct sockaddr* addr, size_t addrlen)
{
	WINPR_ASSERT(emt);

	emt->started = GetTickCount64();
	if (!rdpudp_connect(emt->udp, addr, addrlen))
		return FALSE;

	emt->thread = CreateThread(nullptr, 0, emt_thread, emt, 0, nullptr);
	return emt->thread != nullptr;
}

void rdpemt_free(rdpEmt* emt)
{
	if (!emt)
		return;

	emt->stop = TRUE;
	if (emt->thread)
	{
		(void)WaitForSingleObject(emt->thread, INFINITE);
		(void)CloseHandle(emt->thread);
	}

	SSL_free(emt->ssl); /* frees both BIOs */
	SSL_CTX_free(emt->ctx);
	rdpudp_free(emt->udp);
	free(emt->hostname);
	free(emt->publicKey);
	free(emt->in);
	free(emt->out);
	DeleteCriticalSection(&emt->lock);
	free(emt);
}

BOOL rdpemt_send_data(rdpEmt* emt, const BYTE* data, size_t length)
{
	WINPR_ASSERT(emt);

	BOOL rc = FALSE;
	EnterCriticalSection(&emt->lock);
	if (emt->ready && !emt->failed)
	{
		rc = emt_write_pdu(emt, RDPTUNNEL_ACTION_DATA, data, length);
		if (!rc)
			emt_fail(emt, "failed to send tunnel data");
	}
	LeaveCriticalSection(&emt->lock);
	return rc;
}

BOOL rdpemt_is_supported(void)
{
	return TRUE;
}

UINT32 rdpemt_get_request_id(const rdpEmt* emt)
{
	WINPR_ASSERT(emt);
	return emt->requestId;
}

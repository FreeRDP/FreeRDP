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
#include <winpr/cast.h>
#include <winpr/crt.h>
#include <winpr/interlocked.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/sysinfo.h>
#include <winpr/winsock.h>
#include <winpr/stream.h>

#if !defined(_WIN32)
#if defined(WINPR_HAVE_POLL_H)
#include <poll.h>
#else
#include <sys/select.h>
#endif
#endif

#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/x509.h>

#include <freerdp/crypto/certificate.h>

#include "../crypto/certificate.h"
#include "rdpudp.h"
#include "udplistener.h"
#include "rdpemt.h"

/* [MS-RDPEMT] 2.2.1.1 RDP_TUNNEL_HEADER Action */
#define RDPTUNNEL_ACTION_CREATEREQUEST 0x0
#define RDPTUNNEL_ACTION_CREATERESPONSE 0x1
#define RDPTUNNEL_ACTION_DATA 0x2

#define RDPTUNNEL_HEADER_LENGTH 4

/* [MS-RDPEMT] 2.2.1.1.1 RDP_TUNNEL_SUBHEADER SubHeaderType */
#define RDPTUNNEL_SUBHEADER_AUTODETECT_REQUEST 0x00
#define RDPTUNNEL_SUBHEADER_AUTODETECT_RESPONSE 0x01

/* Time allowed from the first SYN until the server accepts the tunnel. */
#define RDPEMT_SETUP_TIMEOUT_MS 20000

/* TLS record header: type, version, length */
#define TLS_RECORD_HEADER_LENGTH 5

/* Room per datagram for the TLS record header, padding and authentication tag. */
#define TLS_RECORD_OVERHEAD 64

/* The server collects tunnel PDUs into TLS records of up to this much plaintext. */
#define RDPEMT_SERVER_BATCH 16000

/* The default maximum of data that waits for the link: the data that fits into 32768 datagrams. At
 * 20 Mbit/s this is approximately 15 s of data. A server that gets to this maximum does not pace
 * itself. A reliable tunnel cannot drop any of this data, so the tunnel fails above this
 * maximum. */
#define RDPEMT_SEND_LIMIT (32768ull * 1232ull)

struct rdp_emt
{
	wLog* log;
	CRITICAL_SECTION lock;
	HANDLE thread;
	LONG stop; /* set by rdpemt_free, read by the thread; Interlocked functions only */

	UINT32 requestId;
	BYTE cookie[16];
	char* hostname;
	BYTE* publicKey;
	size_t publicKeyLength;

	rdpEmtEventFn callback;
	void* custom;

	/* guarded by lock: tunnel data goes to receiver instead of callback when it is set */
	rdpEmtReceiverFn receiver;
	void* receiverCustom;
	/* handed to receiver and not yet taken; Interlocked functions only */
	LONG directBytes;

	/* server side: the certificate and key of the main connection, and the hash of the cookie that
	 * the SYN of the client contains */
	BOOL server;
	X509* cert;
	EVP_PKEY* key;
	BYTE cookieHash[32];

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

	/* TLS output not yet on the connection, and plaintext not yet parsed into tunnel PDUs;
	 * the position of each is how much they hold */
	wStream* out;
	wStream* in;

	/* server side: tunnel PDUs that wait to go into one TLS record, full up to its position */
	wStream* batch;

	/* server side: the source and the destination of datagrams */
	rdpUdpFlow* flow;

	/* Server side: the data that the link has no room for yet, see EMT_HOLD_DATA. The data waits as
	 * plaintext so that it goes into full TLS records when there is room, and so that it leaves
	 * here in order. */
	wQueue* hold;
	size_t holdBytes;
	size_t sendLimit;
	BOOL writeBlocked; /* reported to the server application until the backlog is gone */
};

/* Drops the first count bytes of a stream that is filled up to its position. */
WINPR_ATTR_NODISCARD
static BOOL stream_consume(wStream* s, size_t count)
{
	WINPR_ASSERT(s);

	const size_t used = Stream_GetPosition(s);
	if (count > used)
		return FALSE;
	BYTE* buffer = Stream_Buffer(s);
	memmove(buffer, &buffer[count], used - count);
	return Stream_SetPosition(s, used - count);
}

static void emt_fail(rdpEmt* emt, const char* why)
{
	WINPR_ASSERT(emt);
	WINPR_ASSERT(why);

	if (emt->failed)
		return;
	emt->failed = TRUE;
	emt->ready = FALSE;
	WLog_Print(emt->log, WLOG_WARN, "UDP multitransport tunnel failed: %s", why);
	emt->callback(emt->custom, emt, RDPEMT_EVENT_FAILED, nullptr, 0);
}

/* Called with lock held. */
static void emt_hand_up(rdpEmt* emt, const BYTE* data, size_t length)
{
	WINPR_ASSERT(emt);
	WINPR_ASSERT(data);

	if (!emt->receiver)
	{
		emt->callback(emt->custom, emt, RDPEMT_EVENT_DATA, data, length);
		return;
	}

	/* the receiver takes this stream over, the data is not copied again on the way */
	wStream* s = Stream_New(nullptr, length);
	if (!s)
	{
		emt_fail(emt, "out of memory for tunnel data");
		return;
	}
	Stream_Write(s, data, length);
	Stream_SealLength(s);
	Stream_ResetPosition(s);
	if (!emt->receiver(emt->receiverCustom, s))
	{
		emt_fail(emt, "the receiver of tunnel data failed");
		return;
	}
	const LONG direct =
	    InterlockedExchangeAdd(&emt->directBytes, WINPR_ASSERTING_INT_CAST(LONG, length));
	WINPR_UNUSED(direct);
}

static void log_ssl_errors(rdpEmt* emt, const char* what)
{
	WINPR_ASSERT(emt);
	WINPR_ASSERT(what);

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
WINPR_ATTR_NODISCARD
static BOOL emt_flush_tls(rdpEmt* emt)
{
	WINPR_ASSERT(emt);

	while (TRUE)
	{
		const size_t pending = BIO_ctrl_pending(emt->wbio);
		if (pending == 0)
			break;
		if (pending > INT32_MAX)
			return FALSE;
		if (!Stream_EnsureRemainingCapacity(emt->out, pending))
			return FALSE;
		const int rc =
		    BIO_read(emt->wbio, Stream_Pointer(emt->out), WINPR_ASSERTING_INT_CAST(int, pending));
		if (rc <= 0)
			break;
		Stream_Seek(emt->out, WINPR_ASSERTING_INT_CAST(size_t, rc));
	}

	const size_t max = rdpudp_get_max_payload(emt->udp);
	if (max == 0)
		return FALSE;
	if (emt->server)
	{
		/* the connection is a byte stream, so the server fills each datagram */
		const BYTE* bytes = Stream_Buffer(emt->out);
		const size_t total = Stream_GetPosition(emt->out);
		size_t offset = 0;
		while (offset < total)
		{
			/* the bytes left, not offset + max, because that sum can wrap */
			const size_t length = (total - offset > max) ? max : total - offset;
			if (!rdpudp_send(emt->udp, &bytes[offset], length))
				return FALSE;
			offset += length;
		}
		return stream_consume(emt->out, total);
	}
	const BYTE* data = Stream_Buffer(emt->out);
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&sbuffer, data, Stream_GetPosition(emt->out));

	size_t chunk = 0;
	while (Stream_GetRemainingLength(s) >= TLS_RECORD_HEADER_LENGTH)
	{
		/* TLS record header: ContentType, ProtocolVersion, length */
		const size_t pos = Stream_GetPosition(s);
		Stream_Seek(s, 3);
		const size_t record = TLS_RECORD_HEADER_LENGTH + Stream_Get_UINT16_BE(s);
		if (Stream_GetRemainingLength(s) < record - TLS_RECORD_HEADER_LENGTH)
		{
			if (!Stream_SetPosition(s, pos))
				return FALSE;
			break;
		}
		Stream_Seek(s, record - TLS_RECORD_HEADER_LENGTH);

		if ((pos > chunk) && (pos + record - chunk > max))
		{
			if (!rdpudp_send(emt->udp, &data[chunk], pos - chunk))
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
				if (!rdpudp_send(emt->udp, &data[pos + offset], length))
					return FALSE;
			}
			chunk = pos + record;
		}
	}

	const size_t end = Stream_GetPosition(s);
	if ((end > chunk) && !rdpudp_send(emt->udp, &data[chunk], end - chunk))
		return FALSE;

	return stream_consume(emt->out, end);
}

WINPR_ATTR_NODISCARD
static BOOL emt_write_pdu(rdpEmt* emt, BYTE action, const BYTE* subHeader, size_t subHeaderLength,
                          const BYTE* payload, size_t length)
{
	WINPR_ASSERT(emt);
	WINPR_ASSERT(subHeader || (subHeaderLength == 0));
	WINPR_ASSERT(payload || (length == 0));

	if ((length > UINT16_MAX) || (subHeaderLength > UINT8_MAX - RDPTUNNEL_HEADER_LENGTH))
		return FALSE;

	wStream* s = Stream_New(nullptr, RDPTUNNEL_HEADER_LENGTH + subHeaderLength + length);
	if (!s)
		return FALSE;

	/* [MS-RDPEMT] 2.2.1.1 RDP_TUNNEL_HEADER, HeaderLength includes the SubHeaders */
	const UINT8 actionByte = WINPR_ASSERTING_INT_CAST(UINT8, action & 0x0F);
	const UINT16 payloadLength = WINPR_ASSERTING_INT_CAST(UINT16, length);
	const UINT8 headerLength =
	    WINPR_ASSERTING_INT_CAST(UINT8, RDPTUNNEL_HEADER_LENGTH + subHeaderLength);
	Stream_Write_UINT8(s, actionByte);     /* Action (4 bits), Flags (4 bits) */
	Stream_Write_UINT16(s, payloadLength); /* PayloadLength */
	Stream_Write_UINT8(s, headerLength);   /* HeaderLength */
	Stream_Write(s, subHeader, subHeaderLength);
	Stream_Write(s, payload, length);

	const int total = WINPR_ASSERTING_INT_CAST(int, Stream_GetPosition(s));
	const int rc = SSL_write(emt->ssl, Stream_Buffer(s), total);
	Stream_Free(s, TRUE);
	if (rc != total)
	{
		log_ssl_errors(emt, "SSL_write");
		return FALSE;
	}
	return emt_flush_tls(emt);
}

/* Writes the collected tunnel PDUs as one TLS record. */
WINPR_ATTR_NODISCARD
static BOOL emt_write_batch(rdpEmt* emt)
{
	WINPR_ASSERT(emt);

	const size_t length = emt->batch ? Stream_GetPosition(emt->batch) : 0;
	if (length == 0)
		return TRUE;
	if (length > INT32_MAX)
		return FALSE;
	const int total = WINPR_ASSERTING_INT_CAST(int, length);
	const int rc = SSL_write(emt->ssl, Stream_Buffer(emt->batch), total);
	Stream_ResetPosition(emt->batch);
	if (rc != total)
	{
		log_ssl_errors(emt, "SSL_write");
		return FALSE;
	}
	return emt_flush_tls(emt);
}

/* Adds a tunnel data PDU to the batch. Writes the batch first if the PDU makes the batch too
 * large. */
WINPR_ATTR_NODISCARD
static BOOL emt_batch_data(rdpEmt* emt, const BYTE* payload, size_t length)
{
	WINPR_ASSERT(emt);
	WINPR_ASSERT(payload || (length == 0));

	if (length > UINT16_MAX)
		return FALSE;
	if (!emt->batch)
	{
		emt->batch = Stream_New(nullptr, RDPEMT_SERVER_BATCH);
		if (!emt->batch)
			return FALSE;
	}
	/* The room left, not a sum, because a sum can wrap. A PDU that is larger than a batch goes
	 * alone. */
	const size_t pdu = RDPTUNNEL_HEADER_LENGTH + length;
	const size_t used = Stream_GetPosition(emt->batch);
	if (((used > RDPEMT_SERVER_BATCH) || (RDPEMT_SERVER_BATCH - used < pdu)) &&
	    !emt_write_batch(emt))
		return FALSE;
	if (!Stream_EnsureRemainingCapacity(emt->batch, pdu))
		return FALSE;

	/* [MS-RDPEMT] 2.2.1.1 RDP_TUNNEL_HEADER */
	const UINT8 actionByte = RDPTUNNEL_ACTION_DATA;
	const UINT16 payloadLength = WINPR_ASSERTING_INT_CAST(UINT16, length);
	Stream_Write_UINT8(emt->batch, actionByte);
	Stream_Write_UINT16(emt->batch, payloadLength);
	Stream_Write_UINT8(emt->batch, RDPTUNNEL_HEADER_LENGTH);
	Stream_Write(emt->batch, payload, length);
	return TRUE;
}

/* The types of the items that wait in hold, the first byte of each item. */
#define EMT_HOLD_DATA 0
#define EMT_HOLD_SUBHEADER 1

/* A Tunnel Data PDU without HigherLayerData. Windows sends its auto-detect requests like this. The
 * data that was collected before goes first, because a bandwidth measurement counts the data
 * between two of these PDUs. */
WINPR_ATTR_NODISCARD
static BOOL emt_write_subheader(rdpEmt* emt, const BYTE* subHeader, size_t length)
{
	WINPR_ASSERT(emt);
	return emt_write_batch(emt) &&
	       emt_write_pdu(emt, RDPTUNNEL_ACTION_DATA, subHeader, length, nullptr, 0);
}

/* The data that waits for the link, in datagrams. */
WINPR_ATTR_NODISCARD
static size_t emt_backlog(const rdpEmt* emt)
{
	WINPR_ASSERT(emt);
	const size_t max = rdpudp_get_max_payload(emt->udp);
	return rdpudp_get_send_backlog(emt->udp) + ((max > 0) ? emt->holdBytes / max : 0);
}

/* The tunnel is blocked at two windows of backlog and is free again at one window. The backlog
 * stays at approximately two round trips of data at all link speeds, and there is nothing to
 * tune. */
static void emt_update_blocked(rdpEmt* emt)
{
	WINPR_ASSERT(emt);

	const size_t window = rdpudp_get_send_window(emt->udp);
	const size_t backlog = emt_backlog(emt);
	if (!emt->writeBlocked && (backlog >= 2 * window))
		emt->writeBlocked = TRUE;
	else if (emt->writeBlocked && (backlog <= window))
	{
		emt->writeBlocked = FALSE;
		emt->callback(emt->custom, emt, RDPEMT_EVENT_WRITABLE, nullptr, 0);
	}
}

/* Keeps the data for a tunnel data PDU until the link has room for it. */
WINPR_ATTR_NODISCARD
static BOOL emt_hold(rdpEmt* emt, BYTE kind, const BYTE* payload, size_t length)
{
	WINPR_ASSERT(emt);
	WINPR_ASSERT(payload || (length == 0));

	if ((emt->holdBytes > emt->sendLimit) || (length > emt->sendLimit - emt->holdBytes))
	{
		WLog_Print(emt->log, WLOG_ERROR,
		           "%" PRIuz " bytes wait for a link that takes less than the server sends, "
		           "ending the tunnel: the server does not pace itself",
		           emt->holdBytes);
		emt_fail(emt, "the server sends more than the link takes");
		return FALSE;
	}
	wStream* s = Stream_New(nullptr, 1 + length);
	if (!s)
		return FALSE;
	Stream_Write_UINT8(s, kind);
	Stream_Write(s, payload, length);
	if (!Queue_Enqueue(emt->hold, s))
	{
		Stream_Free(s, TRUE);
		return FALSE;
	}
	emt->holdBytes += length;
	emt_update_blocked(emt);
	return TRUE;
}

/* Moves data out of hold until the link has no more room. */
WINPR_ATTR_NODISCARD
static BOOL emt_drain_hold(rdpEmt* emt)
{
	WINPR_ASSERT(emt);

	BOOL moved = FALSE;
	const size_t window = rdpudp_get_send_window(emt->udp);
	while ((Queue_Count(emt->hold) > 0) && (rdpudp_get_send_backlog(emt->udp) < window))
	{
		wStream* s = Queue_Dequeue(emt->hold);
		if (!s)
			break;
		const BYTE* item = Stream_Buffer(s);
		const size_t length = Stream_GetPosition(s) - 1;
		emt->holdBytes -= length;
		const BOOL rc = (item[0] == EMT_HOLD_DATA) ? emt_batch_data(emt, &item[1], length)
		                                           : emt_write_subheader(emt, &item[1], length);
		Stream_Free(s, TRUE);
		if (!rc)
			return FALSE;
		moved = TRUE;
	}
	if (moved && !emt_write_batch(emt))
		return FALSE;
	emt_update_blocked(emt);
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL emt_send_create_request(rdpEmt* emt)
{
	WINPR_ASSERT(emt);

	BYTE request[24] = WINPR_C_ARRAY_INIT;
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticInit(&sbuffer, request, sizeof(request));

	/* [MS-RDPEMT] 2.2.2.1 RDP_TUNNEL_CREATEREQUEST */
	Stream_Write_UINT32(s, emt->requestId);            /* RequestID */
	Stream_Write_UINT32(s, 0);                         /* Reserved */
	Stream_Write(s, emt->cookie, sizeof(emt->cookie)); /* SecurityCookie */

	WLog_Print(emt->log, WLOG_DEBUG, "sending Tunnel Create Request for request id %" PRIu32,
	           emt->requestId);
	return emt_write_pdu(emt, RDPTUNNEL_ACTION_CREATEREQUEST, nullptr, 0, Stream_Buffer(s),
	                     Stream_GetPosition(s));
}

/* The TLS session inside the tunnel ends at the same server as the main connection, so it has
 * to present the certificate that one was accepted with. */
WINPR_ATTR_NODISCARD
static BOOL emt_verify_peer(rdpEmt* emt)
{
	WINPR_ASSERT(emt);

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

/* [MS-RDPEMT] 2.2.1.1.1: the SubHeaders field holds any number of RDP_TUNNEL_SUBHEADER. An
 * auto-detect request is a whole subheader, its headerLength and headerTypeId are the
 * SubHeaderLength and SubHeaderType ([MS-RDPBCGR] 2.2.14.1). */
WINPR_ATTR_NODISCARD
static BOOL emt_process_subheaders(rdpEmt* emt, const BYTE* data, size_t length)
{
	WINPR_ASSERT(emt);
	WINPR_ASSERT(data || (length == 0));

	while (length > 0)
	{
		const size_t subHeaderLength = data[0];
		if ((length < 2) || (subHeaderLength < 2) || (subHeaderLength > length))
		{
			emt_fail(emt, "invalid tunnel subheader");
			return FALSE;
		}

		const BYTE subHeaderType = data[1];
		/* The server asks and the client answers. Each end gets only what the other end sends. */
		if (subHeaderType == (emt->server ? RDPTUNNEL_SUBHEADER_AUTODETECT_RESPONSE
		                                  : RDPTUNNEL_SUBHEADER_AUTODETECT_REQUEST))
			emt->callback(emt->custom, emt, RDPEMT_EVENT_AUTODETECT, data, subHeaderLength);
		else
			WLog_Print(emt->log, WLOG_DEBUG, "ignoring tunnel subheader of type 0x%02" PRIx8,
			           subHeaderType);
		data += subHeaderLength;
		length -= subHeaderLength;
	}
	return TRUE;
}

/* Server: [MS-RDPEMT] 2.2.2.1 RDP_TUNNEL_CREATEREQUEST. The answer is 2.2.2.2
 * RDP_TUNNEL_CREATERESPONSE. The request id and the cookie are those of the Initiate Multitransport
 * Request for this tunnel. */
WINPR_ATTR_NODISCARD
static BOOL emt_accept_create_request(rdpEmt* emt, wStream* s, size_t payloadLength)
{
	WINPR_ASSERT(emt);
	WINPR_ASSERT(s);

	/* emt_process_pdus examined if the payload is present. This code examines that again before it
	 * reads the payload. */
	if ((payloadLength < 8 + sizeof(emt->cookie)) ||
	    !Stream_CheckAndLogRequiredLengthWLog(emt->log, s, payloadLength))
	{
		emt_fail(emt, "short Tunnel Create Request");
		return FALSE;
	}
	const UINT32 requestId = Stream_Get_UINT32(s); /* RequestID */
	Stream_Seek_UINT32(s);                         /* Reserved */
	const BYTE* cookie = Stream_ConstPointer(s);   /* SecurityCookie */
	Stream_Seek(s, payloadLength - 8);

	const BOOL valid = (requestId == emt->requestId) &&
	                   (CRYPTO_memcmp(cookie, emt->cookie, sizeof(emt->cookie)) == 0);
	BYTE response[4] = WINPR_C_ARRAY_INIT;
	wStream rbuffer = WINPR_C_ARRAY_INIT;
	wStream* rs = Stream_StaticInit(&rbuffer, response, sizeof(response));
	if (!rs)
	{
		emt_fail(emt, "failed to build the Tunnel Create Response");
		return FALSE;
	}
	/* HRESULT bit patterns, E_ABORT is negative in an int32 */
	const UINT32 hr = valid ? (UINT32)S_OK : (UINT32)E_ABORT;
	Stream_Write_UINT32(rs, hr); /* HrResponse */
	if (!emt_write_pdu(emt, RDPTUNNEL_ACTION_CREATERESPONSE, nullptr, 0, response,
	                   sizeof(response)))
	{
		emt_fail(emt, "failed to send the Tunnel Create Response");
		return FALSE;
	}
	if (!valid)
	{
		emt_fail(emt, "the Tunnel Create Request names another request or cookie");
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
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL emt_process_pdus(rdpEmt* emt)
{
	WINPR_ASSERT(emt);

	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s =
	    Stream_StaticConstInit(&sbuffer, Stream_Buffer(emt->in), Stream_GetPosition(emt->in));

	while (Stream_GetRemainingLength(s) >= RDPTUNNEL_HEADER_LENGTH)
	{
		/* [MS-RDPEMT] 2.2.1.1 RDP_TUNNEL_HEADER */
		const size_t start = Stream_GetPosition(s);
		const BYTE action = Stream_Get_UINT8(s) & 0x0F;
		const size_t payloadLength = Stream_Get_UINT16(s);
		const size_t headerLength = Stream_Get_UINT8(s);
		if (headerLength < RDPTUNNEL_HEADER_LENGTH)
		{
			emt_fail(emt, "invalid tunnel PDU header");
			return FALSE;
		}

		/* HeaderLength includes any subheaders, PayloadLength does not. Both are bounded by
		 * their wire types (UINT8, UINT16), the sum cannot overflow. */
		WINPR_ASSERT(payloadLength <= UINT16_MAX);
		WINPR_ASSERT(headerLength <= UINT8_MAX);
		const size_t rest = headerLength - RDPTUNNEL_HEADER_LENGTH + payloadLength;
		if (Stream_GetRemainingLength(s) < rest)
		{
			/* not all here yet */
			if (!Stream_SetPosition(s, start))
				return FALSE;
			break;
		}
		/* nothing from a client counts before its cookie */
		if (emt->server && !emt->ready && (headerLength > RDPTUNNEL_HEADER_LENGTH))
		{
			emt_fail(emt, "tunnel subheaders before the Tunnel Create Request");
			return FALSE;
		}

		/* the subheaders go up ahead of the payload of their PDU */
		if (!emt_process_subheaders(emt, Stream_ConstPointer(s),
		                            headerLength - RDPTUNNEL_HEADER_LENGTH))
			return FALSE;
		Stream_Seek(s, headerLength - RDPTUNNEL_HEADER_LENGTH);

		switch (action)
		{
			case RDPTUNNEL_ACTION_CREATERESPONSE:
			{
				/* Only a server answers. If the server accepts this from a client, the tunnel
				 * becomes ready without the cookie. */
				if (emt->server)
				{
					emt_fail(emt, "a Tunnel Create Response from the client");
					return FALSE;
				}

				/* [MS-RDPEMT] 2.2.2.2 RDP_TUNNEL_CREATERESPONSE */
				if (payloadLength < 4)
				{
					emt_fail(emt, "short Tunnel Create Response");
					return FALSE;
				}
				const UINT32 hr = Stream_Get_UINT32(s); /* HrResponse */
				Stream_Seek(s, payloadLength - 4);
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

			case RDPTUNNEL_ACTION_CREATEREQUEST:
				if (!emt->server)
				{
					Stream_Seek(s, payloadLength);
					break;
				}
				if (!emt_accept_create_request(emt, s, payloadLength))
					return FALSE;
				break;

			case RDPTUNNEL_ACTION_DATA:
				/* [MS-RDPEMT] 2.2.2.3 RDP_TUNNEL_DATA, HigherLayerData is a DVC PDU */
				if (emt->server && !emt->ready)
				{
					emt_fail(emt, "tunnel data before the Tunnel Create Request");
					return FALSE;
				}
				if (payloadLength > 0)
					emt_hand_up(emt, Stream_ConstPointer(s), payloadLength);
				Stream_Seek(s, payloadLength);
				break;

			default:
				WLog_Print(emt->log, WLOG_DEBUG, "ignoring tunnel PDU with action %" PRIu8, action);
				Stream_Seek(s, payloadLength);
				break;
		}

		if (emt->failed)
			return FALSE;
	}

	return stream_consume(emt->in, Stream_GetPosition(s));
}

WINPR_ATTR_NODISCARD
static BOOL emt_run_tls(rdpEmt* emt)
{
	WINPR_ASSERT(emt);

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
		/* the server waits for the client's Tunnel Create Request */
		if (!emt->server)
		{
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
	}

	while (TRUE)
	{
		if (!Stream_EnsureRemainingCapacity(emt->in, 16384))
			return FALSE;

		ERR_clear_error();
		const int rc = SSL_read(emt->ssl, Stream_Pointer(emt->in), 16384);
		if (rc > 0)
		{
			Stream_Seek(emt->in, WINPR_ASSERTING_INT_CAST(size_t, rc));
			continue;
		}

		const int err = SSL_get_error(emt->ssl, rc);
		if (err == SSL_ERROR_WANT_READ)
			break;
		log_ssl_errors(emt, "SSL_read");
		emt_fail(emt, (err == SSL_ERROR_ZERO_RETURN) ? "the peer closed the tunnel"
		                                             : "TLS error inside the tunnel");
		return FALSE;
	}

	/* post handshake messages and alerts */
	if (!emt_flush_tls(emt))
		return FALSE;

	return emt_process_pdus(emt);
}

WINPR_ATTR_NODISCARD
static BOOL emt_receive(void* custom, const BYTE* data, size_t length)
{
	rdpEmt* emt = custom;
	WINPR_ASSERT(emt);
	WINPR_ASSERT(data || (length == 0));

	if (!emt->tlsStarted || emt->failed)
		return TRUE;
	if (length > INT32_MAX)
		return FALSE;
	const int len = WINPR_ASSERTING_INT_CAST(int, length);
	if (BIO_write(emt->rbio, data, len) != len)
		return FALSE;
	return emt_run_tls(emt);
}

WINPR_ATTR_NODISCARD
static BOOL emt_flow_send(void* custom, const BYTE* data, size_t length)
{
	return freerdp_udp_flow_send(custom, data, length);
}

WINPR_ATTR_NODISCARD
static BOOL emt_start_tls(rdpEmt* emt)
{
	WINPR_ASSERT(emt);

	emt->ctx = SSL_CTX_new(emt->server ? TLS_server_method() : TLS_client_method());
	if (!emt->ctx)
		return FALSE;

	/* The server shows the certificate of the main connection. The client looks for this
	 * certificate. */
	if (emt->server && ((SSL_CTX_use_certificate(emt->ctx, emt->cert) != 1) ||
	                    (SSL_CTX_use_PrivateKey(emt->ctx, emt->key) != 1)))
	{
		WLog_Print(emt->log, WLOG_ERROR, "cannot use the server certificate for the tunnel");
		return FALSE;
	}

	if (!SSL_CTX_set_min_proto_version(emt->ctx, TLS1_2_VERSION))
	{
		WLog_Print(emt->log, WLOG_ERROR, "SSL_CTX_set_min_proto_version failed");
		return FALSE;
	}
	SSL_CTX_set_options(emt->ctx, SSL_OP_NO_TICKET | SSL_OP_NO_COMPRESSION);
	/* the certificate is checked against the main connection's in emt_verify_peer */
	SSL_CTX_set_verify(emt->ctx, SSL_VERIFY_NONE, nullptr);
	if (!SSL_CTX_set_cipher_list(emt->ctx, "ECDHE-RSA-AES256-GCM-SHA384:"
	                                       "ECDHE-RSA-AES128-GCM-SHA256:"
	                                       "ECDHE-ECDSA-AES256-GCM-SHA384:"
	                                       "ECDHE-ECDSA-AES128-GCM-SHA256:"
	                                       "AES256-GCM-SHA384:AES128-GCM-SHA256"))
	{
		WLog_Print(emt->log, WLOG_ERROR, "SSL_CTX_set_cipher_list failed");
		return FALSE;
	}
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
	if (!SSL_CTX_set_ciphersuites(emt->ctx, "TLS_AES_256_GCM_SHA384:TLS_AES_128_GCM_SHA256"))
	{
		WLog_Print(emt->log, WLOG_ERROR, "SSL_CTX_set_ciphersuites failed");
		return FALSE;
	}
#endif
	/* The ClientHello has to fit in one datagram. Default groups with post quantum key shares
	 * add more than a kilobyte. */
	if (!SSL_CTX_set1_groups_list(emt->ctx, "X25519:P-256:P-384"))
	{
		WLog_Print(emt->log, WLOG_ERROR, "SSL_CTX_set1_groups_list failed");
		return FALSE;
	}
	if (!SSL_CTX_set1_sigalgs_list(emt->ctx, "rsa_pss_rsae_sha256:rsa_pss_rsae_sha384:"
	                                         "rsa_pkcs1_sha256:rsa_pkcs1_sha384:"
	                                         "ecdsa_secp256r1_sha256:ecdsa_secp384r1_sha384"))
	{
		WLog_Print(emt->log, WLOG_ERROR, "SSL_CTX_set1_sigalgs_list failed");
		return FALSE;
	}
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
	if (emt->server)
		SSL_set_accept_state(emt->ssl);
	else
	{
		SSL_set_connect_state(emt->ssl);
		if (emt->hostname)
			(void)SSL_set_tlsext_host_name(emt->ssl, emt->hostname);
	}

	/* Every record of the client fits in one datagram. The server sends a byte stream. */
	const size_t max = rdpudp_get_max_payload(emt->udp);
	if (!emt->server && (max > TLS_RECORD_OVERHEAD + 512))
	{
		if (!SSL_set_max_send_fragment(emt->ssl,
		                               WINPR_ASSERTING_INT_CAST(long, max - TLS_RECORD_OVERHEAD)))
		{
			WLog_Print(emt->log, WLOG_ERROR, "SSL_set_max_send_fragment failed");
			return FALSE;
		}
	}

	emt->tlsStarted = TRUE;
	return emt_run_tls(emt);
}

/* All data that came from the client. A server connection processes this data here, on the thread
 * of the tunnel. The listener only sorts. */
WINPR_ATTR_NODISCARD
static BOOL emt_server_input(rdpEmt* emt)
{
	WINPR_ASSERT(emt);

	BYTE data[UDPFLOW_MAX_DATAGRAM] = WINPR_C_ARRAY_INIT;
	size_t length = 0;
	while (!emt->failed && udpflow_read(emt->flow, data, &length))
	{
		if (rdpudp_get_state(emt->udp) == RDPUDP_STATE_CLOSED)
		{
			if (!rdpudp_accept(emt->udp, emt_flow_send, emt->flow, data, length))
			{
				emt_fail(emt, "cannot accept the client's SYN");
				return FALSE;
			}
			emt->callback(emt->custom, emt, RDPEMT_EVENT_CLAIMED, nullptr, 0);
		}
		else if (!rdpudp_input(emt->udp, data, length))
		{
			emt_fail(emt, "the RDP-UDP connection failed");
			return FALSE;
		}
		if (!emt->tlsStarted && (rdpudp_get_state(emt->udp) == RDPUDP_STATE_ESTABLISHED) &&
		    !emt_start_tls(emt))
		{
			log_ssl_errors(emt, "TLS setup");
			emt_fail(emt, "failed to start TLS");
			return FALSE;
		}
	}
	return !emt->failed;
}

WINPR_ATTR_NODISCARD
static BOOL emt_poll(rdpEmt* emt)
{
	WINPR_ASSERT(emt);

	if (emt->failed)
		return FALSE;

	if (emt->server && !emt_server_input(emt))
		return FALSE;

	if (emt->server && (rdpudp_get_state(emt->udp) == RDPUDP_STATE_CLOSED))
	{
		if (GetTickCount64() - emt->started > RDPEMT_SETUP_TIMEOUT_MS)
		{
			emt_fail(emt, "no SYN from the client");
			return FALSE;
		}
		return TRUE;
	}

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

	if (emt->server && emt->ready && !emt_drain_hold(emt))
	{
		emt_fail(emt, "failed to send tunnel data");
		return FALSE;
	}

	return !emt->failed;
}

/* Waits until a datagram arrives or the timeout in milliseconds passes. Waking up early, or
 * from an interrupted wait, only makes the timers run sooner. */
static void emt_wait_readable(SOCKET sockfd, UINT32 timeout)
{
#if !defined(_WIN32) && defined(WINPR_HAVE_POLL_H)
	/* unlike an fd_set, poll takes any descriptor number */
	struct pollfd pollset = WINPR_C_ARRAY_INIT;
	pollset.fd = WINPR_ASSERTING_INT_CAST(int, sockfd);
	pollset.events = POLLIN;
	const UINT32 ms = (timeout > INT32_MAX) ? INT32_MAX : timeout;
	(void)poll(&pollset, 1, WINPR_ASSERTING_INT_CAST(int, ms));
#else
#if !defined(_WIN32)
	/* an fd_set only holds descriptors below FD_SETSIZE */
	if (sockfd >= FD_SETSIZE)
	{
		Sleep(timeout);
		return;
	}
#endif
	fd_set rset;
	FD_ZERO(&rset);
	FD_SET(sockfd, &rset);
	struct timeval tv = WINPR_C_ARRAY_INIT;
	tv.tv_sec = WINPR_ASSERTING_INT_CAST(long, timeout / 1000);
	tv.tv_usec = WINPR_ASSERTING_INT_CAST(long, (timeout % 1000) * 1000);
#if defined(_WIN32)
	(void)select(0, &rset, nullptr, nullptr, &tv); /* the first argument is ignored */
#else
	(void)select(WINPR_ASSERTING_INT_CAST(int, sockfd) + 1, &rset, nullptr, nullptr, &tv);
#endif
#endif
}

WINPR_ATTR_NODISCARD
static BOOL emt_stopping(rdpEmt* emt)
{
	WINPR_ASSERT(emt);
	return InterlockedCompareExchange(&emt->stop, 0, 0) != 0;
}

WINPR_ATTR_NODISCARD
static DWORD WINAPI emt_thread(LPVOID arg)
{
	rdpEmt* emt = arg;
	WINPR_ASSERT(emt);

	while (!emt_stopping(emt))
	{
		EnterCriticalSection(&emt->lock);
		const UINT32 timeout = rdpudp_get_timeout(emt->udp);
		const SOCKET sockfd = rdpudp_get_socket(emt->udp);
		LeaveCriticalSection(&emt->lock);

		/* A server connection has no socket. Its datagrams wake it through the flow. */
		if (emt->server)
			(void)WaitForSingleObject(freerdp_udp_flow_get_event(emt->flow), timeout);
		else
			emt_wait_readable(sockfd, timeout);

		if (emt_stopping(emt))
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

/* The worker of a tunnel without a thread, see freerdp_udp_flow_check(). */
WINPR_ATTR_NODISCARD
static BOOL emt_worker_check(void* custom)
{
	rdpEmt* emt = custom;
	WINPR_ASSERT(emt);

	EnterCriticalSection(&emt->lock);
	const BOOL rc = emt_poll(emt);
	LeaveCriticalSection(&emt->lock);
	return rc;
}

WINPR_ATTR_NODISCARD
static UINT32 emt_worker_timeout(void* custom)
{
	rdpEmt* emt = custom;
	WINPR_ASSERT(emt);

	EnterCriticalSection(&emt->lock);
	const UINT32 timeout = rdpudp_get_timeout(emt->udp);
	LeaveCriticalSection(&emt->lock);
	return timeout;
}

/* [MS-RDPEUDP] 2.2.2.9: cookieHash is the SHA-256 hash of the securityCookie. It is sent with
 * each 32 bit word byte swapped, which is what Windows accepts. */
WINPR_ATTR_NODISCARD
static BOOL emt_cookie_hash(const BYTE cookie[16], BYTE cookieHash[32])
{
	WINPR_ASSERT(cookie);
	WINPR_ASSERT(cookieHash);

	BYTE digest[32] = WINPR_C_ARRAY_INIT;
	if (!winpr_Digest(WINPR_MD_SHA256, cookie, 16, digest, sizeof(digest)))
		return FALSE;
	wStream dbuffer = WINPR_C_ARRAY_INIT;
	wStream hbuffer = WINPR_C_ARRAY_INIT;
	wStream* ds = Stream_StaticConstInit(&dbuffer, digest, sizeof(digest));
	wStream* hs = Stream_StaticInit(&hbuffer, cookieHash, 32);
	if (!ds || !hs)
		return FALSE;
	for (size_t x = 0; x < 32 / 4; x++)
	{
		/* the Stream_Write macros evaluate their value more than once */
		const UINT32 word = Stream_Get_UINT32_BE(ds);
		Stream_Write_UINT32(hs, word);
	}
	return TRUE;
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

	if (!emt_cookie_hash(emt->cookie, emt->cookieHash))
		goto fail;

	emt->in = Stream_New(nullptr, 16384);
	emt->out = Stream_New(nullptr, 4096);
	if (!emt->in || !emt->out)
		goto fail;

	emt->udp = rdpudp_new(log, emt->cookieHash, emt_receive, emt);
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

rdpEmt* rdpemt_new_server(wLog* log, UINT32 requestId, const BYTE* cookie, X509* cert,
                          EVP_PKEY* key, BOOL manual, rdpEmtEventFn callback, void* custom)
{
	WINPR_ASSERT(log);
	WINPR_ASSERT(cookie);
	WINPR_ASSERT(cert);
	WINPR_ASSERT(key);
	WINPR_ASSERT(callback);

	rdpEmt* emt = calloc(1, sizeof(rdpEmt));
	if (!emt)
		return nullptr;

	emt->log = log;
	emt->server = TRUE;
	emt->requestId = requestId;
	memcpy(emt->cookie, cookie, sizeof(emt->cookie));
	emt->callback = callback;
	emt->custom = custom;
	if (!InitializeCriticalSectionAndSpinCount(&emt->lock, 4000))
	{
		free(emt);
		return nullptr;
	}

	/* The code keeps each reference immediately after it takes the reference, so rdpemt_free drops
	 * the reference on failure. */
	if (X509_up_ref(cert) != 1)
		goto fail;
	emt->cert = cert;
	if (EVP_PKEY_up_ref(key) != 1)
		goto fail;
	emt->key = key;

	if (!emt_cookie_hash(emt->cookie, emt->cookieHash))
		goto fail;

	emt->in = Stream_New(nullptr, 16384);
	emt->out = Stream_New(nullptr, 4096);
	if (!emt->in || !emt->out)
		goto fail;

	emt->udp = rdpudp_new(log, nullptr, emt_receive, emt);
	emt->flow = freerdp_udp_flow_new(emt->cookieHash);
	emt->hold = Queue_New(FALSE, -1, -1);
	if (!emt->udp || !emt->flow || !emt->hold)
		goto fail;
	emt->sendLimit = RDPEMT_SEND_LIMIT;
	udpflow_set_worker(emt->flow, emt_worker_check, emt_worker_timeout, emt);

	emt->started = GetTickCount64();
	if (!manual)
	{
		emt->thread = CreateThread(nullptr, 0, emt_thread, emt, 0, nullptr);
		if (!emt->thread)
			goto fail;
	}
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
	WINPR_ASSERT(addr);

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

	/* not InterlockedExchange: WinPR's reads the old value non-atomically */
	InterlockedIncrement(&emt->stop);
	if (emt->flow)
		udpflow_wake(emt->flow);
	if (emt->thread)
	{
		(void)WaitForSingleObject(emt->thread, INFINITE);
		(void)CloseHandle(emt->thread);
	}

	/* The thread is gone, so nothing sends any more. The listener can now release the flow. This
	 * can close the socket of the flow. */
	freerdp_udp_flow_free(emt->flow);

	if (emt->hold)
	{
		/* Queue_Free() does not free the items of a queue that is exactly full */
		wStream* s = nullptr;
		while ((s = Queue_Dequeue(emt->hold)))
			Stream_Free(s, TRUE);
		Queue_Free(emt->hold);
	}

	SSL_free(emt->ssl); /* frees both BIOs */
	SSL_CTX_free(emt->ctx);
	X509_free(emt->cert);
	EVP_PKEY_free(emt->key);
	rdpudp_free(emt->udp);
	free(emt->hostname);
	free(emt->publicKey);
	Stream_Free(emt->in, TRUE);
	Stream_Free(emt->out, TRUE);
	Stream_Free(emt->batch, TRUE);
	DeleteCriticalSection(&emt->lock);
	free(emt);
}

/* The link is full, or the data that waits for the link must go first. */
WINPR_ATTR_NODISCARD
static BOOL emt_must_hold(const rdpEmt* emt)
{
	WINPR_ASSERT(emt);
	return (Queue_Count(emt->hold) > 0) ||
	       (rdpudp_get_send_backlog(emt->udp) >= rdpudp_get_send_window(emt->udp));
}

BOOL rdpemt_send_data(rdpEmt* emt, const BYTE* data, size_t length)
{
	WINPR_ASSERT(emt);
	WINPR_ASSERT(data || (length == 0));

	BOOL rc = FALSE;
	EnterCriticalSection(&emt->lock);
	if (emt->ready && !emt->failed)
	{
		if (!emt->server)
			rc = emt_write_pdu(emt, RDPTUNNEL_ACTION_DATA, nullptr, 0, data, length);
		else if (emt_must_hold(emt))
			rc = emt_hold(emt, EMT_HOLD_DATA, data, length);
		else
			rc = emt_batch_data(emt, data, length);
		if (!rc)
			emt_fail(emt, "failed to send tunnel data");
	}
	LeaveCriticalSection(&emt->lock);
	return rc;
}

BOOL rdpemt_flush(rdpEmt* emt)
{
	WINPR_ASSERT(emt);

	BOOL rc = TRUE;
	EnterCriticalSection(&emt->lock);
	if (emt->ready && !emt->failed)
	{
		rc = emt_write_batch(emt) && emt_drain_hold(emt);
		if (!rc)
			emt_fail(emt, "failed to send tunnel data");
	}
	LeaveCriticalSection(&emt->lock);
	return rc;
}

rdpUdpFlow* rdpemt_get_flow(const rdpEmt* emt)
{
	WINPR_ASSERT(emt);
	return emt->flow;
}

void rdpemt_get_send_state(rdpEmt* emt, FreeRDP_UdpSendState* state)
{
	WINPR_ASSERT(emt);
	WINPR_ASSERT(state);

	EnterCriticalSection(&emt->lock);
	const size_t max = (rdpudp_get_state(emt->udp) == RDPUDP_STATE_ESTABLISHED)
	                       ? rdpudp_get_max_payload(emt->udp)
	                       : 0;
	const UINT32 rtt = rdpudp_get_rtt(emt->udp);
	state->writeBlocked = emt->writeBlocked;
	state->queuedBytes = emt->holdBytes + rdpudp_get_send_backlog(emt->udp) * max;
	state->rttMs = rtt;
	state->rateBytesPerSecond =
	    (rtt > 0) ? (1000ull * rdpudp_get_send_window(emt->udp) * max) / rtt : 0;
	LeaveCriticalSection(&emt->lock);
}

BOOL rdpemt_is_write_blocked(rdpEmt* emt)
{
	WINPR_ASSERT(emt);

	EnterCriticalSection(&emt->lock);
	const BOOL blocked = emt->writeBlocked;
	LeaveCriticalSection(&emt->lock);
	return blocked;
}

void rdpemt_set_send_limit(rdpEmt* emt, size_t limit)
{
	WINPR_ASSERT(emt);

	EnterCriticalSection(&emt->lock);
	emt->sendLimit = (limit > 0) ? limit : RDPEMT_SEND_LIMIT;
	LeaveCriticalSection(&emt->lock);
}

BOOL rdpemt_set_receiver(rdpEmt* emt, rdpEmtReceiverFn receiver, void* custom, wQueue* pending)
{
	WINPR_ASSERT(emt);
	WINPR_ASSERT(receiver);
	WINPR_ASSERT(pending);

	/* The tunnel thread raises its events while it holds the lock, so pending does not fill up
	 * meanwhile. Once a receiver is set no data goes to pending any more, so whatever waits there
	 * then may be overtaken. */
	EnterCriticalSection(&emt->lock);
	const BOOL rc = emt->receiver || (Queue_Count(pending) == 0);
	if (rc)
	{
		if (!emt->receiver)
			WLog_Print(emt->log, WLOG_DEBUG,
			           "tunnel data goes to its receiver on the tunnel thread");
		emt->receiver = receiver;
		emt->receiverCustom = custom;
	}
	LeaveCriticalSection(&emt->lock);
	return rc;
}

void rdpemt_clear_receiver(rdpEmt* emt)
{
	WINPR_ASSERT(emt);

	/* the tunnel thread hands data up while it holds the lock, so it is out of the receiver now */
	EnterCriticalSection(&emt->lock);
	if (emt->receiver)
		WLog_Print(emt->log, WLOG_DEBUG, "tunnel data goes to the event callback again");
	emt->receiver = nullptr;
	emt->receiverCustom = nullptr;
	LeaveCriticalSection(&emt->lock);
}

UINT32 rdpemt_take_direct_bytes(rdpEmt* emt)
{
	WINPR_ASSERT(emt);

	/* Not the lock: the main loop calls this on every pass. Swapped for 0 with compare-exchange,
	 * WinPR's InterlockedExchange reads the old value non-atomically. */
	LONG bytes = InterlockedCompareExchange(&emt->directBytes, 0, 0);
	while (bytes != 0)
	{
		const LONG seen = InterlockedCompareExchange(&emt->directBytes, 0, bytes);
		if (seen == bytes)
			break;
		bytes = seen;
	}
	/* The counter wraps when the main loop leaves more than 2 GiB untaken. The count stays right
	 * modulo 2^32, like the UINT32 of a bandwidth measurement. */
	return WINPR_CXX_COMPAT_CAST(UINT32, bytes);
}

BOOL rdpemt_send_subheader(rdpEmt* emt, const BYTE* subHeader, size_t length)
{
	WINPR_ASSERT(emt);
	WINPR_ASSERT(subHeader);

	BOOL rc = FALSE;
	EnterCriticalSection(&emt->lock);
	if (emt->ready && !emt->failed)
	{
		if (!emt->server)
			rc = emt_write_pdu(emt, RDPTUNNEL_ACTION_DATA, subHeader, length, nullptr, 0);
		else if (emt_must_hold(emt))
			rc = emt_hold(emt, EMT_HOLD_SUBHEADER, subHeader, length);
		else
			rc = emt_write_subheader(emt, subHeader, length);
		if (!rc)
			emt_fail(emt, "failed to send a tunnel subheader");
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

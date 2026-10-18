/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Client connection finalization and Monitor Layout PDU unit test
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

#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/sysinfo.h>

#include <freerdp/freerdp.h>
#include <freerdp/client.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/update.h>
#include "../../cache/cache.h"

#include "../rdp.h"
#include "../activation.h"
#include "../connection.h"
#include "../transport.h"
#include "../mcs.h"

/* how long to wait for the client PDUs, and how long to watch for PDUs that must not follow */
#define TEST_READ_TIMEOUT_MS 2000
#define TEST_READ_QUIET_MS 50

/* MCS user id of the client, and the PDUVersion bits of the share control header pduType */
#define TEST_USER_ID (MCS_BASE_CHANNEL_ID + 1)
#define TEST_PDU_VERSION (1 << 4)

static UINT32 g_remote_monitors_calls = 0;

static BOOL test_remote_monitors(WINPR_ATTR_UNUSED rdpContext* context,
                                 WINPR_ATTR_UNUSED UINT32 count,
                                 WINPR_ATTR_UNUSED const MONITOR_DEF* monitors)
{
	g_remote_monitors_calls++;
	return TRUE;
}

static rdpContext* test_context_new(BOOL announce_monitor_layout)
{
	RDP_CLIENT_ENTRY_POINTS entry = WINPR_C_ARRAY_INIT;

	entry.Version = RDP_CLIENT_INTERFACE_VERSION;
	entry.Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	entry.ContextSize = sizeof(rdpClientContext);

	rdpContext* context = freerdp_client_context_new(&entry);
	if (!context)
		return nullptr;

	/* minimal client settings from a command line */
	char* argv[] = { (char*)"test", (char*)"/v:127.0.0.1", (char*)"/cert:ignore",
		             (char*)"/sec:tls" };
	if (freerdp_client_settings_parse_command_line_arguments(context->settings, ARRAYSIZE(argv),
	                                                         argv, FALSE) != 0)
	{
		freerdp_client_context_free(context);
		return nullptr;
	}

	/* A client sets up OrderSupport in PreConnect, which this test does not run. Without it, the
	 * frame marker command makes rdp_write_order_capability_set log a warning for each missing
	 * order flag. */
	if (!freerdp_settings_set_bool(context->settings, FreeRDP_FrameMarkerCommandEnabled, FALSE) ||
	    !freerdp_settings_set_bool(context->settings, FreeRDP_SupportMonitorLayoutPdu,
	                               announce_monitor_layout))
	{
		freerdp_client_context_free(context);
		return nullptr;
	}
	return context;
}

/* a connected pair of loopback TCP sockets: the transport expects a TCP socket */
static BOOL tcp_pair(int sv[2])
{
	struct sockaddr_in addr = WINPR_C_ARRAY_INIT;
	socklen_t len = sizeof(addr);
	BOOL rc = FALSE;

	sv[0] = sv[1] = -1;
	const int listener = socket(AF_INET, SOCK_STREAM, 0);
	if (listener < 0)
		return FALSE;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if ((bind(listener, (struct sockaddr*)&addr, sizeof(addr)) != 0) ||
	    (listen(listener, 1) != 0) || (getsockname(listener, (struct sockaddr*)&addr, &len) != 0))
		goto fail;

	sv[0] = socket(AF_INET, SOCK_STREAM, 0);
	if (sv[0] < 0)
		goto fail;
	if (connect(sv[0], (struct sockaddr*)&addr, sizeof(addr)) != 0)
		goto fail;
	sv[1] = accept(listener, nullptr, nullptr);
	rc = (sv[1] >= 0);
fail:
	(void)close(listener);
	if (!rc)
	{
		if (sv[0] >= 0)
			(void)close(sv[0]);
		sv[0] = sv[1] = -1;
	}
	return rc;
}

/* Length of the first TPKT packet in the buffer, or 0 if there is none or it is not complete */
static size_t first_tpkt_length(const BYTE* buffer, size_t used)
{
	if ((used < 4) || (buffer[0] != 0x03))
		return 0;
	const size_t len = ((size_t)buffer[2] << 8) | buffer[3];
	if ((len < 4) || (len > used))
		return 0;
	return len;
}

/* number of complete TPKT packets at the start of buffer */
static size_t count_tpkt_packets(const BYTE* buffer, size_t used)
{
	size_t count = 0;
	size_t pos = 0;

	for (;;)
	{
		const size_t len = first_tpkt_length(&buffer[pos], used - pos);
		if (len == 0)
			break;
		count++;
		pos += len;
	}
	return count;
}

/* Read what the client wrote to fd until the expected number of complete TPKT packets has arrived
 * (or the timeout expired), then watch briefly for data that must not follow. The result does not
 * depend on when the transport hands its writes to the kernel. Returns the number of bytes read. */
static size_t read_client_pdus(int fd, BYTE* buffer, size_t size, size_t expected)
{
	size_t used = 0;
	const UINT64 deadline = GetTickCount64() + TEST_READ_TIMEOUT_MS;
	BOOL quiet = FALSE;

	for (;;)
	{
		int timeout = TEST_READ_QUIET_MS;

		if (count_tpkt_packets(buffer, used) >= expected)
			quiet = TRUE;
		if (!quiet)
		{
			const UINT64 now = GetTickCount64();
			if (now >= deadline)
				break;
			timeout = (int)(deadline - now);
		}

		struct pollfd pfd = WINPR_C_ARRAY_INIT;
		pfd.fd = fd;
		pfd.events = POLLIN;
		const int rc = poll(&pfd, 1, timeout);
		if (rc < 0)
		{
			if (errno == EINTR)
				continue;
			break;
		}
		if (rc == 0)
			break;
		if (used >= size)
			break;
		const ssize_t rd = recv(fd, &buffer[used], size - used, 0);
		if (rd <= 0)
			break;
		used += (size_t)rd;
	}
	return used;
}

/* Header fields of a PDU the client sent ([MS-RDPBCGR] 2.2.8.1.1.1.1 share control header,
 * 2.2.8.1.1.1.2 share data header, 2.2.1.15 and 2.2.1.16 control action) */
static BOOL parse_client_pdu(const BYTE* pkt, size_t len, UINT16* pduType, UINT8* pduType2,
                             UINT16* action)
{
	/* TPKT (4), X.224 data TPDU (3), MCS send data request: choice (1), initiator (2), channel id
	 * (2), flags (1), PER encoded length (1 or 2) */
	size_t pos = 4 + 3;

	*pduType = 0;
	*pduType2 = 0;
	*action = 0;
	if (len < pos + 1 + 2 + 2 + 1 + 2)
		return FALSE;
	if (pkt[pos] != (BYTE)(DomainMCSPDU_SendDataRequest << 2))
		return FALSE;
	pos += 1 + 2 + 2 + 1;
	pos += (pkt[pos] & 0x80) ? 2 : 1;

	/* share control header: total length (2), pdu type (2), pdu source (2) */
	if (len < pos + 6)
		return FALSE;
	*pduType = (UINT16)((pkt[pos + 2] | (pkt[pos + 3] << 8)) & 0x0F);
	if (*pduType != PDU_TYPE_DATA)
		return TRUE;

	/* share data header: share id (4), pad (1), stream id (1), uncompressed length (2), pdu type 2
	 * (1), compressed type (1), compressed length (2) */
	if (len < pos + 6 + 12)
		return FALSE;
	*pduType2 = pkt[pos + 6 + 4 + 1 + 1 + 2];
	if (*pduType2 == DATA_PDU_TYPE_CONTROL)
	{
		if (len < pos + 6 + 12 + 2)
			return FALSE;
		*action = (UINT16)(pkt[pos + 6 + 12] | (pkt[pos + 6 + 12 + 1] << 8));
	}
	return TRUE;
}

typedef struct
{
	UINT16 pduType;
	UINT8 pduType2;
	UINT16 action;
	const char* name;
} ExpectedPdu;

/* The PDUs the client sends in answer to the Demand Active PDU, in this order
 * ([MS-RDPBCGR] 1.3.1.1, 3.2.5.3.13.2) */
static const ExpectedPdu expected_client_pdus[] = {
	{ PDU_TYPE_CONFIRM_ACTIVE, 0, 0, "Confirm Active" },
	{ PDU_TYPE_DATA, DATA_PDU_TYPE_SYNCHRONIZE, 0, "Synchronize" },
	{ PDU_TYPE_DATA, DATA_PDU_TYPE_CONTROL, CTRLACTION_COOPERATE, "Control (Cooperate)" },
	{ PDU_TYPE_DATA, DATA_PDU_TYPE_CONTROL, CTRLACTION_REQUEST_CONTROL,
	  "Control (Request Control)" },
	{ PDU_TYPE_DATA, DATA_PDU_TYPE_FONT_LIST, 0, "Font List" }
};

static BOOL check_client_pdus(const BYTE* buffer, size_t used, BOOL announce_monitor_layout)
{
	size_t pos = 0;

	for (size_t x = 0; x < ARRAYSIZE(expected_client_pdus); x++)
	{
		const ExpectedPdu* exp = &expected_client_pdus[x];
		UINT16 pduType = 0;
		UINT8 pduType2 = 0;
		UINT16 action = 0;

		const size_t len = first_tpkt_length(&buffer[pos], used - pos);
		if (len == 0)
		{
			(void)fprintf(stderr, "announce=%d: PDU %" PRIuz " (%s) is missing\n",
			              announce_monitor_layout, x, exp->name);
			return FALSE;
		}
		if (!parse_client_pdu(&buffer[pos], len, &pduType, &pduType2, &action) ||
		    (pduType != exp->pduType) || (pduType2 != exp->pduType2) || (action != exp->action))
		{
			(void)fprintf(stderr,
			              "announce=%d: PDU %" PRIuz " should be %s (type %" PRIu16
			              ", type2 %" PRIu8 ", action %" PRIu16 "), got type %" PRIu16
			              ", type2 %" PRIu8 ", action %" PRIu16 "\n",
			              announce_monitor_layout, x, exp->name, exp->pduType, exp->pduType2,
			              exp->action, pduType, pduType2, action);
			return FALSE;
		}
		pos += len;
	}

	if (pos != used)
	{
		(void)fprintf(stderr, "announce=%d: %" PRIuz " bytes follow the %" PRIuz " expected PDUs\n",
		              announce_monitor_layout, used - pos, ARRAYSIZE(expected_client_pdus));
		return FALSE;
	}
	return TRUE;
}

/* A server to client data PDU as the client receives it: TPKT, X.224 data TPDU, MCS send data
 * indication, share control header, share data header with the given pduType2, and the body
 * ([MS-RDPBCGR] 2.2.8.1.1.1.2) */
static size_t build_server_data_frame(BYTE* buffer, size_t size, UINT8 pduType2, const BYTE* body,
                                      size_t bodyLen)
{
	const size_t share = 6 + 12 + bodyLen; /* share control header, share data header, body */
	const size_t total = 4 + 3 + 1 + 2 + 2 + 1 + 1 + share;
	wStream sbuffer = WINPR_C_ARRAY_INIT;

	if ((share > 127) || (total > size))
		return 0;

	wStream* s = Stream_StaticInit(&sbuffer, buffer, size);

	Stream_Write_UINT8(s, 0x03); /* TPKT version */
	Stream_Write_UINT8(s, 0x00);
	Stream_Write_UINT16_BE(s, (UINT16)total);

	Stream_Write_UINT8(s, 0x02); /* X.224 data TPDU */
	Stream_Write_UINT8(s, 0xF0);
	Stream_Write_UINT8(s, 0x80);

	Stream_Write_UINT8(s, DomainMCSPDU_SendDataIndication << 2);   /* MCS send data indication */
	Stream_Write_UINT16_BE(s, TEST_USER_ID - MCS_BASE_CHANNEL_ID); /* initiator */
	Stream_Write_UINT16_BE(s, MCS_GLOBAL_CHANNEL_ID);              /* channel id */
	Stream_Write_UINT8(s, 0x70);         /* data priority and segmentation */
	Stream_Write_UINT8(s, (UINT8)share); /* user data length (PER) */

	Stream_Write_UINT16(s, (UINT16)share); /* share control header: total length */
	Stream_Write_UINT16(s, PDU_TYPE_DATA | TEST_PDU_VERSION); /* pduType */
	Stream_Write_UINT16(s, TEST_USER_ID);                     /* PDU source */

	Stream_Write_UINT32(s, 0x000103EA);             /* share data header: share id */
	Stream_Write_UINT8(s, 0);                       /* pad */
	Stream_Write_UINT8(s, 1);                       /* stream id */
	Stream_Write_UINT16(s, (UINT16)(18 + bodyLen)); /* uncompressed length */
	Stream_Write_UINT8(s, pduType2);                /* pduType2 */
	Stream_Write_UINT8(s, 0);                       /* compressed type */
	Stream_Write_UINT16(s, 0);                      /* compressed length */
	Stream_Write(s, body, bodyLen);
	return Stream_GetPosition(s);
}

static BOOL feed_server_pdu(rdpRdp* rdp, UINT8 pduType2, const BYTE* body, size_t bodyLen,
                            state_run_t* pStatus)
{
	BYTE frame[256] = WINPR_C_ARRAY_INIT;
	const size_t len = build_server_data_frame(frame, sizeof(frame), pduType2, body, bodyLen);
	if (len == 0)
		return FALSE;

	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&sbuffer, frame, len);
	*pStatus = rdp_recv_callback(rdp->transport, s, rdp);
	return TRUE;
}

/* Monitor Layout PDU body ([MS-RDPBCGR] 2.2.12.1): monitor count and monitor definitions */
static BOOL feed_monitor_layout_pdu(rdpRdp* rdp, state_run_t* pStatus)
{
	const UINT32 monitors = 2;
	BYTE body[4 + 20 * 2] = WINPR_C_ARRAY_INIT;
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticInit(&sbuffer, body, sizeof(body));

	Stream_Write_UINT32(s, monitors);
	for (UINT32 x = 0; x < monitors; x++)
	{
		Stream_Write_INT32(s, (INT32)(x * 1280)); /* left */
		Stream_Write_INT32(s, 0);                 /* top */
		Stream_Write_INT32(s, (INT32)(x * 1280 + 1279));
		Stream_Write_INT32(s, 799);
		Stream_Write_UINT32(s, (x == 0) ? 1 : 0); /* flags: primary */
	}
	return feed_server_pdu(rdp, DATA_PDU_TYPE_MONITOR_LAYOUT, body, Stream_GetPosition(s), pStatus);
}

/* After the Demand Active PDU the client sends Confirm Active followed by its finalization PDUs
 * (Synchronize, Control Cooperate, Control Request Control, Font List) without waiting for any
 * server PDU, whether or not it announced the Monitor Layout capability ([MS-RDPBCGR]
 * 3.2.5.3.13.2). */
static BOOL test_finalize_after_confirm_active(BOOL announce_monitor_layout)
{
	BOOL rc = FALSE;
	int sv[2] = { -1, -1 };
	rdpContext* context = test_context_new(announce_monitor_layout);
	if (!context)
		return FALSE;

	if (!tcp_pair(sv))
		goto fail;

	rdpRdp* rdp = context->rdp;
	rdp->mcs->userId = TEST_USER_ID; /* as assigned by an MCS attach user confirm */
	if (!transport_attach(rdp->transport, sv[0]))
		goto fail;
	sv[0] = -1; /* owned by the transport now */

	if (!rdp_client_transition_to_state(rdp, CONNECTION_STATE_CAPABILITIES_EXCHANGE_CONFIRM_ACTIVE))
		goto fail;

	{
		/* the stream is not used by rdp_client_connect_confirm_active */
		BYTE unused[1] = WINPR_C_ARRAY_INIT;
		wStream sbuffer = WINPR_C_ARRAY_INIT;
		wStream* s = Stream_StaticInit(&sbuffer, unused, sizeof(unused));
		const state_run_t status = rdp_client_connect_confirm_active(rdp, s);
		if (status != STATE_RUN_SUCCESS)
		{
			(void)fprintf(stderr, "announce=%d: rdp_client_connect_confirm_active failed: %d\n",
			              announce_monitor_layout, (int)status);
			goto fail;
		}
	}

	{
		const CONNECTION_STATE state = rdp_get_state(rdp);
		if (state != CONNECTION_STATE_FINALIZATION_CLIENT_SYNC)
		{
			(void)fprintf(stderr, "announce=%d: expected state %s, got %s\n",
			              announce_monitor_layout,
			              rdp_state_string(CONNECTION_STATE_FINALIZATION_CLIENT_SYNC),
			              rdp_state_string(state));
			goto fail;
		}
	}

	{
		BYTE buffer[8192] = WINPR_C_ARRAY_INIT;
		const size_t used =
		    read_client_pdus(sv[1], buffer, sizeof(buffer), ARRAYSIZE(expected_client_pdus));
		if (!check_client_pdus(buffer, used, announce_monitor_layout))
			goto fail;
	}
	rc = TRUE;
fail:
	if (sv[0] >= 0)
		(void)close(sv[0]);
	if (sv[1] >= 0)
		(void)close(sv[1]);
	/* rdp_client_connect_confirm_active resized the caches, which freerdp_disconnect releases
	 * otherwise */
	cache_free(context->cache);
	context->cache = nullptr;
	freerdp_client_context_free(context);
	return rc;
}

/* The Monitor Layout PDU is accepted when it arrives while the client finalizes or later, once per
 * activation, and the RemoteMonitors callback runs for it. */
static BOOL test_monitor_layout_pdu_accepted(CONNECTION_STATE state)
{
	BOOL rc = FALSE;
	rdpContext* context = test_context_new(TRUE);
	if (!context)
		return FALSE;

	rdpRdp* rdp = context->rdp;
	g_remote_monitors_calls = 0;
	rdp->update->RemoteMonitors = test_remote_monitors;

	state_run_t status = STATE_RUN_SUCCESS;
	if (!rdp_client_transition_to_state(rdp, state))
		goto fail;

	if (!feed_monitor_layout_pdu(rdp, &status) || state_run_failed(status))
	{
		(void)fprintf(stderr, "%s: first Monitor Layout PDU rejected\n", rdp_state_string(state));
		goto fail;
	}
	if ((g_remote_monitors_calls != 1) || !rdp->monitor_layout_pdu)
	{
		(void)fprintf(stderr, "%s: first PDU: %" PRIu32 " callback calls, flag %d\n",
		              rdp_state_string(state), g_remote_monitors_calls, rdp->monitor_layout_pdu);
		goto fail;
	}
	if (rdp_get_state(rdp) != state)
	{
		(void)fprintf(stderr, "%s: state changed to %s\n", rdp_state_string(state),
		              rdp_state_string(rdp_get_state(rdp)));
		goto fail;
	}

	/* a second PDU within the same activation is a protocol violation and is rejected */
	if (!feed_monitor_layout_pdu(rdp, &status) || !state_run_failed(status))
	{
		(void)fprintf(stderr, "%s: duplicate Monitor Layout PDU was not rejected\n",
		              rdp_state_string(state));
		goto fail;
	}
	rc = TRUE;
fail:
	freerdp_client_context_free(context);
	return rc;
}

/* run every test, so that a failure of one does not hide the others; the first failure sets the
 * result */
static void run_test(BOOL success, int number, int* result)
{
	if (!success)
	{
		(void)fprintf(stderr, "test %d failed\n", number);
		if (*result == 0)
			*result = -number;
	}
}

int TestClientMonitorLayout(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	int result = 0;

	run_test(test_finalize_after_confirm_active(TRUE), 1, &result);
	run_test(test_finalize_after_confirm_active(FALSE), 2, &result);
	run_test(test_monitor_layout_pdu_accepted(CONNECTION_STATE_FINALIZATION_CLIENT_SYNC), 3,
	         &result);
	run_test(test_monitor_layout_pdu_accepted(CONNECTION_STATE_ACTIVE), 4, &result);
	return result;
}

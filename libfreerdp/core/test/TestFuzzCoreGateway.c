/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * libFuzzer harness for the RD Gateway client protocol parsers (libfreerdp/core/gateway)
 *
 * Input: [target:1][target specific data]
 *  0 - RPC/RTS PDU header and RTS signature parsing (rts.c, rts_signature.c)
 *  1 - TSG RPC response processing (tsg.c), data: [state:1][flags:1][pdu]
 *  2 - HTTP response parsing (http.c), data: [readContentLength:1][response]
 *  3 - WebSocket frame parsing (websocket.c)
 */

#include <stddef.h>
#include <stdint.h>

#include <openssl/bio.h>

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/wlog.h>

#include <freerdp/client.h>

#include "../rdp.h"
#include "../gateway/http.h"
#include "../gateway/rpc.h"
#include "../gateway/rts.h"
#include "../gateway/rts_signature.h"
#include "../gateway/tsg.h"
#include "../gateway/websocket.h"
#include "../../crypto/tls.h"

static void fuzz_rts(wStream* s)
{
	const RtsPduSignature* known[] = { &RTS_PDU_CONN_A3_SIGNATURE, &RTS_PDU_CONN_C2_SIGNATURE,
		                               &RTS_PDU_OUT_R1_A2_SIGNATURE, &RTS_PDU_PING_SIGNATURE,
		                               &RTS_PDU_FLOW_CONTROL_ACK_SIGNATURE };
	const size_t start = Stream_GetPosition(s);

	rpcconn_hdr_t header = WINPR_C_ARRAY_INIT;
	if (rts_read_pdu_header(s, &header) && (header.common.ptype == PTYPE_RTS))
	{
		RtsPduSignature signature = WINPR_C_ARRAY_INIT;
		if (rts_extract_pdu_signature(&signature, s, &header))
			(void)rts_identify_pdu_signature(&signature, nullptr);
	}
	rts_free_pdu_header(&header, FALSE);

	for (size_t x = 0; x < ARRAYSIZE(known); x++)
	{
		if (!Stream_SetPosition(s, start))
			return;
		(void)rts_match_pdu_signature(known[x], s, nullptr);
	}
}

static void fuzz_tsg(rdpContext* context, wStream* s)
{
	if (Stream_GetRemainingLength(s) < 2)
		return;

	const BYTE state = Stream_Get_UINT8(s);
	const BYTE flags = Stream_Get_UINT8(s);
	const TSG_STATE selectedState = (TSG_STATE)(state % (TSG_STATE_FINAL + 1));
	if (selectedState == TSG_STATE_TUNNEL_CLOSE_PENDING)
		return;

	rdpTsg* tsg = tsg_new(context->rdp->transport);
	if (!tsg)
		return;

	if (tsg_set_state(tsg, selectedState))
	{
		RPC_PDU pdu = WINPR_C_ARRAY_INIT;
		pdu.s = s;
		pdu.Flags = (flags & 0x01) ? RPC_PDU_FLAG_STUB : 0;
		pdu.Type = PTYPE_RESPONSE;
		(void)tsg_recv_pdu(tsg, &pdu);
	}
	tsg_free(tsg);
}

static void fuzz_http(rdpContext* context, wStream* s)
{
	if ((Stream_GetRemainingLength(s) < 1) || (Stream_GetRemainingLength(s) > INT32_MAX))
		return;

	const BOOL readContentLength = (Stream_Get_UINT8(s) & 0x01) != 0;
	HttpContext* http = http_context_new();
	BIO* bio = BIO_new_mem_buf(Stream_ConstPointer(s), (int)Stream_GetRemainingLength(s));
	if (!http || !bio)
		goto fail;

	{
		/* http_response_recv only uses the BIO and context of the TLS connection */
		rdpTls tls = WINPR_C_ARRAY_INIT;
		tls.bio = bio;
		tls.context = context;

		HttpResponse* response = http_response_recv(&tls, readContentLength);
		if (response)
		{
			(void)http_response_get_status_code(response);
			(void)http_response_get_body_length(response);
			(void)http_response_get_body(response);
			(void)http_response_get_auth_token(response, "NTLM");
			(void)http_response_get_auth_token(response, "Negotiate");
			(void)http_response_get_setcookie(response, "RdgAuthToken");
			(void)http_response_get_transfer_encoding(response);
			(void)http_response_extract_cookies(response, http);
			(void)http_context_enable_websocket_upgrade(http, TRUE);
			(void)http_response_is_websocket(http, response);
			http_response_free(response);
		}
	}

fail:
	BIO_free(bio);
	http_context_free(http);
}

static void fuzz_websocket(wStream* s)
{
	if (Stream_GetRemainingLength(s) > INT32_MAX)
		return;

	websocket_context* ws = websocket_context_new();
	/* read-only: replies (pong/close) fail instead of being read back */
	BIO* bio = BIO_new_mem_buf(Stream_ConstPointer(s), (int)Stream_GetRemainingLength(s));
	if (!ws || !bio)
		goto fail;

	for (size_t x = 0; x < 64; x++)
	{
		BYTE buffer[1024] = WINPR_C_ARRAY_INIT;
		if (websocket_context_read(ws, bio, buffer, sizeof(buffer)) <= 0)
			break;
	}

fail:
	BIO_free(bio);
	websocket_context_free(ws);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if ((size < 1) || (size > (1u << 20)))
		return 0;

	wLog* root = WLog_GetRoot();
	(void)WLog_SetLogLevel(root, WLOG_TRACE);
	(void)WLog_SetLogAppenderType(root, WLOG_APPENDER_CALLBACK);

	wStream buffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&buffer, data, size);
	const BYTE target = Stream_Get_UINT8(s);

	switch (target % 4)
	{
		case 0:
			fuzz_rts(s);
			break;
		case 3:
			fuzz_websocket(s);
			break;
		default:
		{
			RDP_CLIENT_ENTRY_POINTS entry = WINPR_C_ARRAY_INIT;
			entry.Version = RDP_CLIENT_INTERFACE_VERSION;
			entry.Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
			entry.ContextSize = sizeof(rdpContext);

			rdpContext* context = freerdp_client_context_new(&entry);
			if (!context)
				break;

			/* bound the time spent waiting for more (non existing) HTTP data */
			if (freerdp_settings_set_uint32(context->settings, FreeRDP_GatewayResponseTimeout, 1))
			{
				if ((target % 4) == 1)
					fuzz_tsg(context, s);
				else
					fuzz_http(context, s);
			}
			freerdp_client_context_free(context);
		}
		break;
	}

	return 0;
}

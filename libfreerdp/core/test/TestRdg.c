/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * HTTP RD Gateway channel reuse regression tests
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/environment.h>
#include <winpr/sysinfo.h>

#include <freerdp/error.h>

#include "../connection.h"
#include "../gateway/rdg_private.h"
#include "../tcp.h"
#include "../utils.h"

#define CHANNEL_CREATE 0x08
#define CHANNEL_RESPONSE 0x09
#define DATA 0x0A
#define SERVICE_MESSAGE 0x0B
#define KEEPALIVE 0x0D
#define CLOSE_CHANNEL 0x10
#define CLOSE_RESPONSE 0x11

#define CHECK(cond)                                                          \
	do                                                                       \
	{                                                                        \
		if (!(cond))                                                         \
		{                                                                    \
			(void)fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #cond); \
			goto fail;                                                       \
		}                                                                    \
	} while (0)

typedef struct
{
	UINT16 expected;
	BYTE reply[256];
	size_t length;
} GatewayStep;

typedef struct
{
	freerdp* instance;
	rdpRdg* rdg;
	BIO_METHOD* methods;
	GatewayStep steps[12];
	size_t stepCount;
	size_t stepIndex;
	BYTE wire[8192];
	size_t wireLength;
	size_t parsed;
	BYTE incoming[8192];
	size_t incomingLength;
	size_t readOffset;
	size_t maxRead;
	size_t maxWrite;
	size_t readCalls;
	size_t retryEvery;
	size_t blockedWrites;
	size_t waits;
	UINT64 readableAfter;
	UINT32 responseDelay;
	BOOL chunked;
	BOOL failed;
	BOOL cancelRead;
	BOOL cancelWrite;
	BOOL fatalWrite;
	BOOL transportOwnsRdg;
	size_t serviceMessages;
} Gateway;

typedef struct
{
	rdpContext context;
	Gateway* gateway;
} GatewayContext;

static BOOL append_bytes(BYTE* dst, size_t capacity, size_t* length, const void* data, size_t size)
{
	if (size > capacity - *length)
		return FALSE;
	memcpy(dst + *length, data, size);
	*length += size;
	return TRUE;
}

static BOOL enqueue_reply(Gateway* gw, const GatewayStep* step)
{
	if (!gw->chunked)
		return append_bytes(gw->incoming, sizeof(gw->incoming), &gw->incomingLength, step->reply,
		                    step->length);

	/* Split across HTTP chunks as well as across BIO reads. */
	for (size_t offset = 0; offset < step->length;)
	{
		char header[16] = WINPR_C_ARRAY_INIT;
		const size_t block = MIN(5, step->length - offset);
		const int n = snprintf(header, sizeof(header), "%" PRIXz "\r\n", block);
		if ((n <= 0) ||
		    !append_bytes(gw->incoming, sizeof(gw->incoming), &gw->incomingLength, header,
		                  (size_t)n) ||
		    !append_bytes(gw->incoming, sizeof(gw->incoming), &gw->incomingLength,
		                  step->reply + offset, block) ||
		    !append_bytes(gw->incoming, sizeof(gw->incoming), &gw->incomingLength, "\r\n", 2))
			return FALSE;
		offset += block;
	}
	return TRUE;
}

static BOOL process_writes(Gateway* gw)
{
	while (gw->parsed < gw->wireLength)
	{
		size_t end = gw->parsed;
		while ((end + 1 < gw->wireLength) &&
		       ((gw->wire[end] != '\r') || (gw->wire[end + 1] != '\n')))
			end++;
		if (end + 1 >= gw->wireLength)
			return TRUE;
		char header[16] = WINPR_C_ARRAY_INIT;
		const size_t headerLength = end - gw->parsed;
		if (headerLength >= sizeof(header))
			return FALSE;
		memcpy(header, gw->wire + gw->parsed, headerLength);
		const unsigned long length = strtoul(header, nullptr, 16);
		if ((length < 8) || (length > 1024))
			return FALSE;
		const size_t body = end + 2;
		if (gw->wireLength - body < length + 2)
			return TRUE;
		if (gw->stepIndex >= gw->stepCount)
			return FALSE;
		GatewayStep* step = &gw->steps[gw->stepIndex];
		const UINT16 type = (UINT16)(gw->wire[body] | (gw->wire[body + 1] << 8));
		if (step->expected != type)
			return FALSE;
		if (!enqueue_reply(gw, step))
			return FALSE;
		gw->readableAfter = GetTickCount64() + gw->responseDelay;
		gw->stepIndex++;
		gw->parsed = body + length + 2;
	}
	return TRUE;
}

static int gateway_write(BIO* bio, const char* data, int length)
{
	Gateway* gw = BIO_get_data(bio);
	BIO_clear_retry_flags(bio);
	if (gw->cancelWrite)
	{
		if (!freerdp_abort_connect_context(gw->instance->context))
			gw->failed = TRUE;
		BIO_set_retry_write(bio);
		return -1;
	}
	if (gw->fatalWrite)
		return -1;
	if (gw->blockedWrites > 0)
	{
		gw->blockedWrites--;
		BIO_set_retry_write(bio);
		return -1;
	}
	const size_t block = MIN((size_t)length, gw->maxWrite);
	if (!append_bytes(gw->wire, sizeof(gw->wire), &gw->wireLength, data, block) ||
	    !process_writes(gw))
	{
		gw->failed = TRUE;
		return -1;
	}
	return (int)block;
}

static int gateway_read(BIO* bio, char* data, int length)
{
	Gateway* gw = BIO_get_data(bio);
	BIO_clear_retry_flags(bio);
	gw->readCalls++;
	if (gw->cancelRead && !freerdp_abort_connect_context(gw->instance->context))
		gw->failed = TRUE;
	if (gw->cancelRead || (GetTickCount64() < gw->readableAfter) ||
	    ((gw->retryEvery > 0) && (gw->readCalls % gw->retryEvery == 0)) ||
	    (gw->readOffset == gw->incomingLength))
	{
		BIO_set_retry_read(bio);
		return -1;
	}
	const size_t block = MIN(MIN((size_t)length, gw->maxRead), gw->incomingLength - gw->readOffset);
	memcpy(data, gw->incoming + gw->readOffset, block);
	gw->readOffset += block;
	return (int)block;
}

static long gateway_ctrl(BIO* bio, int cmd, long value, WINPR_ATTR_UNUSED void* arg)
{
	Gateway* gw = BIO_get_data(bio);
	switch (cmd)
	{
		case BIO_CTRL_FLUSH:
			return 1;
		case BIO_C_WAIT_READ:
		case BIO_C_WAIT_WRITE:
			gw->waits++;
			Sleep((DWORD)MIN(1, value));
			return 1;
		default:
			return 0;
	}
}

static GatewayStep* add_step(Gateway* gw, UINT16 expected)
{
	if (gw->stepCount >= ARRAYSIZE(gw->steps))
		return nullptr;
	GatewayStep* step = &gw->steps[gw->stepCount++];
	step->expected = expected;
	return step;
}

static BOOL add_reply(GatewayStep* step, UINT16 type, UINT32 result)
{
	if (!step)
		return FALSE;
	const size_t length = type == KEEPALIVE ? 8 : type == CHANNEL_RESPONSE ? 16 : 12;
	if (length > sizeof(step->reply) - step->length)
		return FALSE;
	wStream buffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticInit(&buffer, step->reply + step->length, length);
	Stream_Write_UINT16(s, type);
	Stream_Write_UINT16(s, 0);
	Stream_Write_UINT32(s, (UINT32)length);
	if (length >= 12)
		Stream_Write_UINT32(s, result);
	if (length >= 16)
		Stream_Write_UINT32(s, 0);
	step->length += length;
	return TRUE;
}

static void gateway_free(Gateway* gw)
{
	if (!gw)
		return;
	if (!gw->transportOwnsRdg)
		rdg_free(gw->rdg);
	if (gw->instance)
	{
		freerdp_context_free(gw->instance);
		freerdp_free(gw->instance);
	}
	BIO_meth_free(gw->methods);
	free(gw);
}

static Gateway* gateway_new(BOOL buffered, BOOL chunked)
{
	Gateway* gw = calloc(1, sizeof(Gateway));
	if (!gw)
		return nullptr;
	gw->maxRead = 1024;
	gw->maxWrite = 1024;
	gw->chunked = chunked;
	gw->instance = freerdp_new();
	if (!gw->instance)
		goto fail;
	gw->instance->ContextSize = sizeof(GatewayContext);
	if (!freerdp_context_new(gw->instance))
		goto fail;
	((GatewayContext*)gw->instance->context)->gateway = gw;
	rdpSettings* settings = gw->instance->context->settings;
	if (!freerdp_settings_set_string(settings, FreeRDP_GatewayHostname, "gateway.example") ||
	    !freerdp_settings_set_string(settings, FreeRDP_ServerHostname, "session.example") ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GatewayEnabled, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpTransport, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_GatewayResponseTimeout, 2000))
		goto fail;
	gw->rdg = rdg_new(gw->instance->context);
	gw->methods = BIO_meth_new(BIO_TYPE_SOURCE_SINK, "RDG test gateway");
	if (!gw->rdg || !gw->methods || !BIO_meth_set_read(gw->methods, gateway_read) ||
	    !BIO_meth_set_write(gw->methods, gateway_write) ||
	    !BIO_meth_set_ctrl(gw->methods, gateway_ctrl))
		goto fail;
	gw->rdg->tlsOut->bio = BIO_new(gw->methods);
	gw->rdg->tlsIn->bio = BIO_new(gw->methods);
	if (!gw->rdg->tlsOut->bio || !gw->rdg->tlsIn->bio)
		goto fail;
	BIO_set_data(gw->rdg->tlsOut->bio, gw);
	BIO_set_data(gw->rdg->tlsIn->bio, gw);
	BIO_set_init(gw->rdg->tlsOut->bio, 1);
	BIO_set_init(gw->rdg->tlsIn->bio, 1);
	if (buffered)
	{
		BIO* b = BIO_new(BIO_s_buffered_socket());
		if (!b)
			goto fail;
		gw->rdg->tlsIn->bio = BIO_push(b, gw->rdg->tlsIn->bio);
	}
	gw->rdg->transferEncoding.httpTransferEncoding =
	    chunked ? TransferEncodingChunked : TransferEncodingIdentity;
	gw->rdg->state = RDG_CLIENT_STATE_OPENED;
	BIO* old = rdg_get_front_bio_and_take_ownership(gw->rdg);
	if (!rdg_detach_for_redirect(gw->rdg))
	{
		BIO_free_all(old);
		goto fail;
	}
	BIO_free_all(old);
	return gw;
fail:
	gateway_free(gw);
	return nullptr;
}

static BOOL test_reopen(BOOL buffered, BOOL chunked, BOOL crossed)
{
	BOOL rc = FALSE;
	Gateway* gw = gateway_new(buffered, chunked);
	CHECK(gw);
	gw->maxRead = 1;
	gw->maxWrite = 3;
	gw->retryEvery = 3;
	gw->blockedWrites = 2;
	GatewayStep* close = add_step(gw, CLOSE_CHANNEL);
	CHECK(add_reply(close, DATA, 0));
	if (crossed)
	{
		CHECK(add_reply(close, CLOSE_CHANNEL, 0));
		CHECK(add_step(gw, CLOSE_RESPONSE));
	}
	CHECK(add_reply(close, CLOSE_RESPONSE, 0));
	CHECK(add_reply(add_step(gw, CHANNEL_CREATE), CHANNEL_RESPONSE, S_OK));
	CHECK(rdg_reopen_channel(gw->rdg));
	CHECK(!gw->failed && (gw->stepIndex == gw->stepCount));
	CHECK(gw->rdg->frontBio && !gw->rdg->closeResponsePending && !gw->rdg->channelClosed);
	CHECK(gw->rdg->redirectDeadline == 0);
	CHECK(BIO_wpending(gw->rdg->tlsIn->bio) == 0);
	rc = TRUE;
fail:
	gateway_free(gw);
	return rc;
}

static BOOL test_missing_close_response(void)
{
	BOOL rc = FALSE;
	Gateway* gw = gateway_new(TRUE, FALSE);
	CHECK(gw);
	CHECK(freerdp_settings_set_uint32(gw->instance->context->settings,
	                                  FreeRDP_GatewayResponseTimeout, 30));
	CHECK(add_reply(add_step(gw, CLOSE_CHANNEL), CLOSE_CHANNEL, 0));
	CHECK(add_step(gw, CLOSE_RESPONSE));
	CHECK(!rdg_reopen_channel(gw->rdg));
	CHECK(!gw->failed && (gw->stepIndex == 2));
	CHECK(gw->rdg->closeResponsePending && !gw->rdg->frontBio);
	CHECK(freerdp_get_last_error(gw->instance->context) == FREERDP_ERROR_CONNECT_TRANSPORT_FAILED);
	rc = TRUE;
fail:
	gateway_free(gw);
	return rc;
}

static BOOL test_channel_result(UINT32 result)
{
	BOOL rc = FALSE;
	Gateway* gw = gateway_new(TRUE, FALSE);
	CHECK(gw);
	CHECK(add_reply(add_step(gw, CLOSE_CHANNEL), CLOSE_RESPONSE, S_OK));
	CHECK(add_reply(add_step(gw, CHANNEL_CREATE), CHANNEL_RESPONSE, result));
	CHECK(!rdg_reopen_channel(gw->rdg));
	CHECK(!gw->failed && !gw->rdg->frontBio);
	CHECK(freerdp_get_last_error(gw->instance->context) == result);
	rc = TRUE;
fail:
	gateway_free(gw);
	return rc;
}

static BOOL test_keepalive_failure(void)
{
	BOOL rc = FALSE;
	Gateway* gw = gateway_new(FALSE, FALSE);
	CHECK(gw);
	GatewayStep* close = add_step(gw, CLOSE_CHANNEL);
	CHECK(add_reply(close, KEEPALIVE, 0));
	CHECK(add_reply(close, CLOSE_RESPONSE, 0));
	/* No write step for the keepalive reply: the fake socket fails that write. */
	CHECK(!rdg_reopen_channel(gw->rdg));
	CHECK(gw->failed && (gw->stepIndex == 1));
	CHECK(gw->readOffset == 8);
	CHECK(!gw->rdg->frontBio);
	rc = TRUE;
fail:
	gateway_free(gw);
	return rc;
}

static BOOL test_budget_and_cancel(BOOL opening, BOOL cancel, BOOL writing)
{
	BOOL rc = FALSE;
	Gateway* gw = gateway_new(TRUE, TRUE);
	CHECK(gw);
	CHECK(freerdp_settings_set_uint32(gw->instance->context->settings,
	                                  FreeRDP_GatewayResponseTimeout, 40));
	gw->rdg->channelClosed = opening;
	gw->cancelRead = cancel && !writing;
	gw->cancelWrite = cancel && writing;
	if (writing)
		gw->blockedWrites = SIZE_MAX;
	CHECK(add_step(gw, opening ? CHANNEL_CREATE : CLOSE_CHANNEL));
	const UINT64 start = GetTickCount64();
	CHECK(!rdg_reopen_channel(gw->rdg));
	const UINT64 elapsed = GetTickCount64() - start;
	CHECK(elapsed < 1000);
	CHECK(cancel || (elapsed >= 40));
	CHECK(!gw->failed && (gw->rdg->redirectDeadline == 0));
	CHECK(freerdp_get_last_error(gw->instance->context) ==
	      (cancel ? FREERDP_ERROR_CONNECT_CANCELLED : FREERDP_ERROR_CONNECT_TRANSPORT_FAILED));
	rc = TRUE;
fail:
	gateway_free(gw);
	return rc;
}

static BOOL test_delayed_reply(void)
{
	BOOL rc = FALSE;
	Gateway* gw = gateway_new(TRUE, TRUE);
	CHECK(gw);
	CHECK(freerdp_settings_set_uint32(gw->instance->context->settings,
	                                  FreeRDP_GatewayResponseTimeout, 500));
	gw->responseDelay = 60;
	CHECK(add_reply(add_step(gw, CLOSE_CHANNEL), CLOSE_RESPONSE, S_OK));
	CHECK(add_reply(add_step(gw, CHANNEL_CREATE), CHANNEL_RESPONSE, S_OK));
	CHECK(rdg_reopen_channel(gw->rdg));
	CHECK(!gw->failed && (gw->waits > 0));
	rc = TRUE;
fail:
	gateway_free(gw);
	return rc;
}

static BOOL test_service_message(freerdp* instance, UINT32 type, BOOL mandatory, BOOL consent,
                                 size_t length, const WCHAR* message)
{
	Gateway* gw = ((GatewayContext*)instance->context)->gateway;
	WINPR_UNUSED(mandatory);
	WINPR_UNUSED(consent);
	if (!gw || (type != GATEWAY_MESSAGE_SERVICE) || (length != 2) || !message ||
	    (message[0] != 'x'))
		return FALSE;
	gw->serviceMessages++;
	return TRUE;
}

static BOOL test_control_messages(void)
{
	BOOL rc = FALSE;
	Gateway* gw = gateway_new(TRUE, TRUE);
	CHECK(gw);
	gw->instance->PresentGatewayMessage = test_service_message;
	GatewayStep* close = add_step(gw, CLOSE_CHANNEL);
	CHECK(add_reply(close, DATA, 0));
	CHECK(add_reply(close, KEEPALIVE, 0));
	CHECK(add_reply(close, SERVICE_MESSAGE, 0x00780002));
	CHECK(add_reply(close, CLOSE_RESPONSE, S_OK));
	CHECK(add_step(gw, KEEPALIVE));
	CHECK(add_reply(add_step(gw, CHANNEL_CREATE), CHANNEL_RESPONSE, S_OK));
	CHECK(rdg_reopen_channel(gw->rdg));
	CHECK(!gw->failed && (gw->stepIndex == gw->stepCount) && (gw->serviceMessages == 1));
	rc = TRUE;
fail:
	gateway_free(gw);
	return rc;
}

static BOOL test_already_closed(void)
{
	BOOL rc = FALSE;
	Gateway* gw = gateway_new(TRUE, FALSE);
	CHECK(gw);
	gw->rdg->channelClosed = TRUE;
	CHECK(add_reply(add_step(gw, CHANNEL_CREATE), CHANNEL_RESPONSE, S_OK));
	CHECK(rdg_reopen_channel(gw->rdg));
	CHECK(!gw->failed && (gw->stepIndex == 1) && !gw->rdg->channelClosed);
	rc = TRUE;
fail:
	gateway_free(gw);
	return rc;
}

static BOOL test_two_redirects(void)
{
	BOOL rc = FALSE;
	Gateway* gw = gateway_new(TRUE, TRUE);
	CHECK(gw);
	for (size_t i = 0; i < 2; i++)
	{
		CHECK(add_reply(add_step(gw, CLOSE_CHANNEL), CLOSE_RESPONSE, S_OK));
		CHECK(add_reply(add_step(gw, CHANNEL_CREATE), CHANNEL_RESPONSE, S_OK));
	}
	CHECK(SetEnvironmentVariableA("FREERDP_RDG_REUSE_TUNNEL", "1"));
	transport_stage_rdg_redirect(gw->instance->context->rdp->transport, gw->rdg);
	gw->transportOwnsRdg = TRUE;
	for (size_t i = 0; i < 2; i++)
	{
		rdpTransport* transport = gw->instance->context->rdp->transport;
		transport_set_gateway_enabled(transport, TRUE);
		CHECK(transport_connect(transport, "session.example", 3389, 1000));
		CHECK(!transport_has_rdg_redirect(transport));
		CHECK(transport_take_rdg_for_redirect(transport) == gw->rdg);
		gw->transportOwnsRdg = FALSE;
		CHECK(rdp_client_disconnect_and_clear(gw->instance->context->rdp));
		transport_stage_rdg_redirect(gw->instance->context->rdp->transport, gw->rdg);
		gw->transportOwnsRdg = TRUE;
	}
	CHECK(!gw->failed && (gw->stepIndex == 4));
	rc = TRUE;
fail:
	gateway_free(gw);
	return rc;
}

static BOOL test_transport_failure(void)
{
	BOOL rc = FALSE;
	Gateway* gw = gateway_new(TRUE, FALSE);
	CHECK(gw);
	CHECK(add_reply(add_step(gw, CLOSE_CHANNEL), CLOSE_RESPONSE, S_OK));
	CHECK(add_reply(add_step(gw, CHANNEL_CREATE), CHANNEL_RESPONSE, 0x80004005));
	rdpTransport* transport = freerdp_get_transport(gw->instance->context);
	transport_stage_rdg_redirect(transport, gw->rdg);
	gw->transportOwnsRdg = TRUE;
	transport_set_gateway_enabled(transport, TRUE);
	CHECK(!transport_connect(transport, "session.example", 3389, 1000));
	CHECK(!gw->failed);
	CHECK(!transport_has_rdg_redirect(transport) && (gw->stepIndex == 2));
	CHECK(!transport_take_rdg_for_redirect(transport));
	CHECK(freerdp_get_last_error(gw->instance->context) == 0x80004005);
	rc = TRUE;
fail:
	gateway_free(gw);
	return rc;
}

static int test_cases(void)
{
	for (size_t b = 0; b < 2; b++)
	{
		for (size_t c = 0; c < 2; c++)
		{
			if (!test_reopen(b != 0, c != 0, FALSE) || !test_reopen(b != 0, c != 0, TRUE))
				return -1;
		}
	}
	if (!test_missing_close_response() || !test_channel_result(S_FALSE) ||
	    !test_channel_result(0x80004005) || !test_keepalive_failure() || !test_delayed_reply() ||
	    !test_two_redirects() || !test_control_messages() || !test_already_closed() ||
	    !test_transport_failure())
		return -1;
	for (size_t o = 0; o < 2; o++)
	{
		for (size_t c = 0; c < 2; c++)
		{
			for (size_t w = 0; w < 2; w++)
			{
				if (!test_budget_and_cancel(o != 0, c != 0, w != 0))
					return -1;
			}
		}
	}
	return 0;
}

int TestRdg(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	const char* value = getenv("FREERDP_RDG_REUSE_TUNNEL");
	char* previous = value ? _strdup(value) : nullptr;
	if (value && !previous)
		return -1;
	const int rc = test_cases();
	const BOOL restored = SetEnvironmentVariableA("FREERDP_RDG_REUSE_TUNNEL", previous);
	free(previous);
	return restored ? rc : -1;
}

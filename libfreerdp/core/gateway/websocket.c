/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Websocket Framing
 *
 * Copyright 2023 Michael Saxl <mike@mwsys.mine.bz>
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

#include "websocket.h"
#include <freerdp/log.h>
#include "../tcp.h"

#define TAG FREERDP_TAG("core.gateway.websocket")

struct s_websocket_context
{
	size_t payloadLength;
	uint32_t maskingKey;
	BOOL masking;
	BOOL closeSent;
	BYTE opcode;
	BYTE fragmentOriginalOpcode;
	BYTE lengthAndMaskPosition;
	WEBSOCKET_STATE state;
	wStream* responseStreamBuffer;
};

static int websocket_write_all(BIO* bio, const BYTE* data, size_t length);

BOOL websocket_context_mask_and_send(BIO* bio, wStream* sPacket, wStream* sDataPacket,
                                     UINT32 maskingKey)
{
	const size_t len = Stream_Length(sDataPacket);
	Stream_SetPosition(sDataPacket, 0);

	if (!Stream_EnsureRemainingCapacity(sPacket, len))
		return FALSE;

	/* mask as much as possible with 32bit access */
	size_t streamPos = 0;
	for (; streamPos + 4 <= len; streamPos += 4)
	{
		const uint32_t data = Stream_Get_UINT32(sDataPacket);
		Stream_Write_UINT32(sPacket, data ^ maskingKey);
	}

	/* mask the rest byte by byte */
	for (; streamPos < len; streamPos++)
	{
		BYTE data = 0;
		BYTE* partialMask = ((BYTE*)&maskingKey) + (streamPos % 4);
		Stream_Read_UINT8(sDataPacket, data);
		Stream_Write_UINT8(sPacket, data ^ *partialMask);
	}

	Stream_SealLength(sPacket);

	ERR_clear_error();
	const size_t size = Stream_Length(sPacket);
	const int status = websocket_write_all(bio, Stream_Buffer(sPacket), size);
	Stream_Free(sPacket, TRUE);

	if ((status < 0) || ((size_t)status != size))
		return FALSE;

	return TRUE;
}

wStream* websocket_context_packet_new(size_t len, WEBSOCKET_OPCODE opcode, UINT32* pMaskingKey)
{
	WINPR_ASSERT(pMaskingKey);
	if (len > INT_MAX)
		return NULL;

	size_t fullLen = 0;
	if (len < 126)
		fullLen = len + 6; /* 2 byte "mini header" + 4 byte masking key */
	else if (len < 0x10000)
		fullLen = len + 8; /* 2 byte "mini header" + 2 byte length + 4 byte masking key */
	else
		fullLen = len + 14; /* 2 byte "mini header" + 8 byte length + 4 byte masking key */

	wStream* sWS = Stream_New(NULL, fullLen);
	if (!sWS)
		return NULL;

	UINT32 maskingKey = 0;
	winpr_RAND(&maskingKey, sizeof(maskingKey));

	Stream_Write_UINT8(sWS, (UINT8)(WEBSOCKET_FIN_BIT | opcode));
	if (len < 126)
		Stream_Write_UINT8(sWS, (UINT8)len | WEBSOCKET_MASK_BIT);
	else if (len < 0x10000)
	{
		Stream_Write_UINT8(sWS, 126 | WEBSOCKET_MASK_BIT);
		Stream_Write_UINT16_BE(sWS, (UINT16)len);
	}
	else
	{
		Stream_Write_UINT8(sWS, 127 | WEBSOCKET_MASK_BIT);
		Stream_Write_UINT32_BE(sWS, 0); /* payload is limited to INT_MAX */
		Stream_Write_UINT32_BE(sWS, (UINT32)len);
	}
	Stream_Write_UINT32(sWS, maskingKey);
	*pMaskingKey = maskingKey;
	return sWS;
}

BOOL websocket_context_write_wstream(websocket_context* context, BIO* bio, wStream* sPacket,
                                     WEBSOCKET_OPCODE opcode)
{
	WINPR_ASSERT(context);

	if (context->closeSent)
		return FALSE;

	if (opcode == WebsocketCloseOpcode)
		context->closeSent = TRUE;

	WINPR_ASSERT(bio);
	WINPR_ASSERT(sPacket);

	const size_t len = Stream_Length(sPacket);
	uint32_t maskingKey = 0;
	wStream* sWS = websocket_context_packet_new(len, opcode, &maskingKey);
	if (!sWS)
		return FALSE;

	return websocket_context_mask_and_send(bio, sWS, sPacket, maskingKey);
}

int websocket_write_all(BIO* bio, const BYTE* data, size_t length)
{
	WINPR_ASSERT(bio);
	WINPR_ASSERT(data);
	size_t offset = 0;

	if (length > INT32_MAX)
		return -1;

	while (offset < length)
	{
		ERR_clear_error();
		const size_t diff = length - offset;
		int status = BIO_write(bio, &data[offset], (int)diff);

		if (status > 0)
			offset += (size_t)status;
		else
		{
			if (!BIO_should_retry(bio))
				return -1;

			if (BIO_write_blocked(bio))
			{
				const long rstatus = BIO_wait_write(bio, 100);
				if (rstatus < 0)
					return -1;
			}
			else if (BIO_read_blocked(bio))
				return -2; /* Abort write, there is data that must be read */
			else
				USleep(100);
		}
	}

	return (int)length;
}

int websocket_context_write(websocket_context* context, BIO* bio, const BYTE* buf, int isize,
                            WEBSOCKET_OPCODE opcode)
{
	WINPR_ASSERT(bio);
	WINPR_ASSERT(buf);

	if (isize < 0)
		return -1;

	wStream sbuffer = { 0 };
	wStream* s = Stream_StaticConstInit(&sbuffer, buf, (size_t)isize);
	if (!websocket_context_write_wstream(context, bio, s, opcode))
		return -2;
	return isize;
}

static int websocket_read_data(BIO* bio, BYTE* pBuffer, size_t size,
                               websocket_context* encodingContext)
{
	int status = 0;

	WINPR_ASSERT(bio);
	WINPR_ASSERT(pBuffer);
	WINPR_ASSERT(encodingContext);

	if (encodingContext->payloadLength == 0)
	{
		encodingContext->state = WebsocketStateOpcodeAndFin;
		return 0;
	}

	const size_t rlen =
	    (encodingContext->payloadLength < size ? encodingContext->payloadLength : size);
	if (rlen > INT32_MAX)
		return -1;

	ERR_clear_error();
	status = BIO_read(bio, pBuffer, (int)rlen);
	if ((status <= 0) || ((size_t)status > encodingContext->payloadLength))
		return status;

	encodingContext->payloadLength -= (size_t)status;

	if (encodingContext->payloadLength == 0)
		encodingContext->state = WebsocketStateOpcodeAndFin;

	return status;
}

static int websocket_read_wstream(BIO* bio, websocket_context* encodingContext)
{
	WINPR_ASSERT(bio);
	WINPR_ASSERT(encodingContext);

	wStream* s = encodingContext->responseStreamBuffer;
	WINPR_ASSERT(s);

	if (encodingContext->payloadLength == 0)
	{
		encodingContext->state = WebsocketStateOpcodeAndFin;
		return 0;
	}

	if (!Stream_EnsureRemainingCapacity(s, encodingContext->payloadLength))
	{
		WLog_WARN(TAG,
		          "wStream::capacity [%" PRIuz "] != encodingContext::paylaodLangth [%" PRIuz "]",
		          Stream_GetRemainingCapacity(s), encodingContext->payloadLength);
		return -1;
	}

	const int status = websocket_read_data(bio, Stream_Pointer(s), Stream_GetRemainingCapacity(s),
	                                       encodingContext);
	if (status < 0)
		return status;

	if (!Stream_SafeSeek(s, (size_t)status))
		return -1;

	return status;
}

static BOOL websocket_reply_close(BIO* bio, websocket_context* context, wStream* s)
{
	WINPR_ASSERT(bio);

	return websocket_context_write_wstream(context, bio, s, WebsocketCloseOpcode);
}

static BOOL websocket_reply_pong(BIO* bio, websocket_context* context, wStream* s)
{
	WINPR_ASSERT(bio);
	WINPR_ASSERT(s);

	if (Stream_GetPosition(s) != 0)
		return websocket_context_write_wstream(context, bio, s, WebsocketPongOpcode);

	return websocket_reply_close(bio, context, NULL);
}

static int websocket_handle_payload(BIO* bio, BYTE* pBuffer, size_t size,
                                    websocket_context* encodingContext)
{
	int status = 0;

	WINPR_ASSERT(bio);
	WINPR_ASSERT(pBuffer);
	WINPR_ASSERT(encodingContext);

	const BYTE effectiveOpcode = ((encodingContext->opcode & 0xf) == WebsocketContinuationOpcode
	                                  ? encodingContext->fragmentOriginalOpcode & 0xf
	                                  : encodingContext->opcode & 0xf);

	switch (effectiveOpcode)
	{
		case WebsocketBinaryOpcode:
		{
			status = websocket_read_data(bio, pBuffer, size, encodingContext);
			if (status < 0)
				return status;

			return status;
		}
		case WebsocketPingOpcode:
		{
			status = websocket_read_wstream(bio, encodingContext);
			if (status < 0)
				return status;

			if (encodingContext->payloadLength == 0)
			{
				websocket_reply_pong(bio, encodingContext, encodingContext->responseStreamBuffer);
				Stream_SetPosition(encodingContext->responseStreamBuffer, 0);
			}
		}
		break;
		case WebsocketPongOpcode:
		{
			status = websocket_read_wstream(bio, encodingContext);
			if (status < 0)
				return status;
			/* We don´t care about pong response data, discard. */
			Stream_SetPosition(encodingContext->responseStreamBuffer, 0);
		}
		break;
		case WebsocketCloseOpcode:
		{
			status = websocket_read_wstream(bio, encodingContext);
			if (status < 0)
				return status;

			if (encodingContext->payloadLength == 0)
			{
				websocket_reply_close(bio, encodingContext, encodingContext->responseStreamBuffer);
				encodingContext->closeSent = TRUE;
				Stream_SetPosition(encodingContext->responseStreamBuffer, 0);
			}
		}
		break;
		default:
			WLog_WARN(TAG, "Unimplemented websocket opcode %" PRIx8 ". Dropping", effectiveOpcode);

			status = websocket_read_wstream(bio, encodingContext);
			if (status < 0)
				return status;
			Stream_SetPosition(encodingContext->responseStreamBuffer, 0);
			break;
	}
	/* return how many bytes have been written to pBuffer.
	 * Only WebsocketBinaryOpcode writes into it and it returns directly */
	return 0;
}

int websocket_context_read(websocket_context* encodingContext, BIO* bio, BYTE* pBuffer, size_t size)
{
	int status = 0;
	size_t effectiveDataLen = 0;

	WINPR_ASSERT(bio);
	WINPR_ASSERT(pBuffer);
	WINPR_ASSERT(encodingContext);

	while (TRUE)
	{
		switch (encodingContext->state)
		{
			case WebsocketStateOpcodeAndFin:
			{
				BYTE buffer[1] = { 0 };

				ERR_clear_error();
				status = BIO_read(bio, (char*)buffer, sizeof(buffer));
				if (status <= 0)
					return (effectiveDataLen > 0 ? WINPR_ASSERTING_INT_CAST(int, effectiveDataLen)
					                             : status);

				encodingContext->opcode = buffer[0];
				if (((encodingContext->opcode & 0xf) != WebsocketContinuationOpcode) &&
				    (encodingContext->opcode & 0xf) < 0x08)
					encodingContext->fragmentOriginalOpcode = encodingContext->opcode;
				encodingContext->state = WebsocketStateLengthAndMasking;
			}
			break;
			case WebsocketStateLengthAndMasking:
			{
				BYTE buffer[1] = { 0 };

				ERR_clear_error();
				status = BIO_read(bio, (char*)buffer, sizeof(buffer));
				if (status <= 0)
					return (effectiveDataLen > 0 ? WINPR_ASSERTING_INT_CAST(int, effectiveDataLen)
					                             : status);

				encodingContext->masking = ((buffer[0] & WEBSOCKET_MASK_BIT) == WEBSOCKET_MASK_BIT);
				encodingContext->lengthAndMaskPosition = 0;
				encodingContext->payloadLength = 0;
				const BYTE len = buffer[0] & 0x7f;
				if (len < 126)
				{
					encodingContext->payloadLength = len;
					encodingContext->state = (encodingContext->masking ? WebSocketStateMaskingKey
					                                                   : WebSocketStatePayload);
				}
				else if (len == 126)
					encodingContext->state = WebsocketStateShortLength;
				else
					encodingContext->state = WebsocketStateLongLength;
			}
			break;
			case WebsocketStateShortLength:
			case WebsocketStateLongLength:
			{
				BYTE buffer[1] = { 0 };
				const BYTE lenLength =
				    (encodingContext->state == WebsocketStateShortLength ? 2 : 8);
				while (encodingContext->lengthAndMaskPosition < lenLength)
				{
					ERR_clear_error();
					status = BIO_read(bio, (char*)buffer, sizeof(buffer));
					if (status <= 0)
						return (effectiveDataLen > 0
						            ? WINPR_ASSERTING_INT_CAST(int, effectiveDataLen)
						            : status);
					if (status > UINT8_MAX)
						return -1;
					encodingContext->payloadLength =
					    (encodingContext->payloadLength) << 8 | buffer[0];
					encodingContext->lengthAndMaskPosition +=
					    WINPR_ASSERTING_INT_CAST(BYTE, status);
				}
				encodingContext->state =
				    (encodingContext->masking ? WebSocketStateMaskingKey : WebSocketStatePayload);
			}
			break;
			case WebSocketStateMaskingKey:
			{
				WLog_WARN(
				    TAG, "Websocket Server sends data with masking key. This is against RFC 6455.");
				return -1;
			}
			case WebSocketStatePayload:
			{
				status = websocket_handle_payload(bio, pBuffer, size, encodingContext);
				if (status < 0)
					return (effectiveDataLen > 0 ? WINPR_ASSERTING_INT_CAST(int, effectiveDataLen)
					                             : status);

				effectiveDataLen += WINPR_ASSERTING_INT_CAST(size_t, status);

				if (WINPR_ASSERTING_INT_CAST(size_t, status) >= size)
					return WINPR_ASSERTING_INT_CAST(int, effectiveDataLen);
				pBuffer += status;
				size -= WINPR_ASSERTING_INT_CAST(size_t, status);
			}
			break;
			default:
				break;
		}
	}
	/* should be unreachable */
}

websocket_context* websocket_context_new(void)
{
	websocket_context* context = calloc(1, sizeof(websocket_context));
	if (!context)
		goto fail;

	context->responseStreamBuffer = Stream_New(NULL, 1024);
	if (!context->responseStreamBuffer)
		goto fail;

	if (!websocket_context_reset(context))
		goto fail;

	return context;
fail:
	websocket_context_free(context);
	return NULL;
}

void websocket_context_free(websocket_context* context)
{
	if (!context)
		return;

	Stream_Free(context->responseStreamBuffer, TRUE);
	free(context);
}

BOOL websocket_context_reset(websocket_context* context)
{
	WINPR_ASSERT(context);

	context->state = WebsocketStateOpcodeAndFin;
	return Stream_SetPosition(context->responseStreamBuffer, 0);
}

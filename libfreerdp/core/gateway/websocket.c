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

BOOL websocket_write_wstream(BIO* bio, wStream* sPacket, WEBSOCKET_OPCODE opcode)
{
	size_t fullLen = 0;
	int status = 0;
	wStream* sWS = NULL;

	uint32_t maskingKey = 0;

	size_t streamPos = 0;

	WINPR_ASSERT(bio);
	WINPR_ASSERT(sPacket);

	const size_t len = Stream_Length(sPacket);
	Stream_SetPosition(sPacket, 0);

	if (len > INT_MAX)
		return FALSE;

	if (len < 126)
		fullLen = len + 6; /* 2 byte "mini header" + 4 byte masking key */
	else if (len < 0x10000)
		fullLen = len + 8; /* 2 byte "mini header" + 2 byte length + 4 byte masking key */
	else
		fullLen = len + 14; /* 2 byte "mini header" + 8 byte length + 4 byte masking key */

	sWS = Stream_New(NULL, fullLen);
	if (!sWS)
		return FALSE;

	winpr_RAND(&maskingKey, sizeof(maskingKey));

	Stream_Write_UINT8(sWS, WEBSOCKET_FIN_BIT | opcode);
	if (len < 126)
		Stream_Write_UINT8(sWS, len | WEBSOCKET_MASK_BIT);
	else if (len < 0x10000)
	{
		Stream_Write_UINT8(sWS, 126 | WEBSOCKET_MASK_BIT);
		Stream_Write_UINT16_BE(sWS, len);
	}
	else
	{
		Stream_Write_UINT8(sWS, 127 | WEBSOCKET_MASK_BIT);
		Stream_Write_UINT32_BE(sWS, 0); /* payload is limited to INT_MAX */
		Stream_Write_UINT32_BE(sWS, len);
	}
	Stream_Write_UINT32(sWS, maskingKey);

	/* mask as much as possible with 32bit access */
	for (streamPos = 0; streamPos + 4 <= len; streamPos += 4)
	{
		uint32_t data = 0;
		Stream_Read_UINT32(sPacket, data);
		Stream_Write_UINT32(sWS, data ^ maskingKey);
	}

	/* mask the rest byte by byte */
	for (; streamPos < len; streamPos++)
	{
		BYTE data = 0;
		BYTE* partialMask = ((BYTE*)&maskingKey) + (streamPos % 4);
		Stream_Read_UINT8(sPacket, data);
		Stream_Write_UINT8(sWS, data ^ *partialMask);
	}

	Stream_SealLength(sWS);

	ERR_clear_error();
	const size_t size = Stream_Length(sWS);
	if (size <= INT32_MAX)
		status = BIO_write(bio, Stream_Buffer(sWS), (int)size);
	Stream_Free(sWS, TRUE);

	if (status != (SSIZE_T)fullLen)
		return FALSE;

	return TRUE;
}

static int websocket_write_all(BIO* bio, const BYTE* data, size_t length)
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
				status = BIO_wait_write(bio, 100);
			else if (BIO_read_blocked(bio))
				return -2; /* Abort write, there is data that must be read */
			else
				USleep(100);

			if (status < 0)
				return -1;
		}
	}

	return (int)length;
}

int websocket_write(BIO* bio, const BYTE* buf, int isize, WEBSOCKET_OPCODE opcode)
{
	size_t payloadSize = 0;
	size_t fullLen = 0;
	int status = 0;
	wStream* sWS = NULL;

	uint32_t maskingKey = 0;

	int streamPos = 0;

	WINPR_ASSERT(bio);
	WINPR_ASSERT(buf);

	winpr_RAND(&maskingKey, sizeof(maskingKey));

	payloadSize = isize;
	if ((isize < 0) || (isize > UINT32_MAX))
		return -1;

	if (payloadSize < 126)
		fullLen = payloadSize + 6; /* 2 byte "mini header" + 4 byte masking key */
	else if (payloadSize < 0x10000)
		fullLen = payloadSize + 8; /* 2 byte "mini header" + 2 byte length + 4 byte masking key */
	else
		fullLen = payloadSize + 14; /* 2 byte "mini header" + 8 byte length + 4 byte masking key */

	sWS = Stream_New(NULL, fullLen);
	if (!sWS)
		return FALSE;

	Stream_Write_UINT8(sWS, WEBSOCKET_FIN_BIT | opcode);
	if (payloadSize < 126)
		Stream_Write_UINT8(sWS, payloadSize | WEBSOCKET_MASK_BIT);
	else if (payloadSize < 0x10000)
	{
		Stream_Write_UINT8(sWS, 126 | WEBSOCKET_MASK_BIT);
		Stream_Write_UINT16_BE(sWS, payloadSize);
	}
	else
	{
		Stream_Write_UINT8(sWS, 127 | WEBSOCKET_MASK_BIT);
		/* biggest packet possible is 0xffff + 0xa, so 32bit is always enough */
		Stream_Write_UINT32_BE(sWS, 0);
		Stream_Write_UINT32_BE(sWS, payloadSize);
	}
	Stream_Write_UINT32(sWS, maskingKey);

	/* mask as much as possible with 32bit access */
	for (streamPos = 0; streamPos + 4 <= isize; streamPos += 4)
	{
		uint32_t masked = *((const uint32_t*)(buf + streamPos)) ^ maskingKey;
		Stream_Write_UINT32(sWS, masked);
	}

	/* mask the rest byte by byte */
	for (; streamPos < isize; streamPos++)
	{
		BYTE* partialMask = (BYTE*)(&maskingKey) + streamPos % 4;
		BYTE masked = *((buf + streamPos)) ^ *partialMask;
		Stream_Write_UINT8(sWS, masked);
	}

	Stream_SealLength(sWS);

	status = websocket_write_all(bio, Stream_Buffer(sWS), Stream_Length(sWS));
	Stream_Free(sWS, TRUE);

	if (status < 0)
		return status;

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
	if (status <= 0)
		return status;

	encodingContext->payloadLength -= status;

	if (encodingContext->payloadLength == 0)
		encodingContext->state = WebsocketStateOpcodeAndFin;

	return status;
}

static int websocket_read_discard(BIO* bio, websocket_context* encodingContext)
{
	char _dummy[256] = { 0 };
	int status = 0;

	WINPR_ASSERT(bio);
	WINPR_ASSERT(encodingContext);

	if (encodingContext->payloadLength == 0)
	{
		encodingContext->state = WebsocketStateOpcodeAndFin;
		return 0;
	}

	ERR_clear_error();
	status = BIO_read(bio, _dummy, sizeof(_dummy));
	if (status <= 0)
		return status;

	encodingContext->payloadLength -= status;

	if (encodingContext->payloadLength == 0)
		encodingContext->state = WebsocketStateOpcodeAndFin;

	return status;
}

static int websocket_read_wstream(BIO* bio, wStream* s, websocket_context* encodingContext)
{
	int status = 0;

	WINPR_ASSERT(bio);
	WINPR_ASSERT(s);
	WINPR_ASSERT(encodingContext);

	if (encodingContext->payloadLength == 0)
	{
		encodingContext->state = WebsocketStateOpcodeAndFin;
		return 0;
	}
	if (Stream_GetRemainingCapacity(s) != encodingContext->payloadLength)
	{
		WLog_WARN(TAG,
		          "wStream::capacity [%" PRIuz "] != encodingContext::paylaodLangth [%" PRIuz "]",
		          Stream_GetRemainingCapacity(s), encodingContext->payloadLength);
		return -1;
	}

	const size_t rlen = encodingContext->payloadLength;
	if (rlen > INT32_MAX)
		return -1;

	ERR_clear_error();
	status = BIO_read(bio, Stream_Pointer(s), (int)rlen);
	if (status <= 0)
		return status;

	Stream_Seek(s, status);

	encodingContext->payloadLength -= status;

	if (encodingContext->payloadLength == 0)
	{
		encodingContext->state = WebsocketStateOpcodeAndFin;
		Stream_SealLength(s);
		Stream_SetPosition(s, 0);
	}

	return status;
}

static BOOL websocket_reply_close(BIO* bio, wStream* s)
{
	/* write back close */
	wStream* closeFrame = NULL;
	uint16_t maskingKey1 = 0;
	uint16_t maskingKey2 = 0;
	size_t closeDataLen = 0;

	WINPR_ASSERT(bio);

	closeDataLen = 0;
	if (s != NULL && Stream_Length(s) >= 2)
		closeDataLen = 2;

	closeFrame = Stream_New(NULL, 6 + closeDataLen);
	if (!closeFrame)
		return FALSE;

	Stream_Write_UINT8(closeFrame, WEBSOCKET_FIN_BIT | WebsocketCloseOpcode);
	Stream_Write_UINT8(closeFrame, closeDataLen | WEBSOCKET_MASK_BIT); /* no payload */
	winpr_RAND(&maskingKey1, sizeof(maskingKey1));
	winpr_RAND(&maskingKey2, sizeof(maskingKey2));
	Stream_Write_UINT16(closeFrame, maskingKey1);
	Stream_Write_UINT16(closeFrame, maskingKey2); /* unused half, max 2 bytes of data */

	if (closeDataLen == 2)
	{
		uint16_t data = 0;
		Stream_Read_UINT16(s, data);
		Stream_Write_UINT16(closeFrame, data ^ maskingKey1);
	}
	Stream_SealLength(closeFrame);

	const size_t rlen = Stream_Length(closeFrame);

	int status = -1;
	if (rlen <= INT32_MAX)
	{
		ERR_clear_error();
		status = BIO_write(bio, Stream_Buffer(closeFrame), (int)rlen);
	}
	Stream_Free(closeFrame, TRUE);

	/* server MUST close socket now. The server is not allowed anymore to
	 * send frames but if he does, nothing bad would happen */
	if (status < 0)
		return FALSE;
	return TRUE;
}

static BOOL websocket_reply_pong(BIO* bio, wStream* s)
{
	wStream* closeFrame = NULL;
	uint32_t maskingKey = 0;

	WINPR_ASSERT(bio);

	if (s != NULL)
		return websocket_write_wstream(bio, s, WebsocketPongOpcode);

	closeFrame = Stream_New(NULL, 6);
	if (!closeFrame)
		return FALSE;

	Stream_Write_UINT8(closeFrame, WEBSOCKET_FIN_BIT | WebsocketPongOpcode);
	Stream_Write_UINT8(closeFrame, 0 | WEBSOCKET_MASK_BIT); /* no payload */
	winpr_RAND(&maskingKey, sizeof(maskingKey));
	Stream_Write_UINT32(closeFrame, maskingKey); /* dummy masking key. */
	Stream_SealLength(closeFrame);

	const size_t rlen = Stream_Length(closeFrame);
	int status = -1;
	if (rlen <= INT32_MAX)
	{
		ERR_clear_error();
		status = BIO_write(bio, Stream_Buffer(closeFrame), (int)rlen);
	}
	Stream_Free(closeFrame, TRUE);

	if (status < 0)
		return FALSE;
	return TRUE;
}

static int websocket_handle_payload(BIO* bio, BYTE* pBuffer, size_t size,
                                    websocket_context* encodingContext)
{
	int status = 0;

	WINPR_ASSERT(bio);
	WINPR_ASSERT(pBuffer);
	WINPR_ASSERT(encodingContext);

	BYTE effectiveOpcode = ((encodingContext->opcode & 0xf) == WebsocketContinuationOpcode
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
			if (encodingContext->responseStreamBuffer == NULL)
				encodingContext->responseStreamBuffer =
				    Stream_New(NULL, encodingContext->payloadLength);

			status =
			    websocket_read_wstream(bio, encodingContext->responseStreamBuffer, encodingContext);
			if (status < 0)
				return status;

			if (encodingContext->payloadLength == 0)
			{
				if (!encodingContext->closeSent)
					websocket_reply_pong(bio, encodingContext->responseStreamBuffer);

				Stream_Free(encodingContext->responseStreamBuffer, TRUE);
				encodingContext->responseStreamBuffer = NULL;
			}
		}
		break;
		case WebsocketCloseOpcode:
		{
			if (encodingContext->responseStreamBuffer == NULL)
				encodingContext->responseStreamBuffer =
				    Stream_New(NULL, encodingContext->payloadLength);

			status =
			    websocket_read_wstream(bio, encodingContext->responseStreamBuffer, encodingContext);
			if (status < 0)
				return status;

			if (encodingContext->payloadLength == 0)
			{
				websocket_reply_close(bio, encodingContext->responseStreamBuffer);
				encodingContext->closeSent = TRUE;

				if (encodingContext->responseStreamBuffer)
					Stream_Free(encodingContext->responseStreamBuffer, TRUE);
				encodingContext->responseStreamBuffer = NULL;
			}
		}
		break;
		default:
			WLog_WARN(TAG, "Unimplemented websocket opcode %x. Dropping", effectiveOpcode & 0xf);

			status = websocket_read_discard(bio, encodingContext);
			if (status < 0)
				return status;
	}
	/* return how many bytes have been written to pBuffer.
	 * Only WebsocketBinaryOpcode writes into it and it returns directly */
	return 0;
}

int websocket_read(BIO* bio, BYTE* pBuffer, size_t size, websocket_context* encodingContext)
{
	int status = 0;
	int effectiveDataLen = 0;

	WINPR_ASSERT(bio);
	WINPR_ASSERT(pBuffer);
	WINPR_ASSERT(encodingContext);

	while (TRUE)
	{
		switch (encodingContext->state)
		{
			case WebsocketStateOpcodeAndFin:
			{
				BYTE buffer[1];
				ERR_clear_error();
				status = BIO_read(bio, (char*)buffer, sizeof(buffer));
				if (status <= 0)
					return (effectiveDataLen > 0 ? effectiveDataLen : status);

				encodingContext->opcode = buffer[0];
				if (((encodingContext->opcode & 0xf) != WebsocketContinuationOpcode) &&
				    (encodingContext->opcode & 0xf) < 0x08)
					encodingContext->fragmentOriginalOpcode = encodingContext->opcode;
				encodingContext->state = WebsocketStateLengthAndMasking;
			}
			break;
			case WebsocketStateLengthAndMasking:
			{
				BYTE buffer[1];
				BYTE len = 0;
				ERR_clear_error();
				status = BIO_read(bio, (char*)buffer, sizeof(buffer));
				if (status <= 0)
					return (effectiveDataLen > 0 ? effectiveDataLen : status);

				encodingContext->masking = ((buffer[0] & WEBSOCKET_MASK_BIT) == WEBSOCKET_MASK_BIT);
				encodingContext->lengthAndMaskPosition = 0;
				encodingContext->payloadLength = 0;
				len = buffer[0] & 0x7f;
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
				BYTE buffer[1];
				BYTE lenLength = (encodingContext->state == WebsocketStateShortLength ? 2 : 8);
				while (encodingContext->lengthAndMaskPosition < lenLength)
				{
					ERR_clear_error();
					status = BIO_read(bio, (char*)buffer, sizeof(buffer));
					if (status <= 0)
						return (effectiveDataLen > 0 ? effectiveDataLen : status);

					encodingContext->payloadLength =
					    (encodingContext->payloadLength) << 8 | buffer[0];
					encodingContext->lengthAndMaskPosition += status;
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
					return (effectiveDataLen > 0 ? effectiveDataLen : status);

				effectiveDataLen += status;

				if ((size_t)status == size)
					return effectiveDataLen;
				pBuffer += status;
				size -= status;
			}
		}
	}
	/* should be unreachable */
}

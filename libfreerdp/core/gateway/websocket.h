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

#ifndef FREERDP_LIB_CORE_GATEWAY_WEBSOCKET_H
#define FREERDP_LIB_CORE_GATEWAY_WEBSOCKET_H

#include <winpr/wtypes.h>
#include <winpr/stream.h>

#include <freerdp/api.h>

#include "../../crypto/tls.h"

#define WEBSOCKET_MASK_BIT 0x80
#define WEBSOCKET_FIN_BIT 0x80

typedef enum
{
	WebsocketContinuationOpcode = 0x0,
	WebsocketTextOpcode = 0x1,
	WebsocketBinaryOpcode = 0x2,
	WebsocketCloseOpcode = 0x8,
	WebsocketPingOpcode = 0x9,
	WebsocketPongOpcode = 0xa,
} WEBSOCKET_OPCODE;

typedef enum
{
	WebsocketStateOpcodeAndFin,
	WebsocketStateLengthAndMasking,
	WebsocketStateShortLength,
	WebsocketStateLongLength,
	WebSocketStateMaskingKey,
	WebSocketStatePayload,
} WEBSOCKET_STATE;

typedef struct s_websocket_context websocket_context;

FREERDP_LOCAL void websocket_context_free(websocket_context* context);

WINPR_ATTR_MALLOC(websocket_context_free, 1)
FREERDP_LOCAL websocket_context* websocket_context_new(void);

FREERDP_LOCAL BOOL websocket_context_reset(websocket_context* context);

FREERDP_LOCAL BOOL websocket_context_write_wstream(websocket_context* context, BIO* bio,
                                                   wStream* sPacket, WEBSOCKET_OPCODE opcode);
FREERDP_LOCAL int websocket_context_write(websocket_context* context, BIO* bio, const BYTE* buf,
                                          int isize, WEBSOCKET_OPCODE opcode);
FREERDP_LOCAL int websocket_context_read(websocket_context* encodingContext, BIO* bio,
                                         BYTE* pBuffer, size_t size);

WINPR_ATTR_MALLOC(Stream_Free, 1)
FREERDP_LOCAL wStream* websocket_context_packet_new(size_t len, WEBSOCKET_OPCODE opcode,
                                                    UINT32* pMaskingKey);

FREERDP_LOCAL BOOL websocket_context_mask_and_send(BIO* bio, wStream* sPacket, wStream* sDataPacket,
                                                   UINT32 maskingKey);

#endif /* FREERDP_LIB_CORE_GATEWAY_WEBSOCKET_H */

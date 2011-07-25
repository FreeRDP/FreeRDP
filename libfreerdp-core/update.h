/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Update Data PDUs
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __UPDATE_H
#define __UPDATE_H

#include "rdp.h"
#include "orders.h"
#include <freerdp/types.h>
#include <freerdp/utils/stream.h>

#define UPDATE_TYPE_ORDERS		0x0000
#define UPDATE_TYPE_BITMAP		0x0001
#define UPDATE_TYPE_PALETTE		0x0002
#define UPDATE_TYPE_SYNCHRONIZE		0x0003

typedef struct
{
	uint16 left;
	uint16 top;
	uint16 right;
	uint16 bottom;
	uint16 width;
	uint16 height;
	uint16 bpp;
	uint16 flags;
	uint16 length;
	uint8* data;
} BITMAP_DATA;

#define BITMAP_COMPRESSION		0x0001
#define NO_BITMAP_COMPRESSION_HDR	0x0400

void rdp_read_bitmap_data(STREAM* s, BITMAP_DATA* bitmap_data);
void rdp_recv_bitmap_update(rdpRdp* rdp, STREAM* s);
void rdp_recv_update_data_pdu(rdpRdp* rdp, STREAM* s);

#endif /* __UPDATE_H */

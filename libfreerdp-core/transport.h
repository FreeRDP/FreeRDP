/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Network Transport Layer
 *
 * Copyright 2011 Vic Lee
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

#ifndef __TRANSPORT_H
#define __TRANSPORT_H

typedef enum
{
	TRANSPORT_LAYER_TCP,
	TRANSPORT_LAYER_TLS
} TRANSPORT_LAYER;

typedef enum
{
	TRANSPORT_STATE_INITIAL,
	TRANSPORT_STATE_NEGO,
	TRANSPORT_STATE_RDP,
	TRANSPORT_STATE_TLS,
	TRANSPORT_STATE_NLA,
	TRANSPORT_STATE_FINAL
} TRANSPORT_STATE;

typedef struct rdp_transport rdpTransport;

#include "tcp.h"
#include "tls.h"
#include "credssp.h"

#include <time.h>
#include <freerdp/types.h>
#include <freerdp/settings.h>
#include <freerdp/utils/stream.h>

typedef int (*TransportRecv) (rdpTransport* transport, STREAM* stream, void* extra);

struct rdp_transport
{
	STREAM* recv_stream;
	STREAM* send_stream;
	TRANSPORT_LAYER layer;
	TRANSPORT_STATE state;
	struct rdp_tcp* tcp;
	struct rdp_tls* tls;
	struct rdp_settings* settings;
	struct rdp_credssp* credssp;
	struct timespec ts;
	void* recv_extra;
	STREAM* recv_buffer;
	TransportRecv recv_callback;
};

STREAM* transport_recv_stream_init(rdpTransport* transport, int size);
STREAM* transport_send_stream_init(rdpTransport* transport, int size);
boolean transport_connect(rdpTransport* transport, const uint8* hostname, uint16 port);
boolean transport_disconnect(rdpTransport* transport);
boolean transport_connect_rdp(rdpTransport* transport);
boolean transport_connect_tls(rdpTransport* transport);
boolean transport_connect_nla(rdpTransport* transport);
int transport_read(rdpTransport* transport, STREAM* s);
int transport_write(rdpTransport* transport, STREAM* s);
int transport_check_fds(rdpTransport* transport);
rdpTransport* transport_new(rdpSettings* settings);
void transport_free(rdpTransport* transport);

#endif

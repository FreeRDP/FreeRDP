/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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
	TRANSPORT_LAYER_TLS,
	TRANSPORT_LAYER_TSG,
	TRANSPORT_LAYER_CLOSED
} TRANSPORT_LAYER;

typedef struct rdp_transport rdpTransport;

#include "tcp.h"
#include "tsg.h"
#include "nla.h"

#include <winpr/sspi.h>
#include <freerdp/crypto/tls.h>

#include <time.h>
#include <freerdp/types.h>
#include <freerdp/settings.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/wait_obj.h>

typedef BOOL (*TransportRecv) (rdpTransport* transport, STREAM* stream, void* extra);

struct rdp_transport
{
	STREAM* recv_stream;
	STREAM* send_stream;
	TRANSPORT_LAYER layer;
	rdpTsg* tsg;
	rdpTcp* TcpIn;
	rdpTcp* TcpOut;
	rdpTls* TlsIn;
	rdpTls* TlsOut;
	rdpCredssp* credssp;
	rdpSettings* settings;
	UINT32 usleep_interval;
	void* recv_extra;
	STREAM* recv_buffer;
	TransportRecv recv_callback;
	struct wait_obj* recv_event;
	BOOL blocking;
	BOOL ProcessSinglePdu;
	BOOL SplitInputOutput;
};

STREAM* transport_recv_stream_init(rdpTransport* transport, int size);
STREAM* transport_send_stream_init(rdpTransport* transport, int size);
BOOL transport_connect(rdpTransport* transport, const char* hostname, UINT16 port);
void transport_attach(rdpTransport* transport, int sockfd);
BOOL transport_disconnect(rdpTransport* transport);
BOOL transport_connect_rdp(rdpTransport* transport);
BOOL transport_connect_tls(rdpTransport* transport);
BOOL transport_connect_nla(rdpTransport* transport);
BOOL transport_connect_tsg(rdpTransport* transport);
BOOL transport_accept_rdp(rdpTransport* transport);
BOOL transport_accept_tls(rdpTransport* transport);
BOOL transport_accept_nla(rdpTransport* transport);
int transport_read(rdpTransport* transport, STREAM* s);
int transport_write(rdpTransport* transport, STREAM* s);
void transport_get_fds(rdpTransport* transport, void** rfds, int* rcount);
int transport_check_fds(rdpTransport** ptransport);
BOOL transport_set_blocking_mode(rdpTransport* transport, BOOL blocking);
rdpTransport* transport_new(rdpSettings* settings);
void transport_free(rdpTransport* transport);

#endif

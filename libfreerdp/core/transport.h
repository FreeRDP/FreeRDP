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
#include "nla.h"

#include "gateway/tsg.h"

#include <winpr/sspi.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

#include <freerdp/crypto/tls.h>

#include <time.h>
#include <freerdp/types.h>
#include <freerdp/settings.h>

typedef int (*TransportRecv) (rdpTransport* transport, wStream* stream, void* extra);

struct rdp_transport
{
	TRANSPORT_LAYER layer;
	rdpTsg* tsg;
	rdpTcp* TcpIn;
	rdpTcp* TcpOut;
	rdpTls* TlsIn;
	rdpTls* TlsOut;
	rdpCredssp* credssp;
	rdpSettings* settings;
	UINT32 SleepInterval;
	void* ReceiveExtra;
	wStream* ReceiveBuffer;
	TransportRecv ReceiveCallback;
	HANDLE ReceiveEvent;
	HANDLE GatewayEvent;
	BOOL blocking;
	BOOL SplitInputOutput;
	wStreamPool* ReceivePool;
	HANDLE connectedEvent;
	HANDLE stopEvent;
	HANDLE thread;
	BOOL async;
	HANDLE ReadMutex;
	HANDLE WriteMutex;
};

wStream* transport_send_stream_init(rdpTransport* transport, int size);
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
int transport_read(rdpTransport* transport, wStream* s);
int transport_write(rdpTransport* transport, wStream* s);
void transport_get_fds(rdpTransport* transport, void** rfds, int* rcount);
int transport_check_fds(rdpTransport** ptransport);
BOOL transport_set_blocking_mode(rdpTransport* transport, BOOL blocking);
void transport_get_read_handles(rdpTransport* transport, HANDLE* events, DWORD* count);

wStream* transport_receive_pool_take(rdpTransport* transport);
int transport_receive_pool_return(rdpTransport* transport, wStream* pdu);

rdpTransport* transport_new(rdpSettings* settings);
void transport_free(rdpTransport* transport);

#endif

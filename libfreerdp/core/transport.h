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
	TRANSPORT_LAYER_TSG_TLS,
	TRANSPORT_LAYER_CLOSED
} TRANSPORT_LAYER;

typedef struct rdp_transport rdpTransport;

#include "tcp.h"
#include "nla.h"

#include "gateway/tsg.h"

#include <winpr/sspi.h>
#include <winpr/wlog.h>
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
	BIO* frontBio;
	rdpTsg* tsg;
	rdpTls* tls;
	rdpTcp* TcpIn;
	rdpTcp* TcpOut;
	rdpTls* TlsIn;
	rdpTls* TlsOut;
	rdpTls* TsgTls;
	rdpContext* context;
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
	BOOL NlaMode;
	BOOL GatewayEnabled;
	CRITICAL_SECTION ReadLock;
	CRITICAL_SECTION WriteLock;
};

wStream* transport_send_stream_init(rdpTransport* transport, int size);
BOOL transport_connect(rdpTransport* transport, const char* hostname, UINT16 port, int timeout);
void transport_attach(rdpTransport* transport, int sockfd);
BOOL transport_disconnect(rdpTransport* transport);
BOOL transport_connect_rdp(rdpTransport* transport);
BOOL transport_connect_tls(rdpTransport* transport);
BOOL transport_connect_nla(rdpTransport* transport);
BOOL transport_accept_rdp(rdpTransport* transport);
BOOL transport_accept_tls(rdpTransport* transport);
BOOL transport_accept_nla(rdpTransport* transport);
void transport_stop(rdpTransport* transport);
int transport_read_pdu(rdpTransport* transport, wStream* s);
int transport_write(rdpTransport* transport, wStream* s);

void transport_get_fds(rdpTransport* transport, void** rfds, int* rcount);
int transport_check_fds(rdpTransport* transport);

DWORD transport_get_event_handles(rdpTransport* transport, HANDLE* events);

BOOL transport_set_blocking_mode(rdpTransport* transport, BOOL blocking);
void transport_set_gateway_enabled(rdpTransport* transport, BOOL GatewayEnabled);
void transport_set_nla_mode(rdpTransport* transport, BOOL NlaMode);
BOOL tranport_is_write_blocked(rdpTransport* transport);
int tranport_drain_output_buffer(rdpTransport* transport);

wStream* transport_receive_pool_take(rdpTransport* transport);
int transport_receive_pool_return(rdpTransport* transport, wStream* pdu);

rdpTransport* transport_new(rdpContext* context);
void transport_free(rdpTransport* transport);

#endif

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

#ifndef FREERDP_LIB_CORE_TRANSPORT_H
#define FREERDP_LIB_CORE_TRANSPORT_H

typedef enum
{
	TRANSPORT_LAYER_TCP,
	TRANSPORT_LAYER_TLS,
	TRANSPORT_LAYER_TSG,
	TRANSPORT_LAYER_TSG_TLS,
	TRANSPORT_LAYER_CLOSED
} TRANSPORT_LAYER;

#include "tcp.h"
#include "nla.h"

#include "gateway/tsg.h"
#include "gateway/rdg.h"

#include <winpr/sspi.h>
#include <winpr/wlog.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

#include <freerdp/api.h>
#include <freerdp/crypto/tls.h>

#include <time.h>
#include <freerdp/types.h>
#include <freerdp/settings.h>
#include <freerdp/transport_io.h>

typedef int (*TransportRecv)(rdpTransport* transport, wStream* stream, void* extra);

FREERDP_LOCAL wStream* transport_send_stream_init(rdpTransport* transport, size_t size);
FREERDP_LOCAL BOOL transport_connect(rdpTransport* transport, const char* hostname, UINT16 port,
                                     DWORD timeout);
FREERDP_LOCAL BOOL transport_attach(rdpTransport* transport, int sockfd);
FREERDP_LOCAL BOOL transport_disconnect(rdpTransport* transport);
FREERDP_LOCAL BOOL transport_connect_rdp(rdpTransport* transport);
FREERDP_LOCAL BOOL transport_connect_tls(rdpTransport* transport);
FREERDP_LOCAL BOOL transport_connect_nla(rdpTransport* transport);
FREERDP_LOCAL BOOL transport_accept_rdp(rdpTransport* transport);
FREERDP_LOCAL BOOL transport_accept_tls(rdpTransport* transport);
FREERDP_LOCAL BOOL transport_accept_nla(rdpTransport* transport);

FREERDP_LOCAL int transport_read_pdu(rdpTransport* transport, wStream* s);
FREERDP_LOCAL int transport_write(rdpTransport* transport, wStream* s);

#if defined(WITH_FREERDP_DEPRECATED)
FREERDP_LOCAL void transport_get_fds(rdpTransport* transport, void** rfds, int* rcount);
#endif
FREERDP_LOCAL int transport_check_fds(rdpTransport* transport);

FREERDP_LOCAL DWORD transport_get_event_handles(rdpTransport* transport, HANDLE* events,
                                                DWORD nCount);
FREERDP_LOCAL HANDLE transport_get_front_bio(rdpTransport* transport);

FREERDP_LOCAL BOOL transport_set_blocking_mode(rdpTransport* transport, BOOL blocking);
FREERDP_LOCAL void transport_set_gateway_enabled(rdpTransport* transport, BOOL GatewayEnabled);
FREERDP_LOCAL void transport_set_nla_mode(rdpTransport* transport, BOOL NlaMode);
FREERDP_LOCAL BOOL transport_is_write_blocked(rdpTransport* transport);
FREERDP_LOCAL int transport_drain_output_buffer(rdpTransport* transport);

FREERDP_LOCAL wStream* transport_receive_pool_take(rdpTransport* transport);
FREERDP_LOCAL int transport_receive_pool_return(rdpTransport* transport, wStream* pdu);

FREERDP_LOCAL const rdpTransportIo* transport_get_io_callbacks(rdpTransport* transport);
FREERDP_LOCAL BOOL transport_set_io_callbacks(rdpTransport* transport,
                                              const rdpTransportIo* io_callbacks);

FREERDP_LOCAL BOOL transport_set_nla(rdpTransport* transport, rdpNla* nla);
FREERDP_LOCAL rdpNla* transport_get_nla(rdpTransport* transport);

FREERDP_LOCAL BOOL transport_set_tls(rdpTransport* transport, rdpTls* tls);
FREERDP_LOCAL rdpTls* transport_get_tls(rdpTransport* transport);

FREERDP_LOCAL BOOL transport_set_tsg(rdpTransport* transport, rdpTsg* tsg);
FREERDP_LOCAL rdpTsg* transport_get_tsg(rdpTransport* transport);

FREERDP_LOCAL wStream* transport_take_from_pool(rdpTransport* transport, size_t size);

FREERDP_LOCAL ULONG transport_get_bytes_sent(rdpTransport* transport, BOOL resetCount);

FREERDP_LOCAL BOOL transport_have_more_bytes_to_read(rdpTransport* transport);

FREERDP_LOCAL TRANSPORT_LAYER transport_get_layer(rdpTransport* transport);
FREERDP_LOCAL BOOL transport_set_layer(rdpTransport* transport, TRANSPORT_LAYER layer);

FREERDP_LOCAL BOOL transport_get_blocking(rdpTransport* transport);
FREERDP_LOCAL BOOL transport_set_blocking(rdpTransport* transport, BOOL blocking);

FREERDP_LOCAL BOOL transport_set_connected_event(rdpTransport* transport);

FREERDP_LOCAL BOOL transport_set_recv_callbacks(rdpTransport* transport, TransportRecv recv,
                                                void* extra);

FREERDP_LOCAL int transport_tcp_connect(rdpTransport* transport, const char* hostname, int port,
                                        DWORD timeout);

FREERDP_LOCAL rdpTransport* transport_new(rdpContext* context);
FREERDP_LOCAL void transport_free(rdpTransport* transport);

#endif /* FREERDP_LIB_CORE_TRANSPORT_H */

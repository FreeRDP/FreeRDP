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

#include <freerdp/utils/stream.h>

typedef int (* PacketReceivedCallback) (STREAM * stream, void * callback_data);

struct rdp_transport
{
	int sockfd;
	struct crypto_tls * tls;
	struct timespec ts;
	STREAM * recv_buffer;

	PacketReceivedCallback recv_callback;
	void * recv_callback_data;
};
typedef struct rdp_transport rdpTransport;

rdpTransport *
transport_new(void);
void
transport_free(rdpTransport * transport);
int
transport_connect(rdpTransport * transport, const char * server, int port);
int
transport_disconnect(rdpTransport * transport);
int
transport_start_tls(rdpTransport * transport);
int
transport_send(rdpTransport * transport, STREAM * stream);
int
transport_check_fds(rdpTransport * transport);

#endif

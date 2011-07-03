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

#ifndef __NETWORK_H
#define __NETWORK_H

#include <freerdp/utils/stream.h>

typedef int (* PacketReceivedCallback) (STREAM * stream);

struct rdp_network
{
	int sockfd;
	struct crypto_tls * tls;
	PacketReceivedCallback * recv_callback;
};
typedef struct rdp_network rdpNetwork;

rdpNetwork *
network_new(void);
void
network_free(rdpNetwork * network);
int
network_connect(rdpNetwork * network, const char * server, int port);
int
network_disconnect(rdpNetwork * network);
int
network_start_tls(rdpNetwork * network);
int
network_send(rdpNetwork * network, STREAM * stream);
int
network_check_fds(rdpNetwork * network);

#endif

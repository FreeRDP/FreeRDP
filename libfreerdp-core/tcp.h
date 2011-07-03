/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Transmission Control Protocol (TCP)
 *
 * Copyright 2011 Vic Lee
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

#ifndef __TCP_H
#define __TCP_H

#include <freerdp/types/base.h>
#include <freerdp/utils/stream.h>

typedef struct rdp_tcp rdpTcp;
typedef FRDP_BOOL (*TcpConnect) (rdpTcp * tcp, const char * hostname, int port);
typedef FRDP_BOOL (*TcpDisconnect) (rdpTcp * tcp);
typedef FRDP_BOOL (*TcpSetBlockingMode) (rdpTcp * tcp, FRDP_BOOL blocking);

struct rdp_tcp
{
	int sockfd;
	TcpConnect connect;
	TcpDisconnect disconnect;
	TcpSetBlockingMode set_blocking_mode;
};

FRDP_BOOL
tcp_connect(rdpTcp * tcp, const char * hostname, int port);
FRDP_BOOL
tcp_disconnect(rdpTcp * tcp);
FRDP_BOOL
tcp_set_blocking_mode(rdpTcp * tcp, FRDP_BOOL blocking);

rdpTcp*
tcp_new();
void
tcp_free(rdpTcp* tcp);

#endif /* __TCP_H */

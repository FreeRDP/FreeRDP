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

#include <freerdp/types.h>
#include <freerdp/settings.h>
#include <freerdp/utils/stream.h>

typedef struct rdp_tcp rdpTcp;
typedef boolean (*TcpConnect) (rdpTcp* tcp, const char* hostname, int port);
typedef boolean (*TcpDisconnect) (rdpTcp* tcp);
typedef boolean (*TcpSetBlockingMode) (rdpTcp* tcp, boolean blocking);

struct rdp_tcp
{
	int sockfd;
	uint8 ip_address[32];
	struct rdp_settings* settings;
	TcpConnect connect;
	TcpDisconnect disconnect;
	TcpSetBlockingMode set_blocking_mode;
};

boolean tcp_connect(rdpTcp* tcp, const char* hostname, int port);
boolean tcp_disconnect(rdpTcp* tcp);
boolean tcp_set_blocking_mode(rdpTcp* tcp, boolean blocking);

rdpTcp* tcp_new(rdpSettings* settings);
void tcp_free(rdpTcp* tcp);

#endif /* __TCP_H */

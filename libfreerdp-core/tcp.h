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

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#endif

#include <freerdp/types.h>
#include <freerdp/settings.h>
#include <freerdp/utils/stream.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

typedef struct rdp_tcp rdpTcp;

struct rdp_tcp
{
	int sockfd;
	char ip_address[32];
	uint8 mac_address[6];
	struct rdp_settings* settings;
#ifdef _WIN32
	WSAEVENT wsa_event;
#endif
};

boolean tcp_connect(rdpTcp* tcp, const char* hostname, uint16 port);
boolean tcp_disconnect(rdpTcp* tcp);
int tcp_read(rdpTcp* tcp, uint8* data, int length);
int tcp_write(rdpTcp* tcp, uint8* data, int length);
boolean tcp_set_blocking_mode(rdpTcp* tcp, boolean blocking);
boolean tcp_set_keep_alive_mode(rdpTcp* tcp);

rdpTcp* tcp_new(rdpSettings* settings);
void tcp_free(rdpTcp* tcp);

#endif /* __TCP_H */

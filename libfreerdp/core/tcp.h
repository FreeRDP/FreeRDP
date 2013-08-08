/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#include <winpr/windows.h>

#include <freerdp/types.h>
#include <freerdp/settings.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/stream.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

typedef struct rdp_tcp rdpTcp;

struct rdp_tcp
{
	int sockfd;
	char ip_address[32];
	BYTE mac_address[6];
	struct rdp_settings* settings;
#ifdef _WIN32
	WSAEVENT wsa_event;
#endif
	HANDLE event;
};

BOOL tcp_connect(rdpTcp* tcp, const char* hostname, UINT16 port);
BOOL tcp_disconnect(rdpTcp* tcp);
int tcp_read(rdpTcp* tcp, BYTE* data, int length);
int tcp_write(rdpTcp* tcp, BYTE* data, int length);
int tcp_wait_read(rdpTcp* tcp);
int tcp_wait_write(rdpTcp* tcp);
BOOL tcp_set_blocking_mode(rdpTcp* tcp, BOOL blocking);
BOOL tcp_set_keep_alive_mode(rdpTcp* tcp);
int tcp_attach(rdpTcp* tcp, int sockfd);
HANDLE tcp_get_event_handle(rdpTcp* tcp);

rdpTcp* tcp_new(rdpSettings* settings);
void tcp_free(rdpTcp* tcp);

#endif /* __TCP_H */

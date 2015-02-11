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
#include <winpr/winsock.h>

#include <freerdp/utils/ringbuffer.h>
#include <openssl/bio.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define BIO_TYPE_TSG			65
#define BIO_TYPE_SIMPLE			66
#define BIO_TYPE_BUFFERED		67

#define BIO_C_SET_SOCKET		1101
#define BIO_C_GET_SOCKET		1102
#define BIO_C_GET_EVENT			1103
#define BIO_C_SET_NONBLOCK		1104

#define BIO_set_socket(b, s, c)		BIO_ctrl(b, BIO_C_SET_SOCKET, c, s);
#define BIO_get_socket(b, c)		BIO_ctrl(b, BIO_C_GET_SOCKET, 0, (char*) c)
#define BIO_get_event(b, c)		BIO_ctrl(b, BIO_C_GET_EVENT, 0, (char*) c)
#define BIO_set_nonblock(b, c)		BIO_ctrl(b, BIO_C_SET_NONBLOCK, c, NULL)

typedef struct rdp_tcp rdpTcp;

struct rdp_tcp
{
	int sockfd;
	BOOL ipcSocket;
	char ip_address[32];
	BYTE mac_address[6];
	rdpSettings* settings;
	BIO* socketBio;
	BIO* bufferedBio;
	RingBuffer xmitBuffer;
	BOOL writeBlocked;
	BOOL readBlocked;
	HANDLE event;
};

BOOL freerdp_tcp_connect(rdpTcp* tcp, const char* hostname, int port, int timeout);
int freerdp_tcp_read(rdpTcp* tcp, BYTE* data, int length);
int freerdp_tcp_write(rdpTcp* tcp, BYTE* data, int length);
int freerdp_tcp_wait_read(rdpTcp* tcp, DWORD dwMilliSeconds);
int freerdp_tcp_wait_write(rdpTcp* tcp, DWORD dwMilliSeconds);
BOOL freerdp_tcp_set_blocking_mode(rdpTcp* tcp, BOOL blocking);
BOOL freerdp_tcp_set_keep_alive_mode(rdpTcp* tcp);
int freerdp_tcp_attach(rdpTcp* tcp, int sockfd);
HANDLE freerdp_tcp_get_event_handle(rdpTcp* tcp);

rdpTcp* freerdp_tcp_new(rdpSettings* settings);
void freerdp_tcp_free(rdpTcp* tcp);

#endif /* __TCP_H */

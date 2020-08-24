/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * IO Update Interface API
 *
 * Copyright 2020 Gluzskiy Alexandr <sss at sss dot chaoslab dot ru>
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

#ifndef FREERDP_UPDATE_IO_H
#define FREERDP_UPDATE_IO_H
/* TODO:
 * RDG
 * RPC
 */

#include <freerdp/types.h>

typedef struct rdp_transport rdpTransport;
typedef struct bio_st BIO;

typedef int (*pTCPConnect)(rdpContext* context, rdpSettings* settings, const char* hostname,
                           int port, DWORD timeout);
typedef BOOL (*pTLSConnect)(rdpTransport* transport);
typedef BOOL (*pProxyConnect)(rdpSettings* settings, BIO* bufferedBio, const char* proxyUsername,
                              const char* proxyPassword, const char* hostname, UINT16 port);
typedef BOOL (*pTLSAccept)(rdpTransport* transport);
typedef BOOL (*pTransportAttach)(rdpTransport* transport, int sockfd);
typedef BOOL (*pTransportDisconnect)(rdpTransport* transport);

typedef int (*pRead)(rdpContext* context, const BYTE* buf, size_t buf_size);
typedef int (*pWrite)(rdpContext* context, const BYTE* buf, size_t buf_size);
typedef int (*pDataHandler)(rdpContext* context, const BYTE* buf, size_t buf_size);

struct rdp_io_update
{
	rdpContext* context;     /* 0 */
	UINT32 paddingA[16 - 1]; /* 1 */

	/* switchable connect
	 * used to create tcp connection */
	pTCPConnect TCPConnect; /* 17 */

	/* switchable TLSConnect
	 * used to setup tls on already established TCP connection */
	pTLSConnect TLSConnect;

	/* switchable proxy_connect
	 * used to initialize proxy connection,
	 * can be implemented inside TcpConnect and just return TRUE,
	 * used to maintain compatibility with old design. */
	pProxyConnect ProxyConnect; /* 18 */

	/* switchable server-side tls-accept
	 * used to setup tls on already established TCP connection */
	pTLSAccept TLSAccept;

	/* should noop and return TRUE, used to mimic
	 * current freerdp design */
	pTransportAttach TransportAttach; /* 19 */

	/* callback used to shutdown whole io operations */
	pTransportDisconnect TransportDisconnect; /* 20 */

	UINT32 paddingB[32 - 21]; /* 21 */

	/* switchable read
	 * used to read bytes from IO backend */
	pWrite Read; /* 33 */

	/* switchable write
	 * used to write bytes to IO backend */
	pWrite Write; /* 34 */

	/* switchable data handler
	 * used if IO backed doing internal polling and reading
	 * and just passing recieved data to freerdp */
	pDataHandler DataHandler; /* 35 */

	UINT32 paddingC[48 - 36]; /* 36 */
};
typedef struct rdp_io_update rdpIoUpdate;

#endif /* FREERDP_UPDATE_IO_H */

/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Interface
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#ifndef FREERDP_TRANSPORT_IO_H
#define FREERDP_TRANSPORT_IO_H

typedef struct rdp_transport_io rdpTransportIo;

#include <freerdp/api.h>
#include <freerdp/types.h>

#include <winpr/stream.h>
#include <freerdp/freerdp.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef int (*pTCPConnect)(rdpContext* context, rdpSettings* settings, const char* hostname,
	                           int port, DWORD timeout);
	typedef BOOL (*pTransportFkt)(rdpTransport* transport);
	typedef BOOL (*pTransportAttach)(rdpTransport* transport, int sockfd);
	typedef int (*pTransportRWFkt)(rdpTransport* transport, wStream* s);
	typedef SSIZE_T (*pTransportRead)(rdpTransport* transport, BYTE* data, size_t bytes);

	struct rdp_transport_io
	{
		pTCPConnect TCPConnect;
		pTransportFkt TLSConnect;
		pTransportFkt TLSAccept;
		pTransportAttach TransportAttach;
		pTransportFkt TransportDisconnect;
		pTransportRWFkt ReadPdu;  /* Reads a whole PDU from the transport */
		pTransportRWFkt WritePdu; /* Writes a whole PDU to the transport */
		pTransportRead ReadBytes; /* Reads up to a requested amount of bytes from the transport */
	};
	typedef struct rdp_transport_io rdpTransportIo;

	FREERDP_API const rdpTransportIo* freerdp_get_io_callbacks(rdpContext* context);
	FREERDP_API BOOL freerdp_set_io_callbacks(rdpContext* context,
	                                          const rdpTransportIo* io_callbacks);

	FREERDP_API BOOL freerdp_set_io_callback_context(rdpContext* context, void* usercontext);
	FREERDP_API void* freerdp_get_io_callback_context(rdpContext* context);

	/* PDU parser.
	 * incomplete: FALSE if the whole PDU is available, TRUE otherwise
	 * Return: 0  -> PDU header incomplete
	 *         >0 -> PDU header complete, length of PDU.
	 *         <0 -> Abort, an error occured
	 */
	FREERDP_API SSIZE_T transport_parse_pdu(rdpTransport* transport, wStream* s, BOOL* incomplete);
	FREERDP_API rdpContext* transport_get_context(rdpTransport* transport);
	FREERDP_API rdpTransport* freerdp_get_transport(rdpContext* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_TRANSPORT_IO_H */

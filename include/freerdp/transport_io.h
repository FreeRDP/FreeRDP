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

#include <winpr/stream.h>

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/settings.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * @brief Read data from a transport layer
	 * @param userContext user defined context passed by @ref freerdp_set_io_callback_context
	 * @param data a buffer to read to
	 * @param bytes the size of the buffer
	 * @return the number of bytes read or <0 for failures
	 * @since version 3.9.0
	 */
	typedef int (*pTransportLayerRead)(void* userContext, void* data, int bytes);

	/**
	 * @brief write data to a transport layer
	 * @param userContext user defined context passed by @ref freerdp_set_io_callback_context
	 * @param data a buffer to write
	 * @param bytes the size of the buffer
	 * @return the number of bytes written or <0 for failures
	 * @since version 3.9.0
	 */
	typedef int (*pTransportLayerWrite)(void* userContext, const void* data, int bytes);
	typedef BOOL (*pTransportLayerFkt)(void* userContext);
	typedef BOOL (*pTransportLayerWait)(void* userContext, BOOL waitWrite, DWORD timeout);
	typedef HANDLE (*pTransportLayerGetEvent)(void* userContext);

	/**
	 * @since version 3.9.0
	 */
	typedef struct
	{
		ALIGN64 void* userContext;
		ALIGN64 pTransportLayerRead Read;
		ALIGN64 pTransportLayerWrite Write;
		ALIGN64 pTransportLayerFkt Close;
		ALIGN64 pTransportLayerWait Wait;
		ALIGN64 pTransportLayerGetEvent GetEvent;
		UINT64 reserved[64 - 6]; /* Reserve some space for ABI compatibility */
	} rdpTransportLayer;

	typedef int (*pTCPConnect)(rdpContext* context, rdpSettings* settings, const char* hostname,
	                           int port, DWORD timeout);
	typedef BOOL (*pTransportFkt)(rdpTransport* transport);
	typedef BOOL (*pTransportAttach)(rdpTransport* transport, int sockfd);
	typedef int (*pTransportRWFkt)(rdpTransport* transport, wStream* s);
	typedef SSIZE_T (*pTransportRead)(rdpTransport* transport, BYTE* data, size_t bytes);
	typedef BOOL (*pTransportGetPublicKey)(rdpTransport* transport, const BYTE** data,
	                                       DWORD* length);
	/**
	 *  @brief Mofify transport behaviour between bocking and non blocking operation
	 *
	 *  @param transport The transport to manipulate
	 *  @param blocking Boolean to set the transport \b TRUE blocking and \b FALSE non-blocking
	 *  @return \b TRUE for success, \b FALSE for any error
	 *
	 * @since version 3.3.0
	 */
	typedef BOOL (*pTransportSetBlockingMode)(rdpTransport* transport, BOOL blocking);
	typedef rdpTransportLayer* (*pTransportConnectLayer)(rdpTransport* transport,
	                                                     const char* hostname, int port,
	                                                     DWORD timeout);

	/**
	 * @brief Return the public key as PEM from transport layer.
	 * @param transport the transport to query
	 * @param layer the transport layer to attach
	 *
	 * @return \b TRUE for success, \b FALSE for failure
	 * @since version 3.2.0
	 */
	typedef BOOL (*pTransportAttachLayer)(rdpTransport* transport, rdpTransportLayer* layer);

	struct rdp_transport_io
	{
		pTCPConnect TCPConnect;
		pTransportFkt TLSConnect;
		pTransportFkt TLSAccept;
		pTransportAttach TransportAttach;
		pTransportFkt TransportDisconnect;
		pTransportRWFkt ReadPdu;  /* Reads a whole PDU from the transport */
		pTransportRWFkt WritePdu; /* Writes a whole PDU to the transport */
		pTransportRead ReadBytes; /* Reads up to a requested amount of bytes */
		pTransportGetPublicKey GetPublicKey;       /** @since version 3.2.0 */
		pTransportSetBlockingMode SetBlockingMode; /** @since version 3.3.0 */
		pTransportConnectLayer ConnectLayer;       /** @since 3.9.0 */
		pTransportAttachLayer AttachLayer;         /** @since 3.9.0 */
		UINT64 reserved[64 - 12]; /* Reserve some space for ABI compatibility */
	};
	typedef struct rdp_transport_io rdpTransportIo;

	FREERDP_API BOOL freerdp_io_callback_set_event(rdpContext* context, BOOL set);

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

	/**
	 * @brief Free a transport layer instance
	 * @param layer A pointer to the layer to free or \b NULL
	 * @since version 3.9.0
	 */
	FREERDP_API void transport_layer_free(rdpTransportLayer* layer);

	/**
	 * @brief Create new transport layer instance
	 * @param transport A pointer to the transport instance to use
	 * @param contextSize The size of the context to use
	 * @return A new transport layer instance or \b NULL in case of failure
	 * @since version 3.9.0
	 */
	WINPR_ATTR_MALLOC(transport_layer_free, 1)
	FREERDP_API rdpTransportLayer* transport_layer_new(rdpTransport* transport, size_t contextSize);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_TRANSPORT_IO_H */

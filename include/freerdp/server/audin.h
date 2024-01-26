/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server Audio Input Virtual Channel
 *
 * Copyright 2012 Vic Lee
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2023 Pascal Nowack <Pascal.Nowack@gmx.de>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_CHANNEL_AUDIN_SERVER_H
#define FREERDP_CHANNEL_AUDIN_SERVER_H

#include <freerdp/config.h>

#include <freerdp/channels/audin.h>
#include <freerdp/channels/wtsvc.h>

#if !defined(CHANNEL_AUDIN_SERVER)
#error "This header must not be included if CHANNEL_AUDIN_SERVER is not defined"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct s_audin_server_context audin_server_context;

	typedef BOOL (*psAudinServerChannelOpen)(audin_server_context* context);
	typedef BOOL (*psAudinServerChannelIsOpen)(audin_server_context* context);
	typedef BOOL (*psAudinServerChannelClose)(audin_server_context* context);

	typedef BOOL (*psAudinServerChannelIdAssigned)(audin_server_context* context, UINT32 channelId);

	typedef UINT (*psAudinServerVersion)(audin_server_context* context,
	                                     const SNDIN_VERSION* version);
	typedef UINT (*psAudinServerFormats)(audin_server_context* context,
	                                     const SNDIN_FORMATS* formats);
	typedef UINT (*psAudinServerOpen)(audin_server_context* context, const SNDIN_OPEN* open);
	typedef UINT (*psAudinServerOpenReply)(audin_server_context* context,
	                                       const SNDIN_OPEN_REPLY* open_reply);
	typedef UINT (*psAudinServerIncomingData)(audin_server_context* context,
	                                          const SNDIN_DATA_INCOMING* data_incoming);
	typedef UINT (*psAudinServerData)(audin_server_context* context, const SNDIN_DATA* data);
	typedef UINT (*psAudinServerFormatChange)(audin_server_context* context,
	                                          const SNDIN_FORMATCHANGE* format_change);

	struct s_audin_server_context
	{
		HANDLE vcm;

		/* Server self-defined pointer. */
		void* userdata;

		/**
		 * Server version to send to the client, when the DVC was successfully
		 * opened.
		 **/
		SNDIN_VERSION_Version serverVersion;

		/*** APIs called by the server. ***/

		/**
		 * Open the audio input channel.
		 */
		psAudinServerChannelOpen Open;

		/**
		 * Check, whether the audio input channel thread was created
		 */
		psAudinServerChannelIsOpen IsOpen;

		/**
		 * Close the audio input channel.
		 */
		psAudinServerChannelClose Close;

		/**
		 * For the following server to client PDUs,
		 * the message header does not have to be set.
		 */

		/**
		 * Send a Version PDU.
		 */
		psAudinServerVersion SendVersion;

		/**
		 * Send a Sound Formats PDU.
		 */
		psAudinServerFormats SendFormats;

		/**
		 * Send an Open PDU.
		 */
		psAudinServerOpen SendOpen;

		/**
		 * Send a Format Change PDU.
		 */
		psAudinServerFormatChange SendFormatChange;

		/*** Callbacks registered by the server. ***/

		/**
		 * Callback, when the channel got its id assigned.
		 */
		psAudinServerChannelIdAssigned ChannelIdAssigned;

		/*
		 * Callback for the Version PDU.
		 */
		psAudinServerVersion ReceiveVersion;

		/*
		 * Callback for the Sound Formats PDU.
		 */
		psAudinServerFormats ReceiveFormats;

		/*
		 * Callback for the Open Reply PDU.
		 */
		psAudinServerOpenReply OpenReply;

		/*
		 * Callback for the Incoming Data PDU.
		 */
		psAudinServerIncomingData IncomingData;

		/*
		 * Callback for the Data PDU.
		 */
		psAudinServerData Data;

		/*
		 * Callback for the Format Change PDU.
		 */
		psAudinServerFormatChange ReceiveFormatChange;

		rdpContext* rdpcontext;
	};

	FREERDP_API void audin_server_context_free(audin_server_context* context);

	WINPR_ATTR_MALLOC(audin_server_context_free, 1)
	FREERDP_API audin_server_context* audin_server_context_new(HANDLE vcm);

	/** \brief sets the supported audio formats for AUDIN server channel context.
	 *
	 *  \param context The context to set the formats for
	 *  \param count The number of formats found in \b formats. Use \b -1 to set to default formats
	 * supported by FreeRDP \param formats An array of \b count elements
	 *
	 *  \return \b TRUE if successful and at least one format is supported, \b FALSE otherwise.
	 */
	FREERDP_API BOOL audin_server_set_formats(audin_server_context* context, SSIZE_T count,
	                                          const AUDIO_FORMAT* formats);

	FREERDP_API const AUDIO_FORMAT*
	audin_server_get_negotiated_format(const audin_server_context* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_AUDIN_SERVER_H */

/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Interface
 *
 * Copyright 2009-2011 Jay Sorg
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

#ifndef FREERDP_H
#define FREERDP_H

typedef struct rdp_rdp rdpRdp;
typedef struct rdp_gdi rdpGdi;
typedef struct rdp_rail rdpRail;
typedef struct rdp_cache rdpCache;
typedef struct rdp_channels rdpChannels;
typedef struct rdp_graphics rdpGraphics;

typedef struct rdp_freerdp freerdp;
typedef struct rdp_context rdpContext;
typedef struct rdp_freerdp_peer freerdp_peer;

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/error.h>
#include <freerdp/settings.h>
#include <freerdp/extension.h>
#include <freerdp/utils/stream.h>

#include <freerdp/input.h>
#include <freerdp/update.h>
#include <freerdp/message.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*pContextNew)(freerdp* instance, rdpContext* context);
typedef void (*pContextFree)(freerdp* instance, rdpContext* context);

typedef BOOL (*pPreConnect)(freerdp* instance);
typedef BOOL (*pPostConnect)(freerdp* instance);
typedef BOOL (*pAuthenticate)(freerdp* instance, char** username, char** password, char** domain);
typedef BOOL (*pVerifyCertificate)(freerdp* instance, char* subject, char* issuer, char* fingerprint);
typedef BOOL (*pVerifyChangedCertificate)(freerdp* instance, char* subject, char* issuer, char* new_fingerprint, char* old_fingerprint);

typedef int (*pSendChannelData)(freerdp* instance, int channelId, BYTE* data, int size);
typedef int (*pReceiveChannelData)(freerdp* instance, int channelId, BYTE* data, int size, int flags, int total_size);

/**
 * Defines the context for a given instance of RDP connection.
 * It is embedded in the rdp_freerdp structure, and allocated by a call to freerdp_context_new().
 * It is deallocated by a call to freerdp_context_free().
 */
struct rdp_context
{
	freerdp* instance; /**< (offset 0)
						  Pointer to a rdp_freerdp structure.
						  This is a back-link to retrieve the freerdp instance from the context.
						  It is set by the freerdp_context_new() function */
	freerdp_peer* peer; /**< (offset 1)
						   Pointer to the client peer.
						   This is set by a call to freerdp_peer_context_new() during peer initialization.
						   This field is used only on the server side. */
	UINT32 paddingA[16 - 2]; /* 2 */

	int argc;	/**< (offset 16)
				   Number of arguments given to the program at launch time.
				   Used to keep this data available and used later on, typically just before connection initialization.
				   @see freerdp_parse_args() */
	char** argv; /**< (offset 17)
					List of arguments given to the program at launch time.
					Used to keep this data available and used later on, typically just before connection initialization.
					@see freerdp_parse_args() */

	UINT32 paddingB[32 - 18]; /* 18 */

	rdpRdp* rdp; /**< (offset 32)
					Pointer to a rdp_rdp structure used to keep the connection's parameters.
					It is allocated by freerdp_context_new() and deallocated by freerdp_context_free(), at the same
					time that this rdp_context structure - there is no need to specifically allocate/deallocate this. */
	rdpGdi* gdi; /**< (offset 33)
					Pointer to a rdp_gdi structure used to keep the gdi settings.
					It is allocated by gdi_init() and deallocated by gdi_free().
					It must be deallocated before deallocating this rdp_context structure. */
	rdpRail* rail; /* 34 */
	rdpCache* cache; /* 35 */
	rdpChannels* channels; /* 36 */
	rdpGraphics* graphics; /* 37 */
	rdpInput* input; /* 38 */
	rdpUpdate* update; /* 39 */
	rdpSettings* settings; /* 40 */
	UINT32 paddingC[64 - 41]; /* 41 */
};

/** Defines the options for a given instance of RDP connection.
 *  This is built by the client and given to the FreeRDP library to create the connection
 *  with the expected options.
 *  It is allocated by a call to freerdp_new() and deallocated by a call to freerdp_free().
 *  Some of its content need specific allocation/deallocation - see field description for details.
 */
struct rdp_freerdp
{
	rdpContext* context; /**< (offset 0)
							  Pointer to a rdpContext structure.
							  Client applications can use the context_size field to register a context bigger than the rdpContext
							  structure. This allow clients to use additional context information.
							  When using this capability, client application should ALWAYS declare their structure with the
							  rdpContext field first, and any additional content following it.
							  Can be allocated by a call to freerdp_context_new().
							  Must be deallocated by a call to freerdp_context_free() before deallocating the current instance. */

	UINT32 paddingA[16 - 1]; /* 1 */

	rdpInput* input; /* (offset 16)
						Input handle for the connection.
						Will be initialized by a call to freerdp_context_new() */
	rdpUpdate* update; /* (offset 17)
						  Update display parameters. Used to register display events callbacks and settings.
						  Will be initialized by a call to freerdp_context_new() */
	rdpSettings* settings; /**< (offset 18)
								Pointer to a rdpSettings structure. Will be used to maintain the required RDP settings.
								Will be initialized by a call to freerdp_context_new() */
	UINT32 paddingB[32 - 19]; /* 19 */

	size_t context_size; /* (offset 32)
							Specifies the size of the 'context' field. freerdp_context_new() will use this size to allocate the context buffer.
							freerdp_new() sets it to sizeof(rdpContext).
							If modifying it, there should always be a minimum of sizeof(rdpContext), as the freerdp library will assume it can use the
							'context' field to set the required informations in it.
							Clients will typically make it bigger, and use a context structure embedding the rdpContext, and
							adding additional information after that.
						 */

	pContextNew ContextNew; /**< (offset 33)
								 Callback for context allocation
								 Can be set before calling freerdp_context_new() to have it executed after allocation and initialization.
								 Must be set to NULL if not needed. */

	pContextFree ContextFree; /**< (offset 34)
								   Callback for context deallocation
								   Can be set before calling freerdp_context_free() to have it executed before deallocation.
								   Must be set to NULL if not needed. */
	UINT32 paddingC[48 - 35]; /* 35 */

	pPreConnect PreConnect; /**< (offset 48)
								 Callback for pre-connect operations.
								 Can be set before calling freerdp_connect() to have it executed before the actual connection happens.
								 Must be set to NULL if not needed. */

	pPostConnect PostConnect; /**< (offset 49)
								   Callback for post-connect operations.
								   Can be set before calling freerdp_connect() to have it executed after the actual connection has succeeded.
								   Must be set to NULL if not needed. */

	pAuthenticate Authenticate; /**< (offset 50)
									 Callback for authentication.
									 It is used to get the username/password when it was not provided at connection time. */
	pVerifyCertificate VerifyCertificate; /**< (offset 51)
											   Callback for certificate validation.
											   Used to verify that an unknown certificate is trusted. */
	pVerifyChangedCertificate VerifyChangedCertificate; /**< (offset 52)
															 Callback for changed certificate validation. 
															 Used when a certificate differs from stored fingerprint.
															 If returns TRUE, the new fingerprint will be trusted and old thrown out. */
	UINT32 paddingD[64 - 51]; /* 51 */

	pSendChannelData SendChannelData; /* (offset 64)
										 Callback for sending data to a channel.
										 By default, it is set by freerdp_new() to freerdp_send_channel_data(), which eventually calls
										 freerdp_channel_send() */
	pReceiveChannelData ReceiveChannelData; /* (offset 65)
											   Callback for receiving data from a channel.
											   This is called by freerdp_channel_process() (if not NULL).
											   Clients will typically use a function that calls freerdp_channels_data() to perform the needed tasks. */
	UINT32 paddingE[80 - 66]; /* 66 */
};

FREERDP_API void freerdp_context_new(freerdp* instance);
FREERDP_API void freerdp_context_free(freerdp* instance);

FREERDP_API BOOL freerdp_connect(freerdp* instance);
FREERDP_API BOOL freerdp_shall_disconnect(freerdp* instance);
FREERDP_API BOOL freerdp_disconnect(freerdp* instance);

FREERDP_API BOOL freerdp_get_fds(freerdp* instance, void** rfds, int* rcount, void** wfds, int* wcount);
FREERDP_API BOOL freerdp_check_fds(freerdp* instance);

FREERDP_API wMessageQueue* freerdp_get_message_queue(freerdp* instance, DWORD id);
FREERDP_API HANDLE freerdp_get_message_queue_event_handle(freerdp* instance, DWORD id);
FREERDP_API int freerdp_message_queue_process_message(freerdp* instance, DWORD id, wMessage* message);
FREERDP_API int freerdp_message_queue_process_pending_messages(freerdp* instance, DWORD id);

FREERDP_API UINT32 freerdp_error_info(freerdp* instance);

FREERDP_API void freerdp_get_version(int* major, int* minor, int* revision);

FREERDP_API freerdp* freerdp_new(void);
FREERDP_API void freerdp_free(freerdp* instance);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_H */

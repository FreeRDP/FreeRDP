/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#ifndef __FREERDP_H
#define __FREERDP_H

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
#include <freerdp/settings.h>
#include <freerdp/extension.h>

#include <freerdp/input.h>
#include <freerdp/update.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*pContextNew)(freerdp* instance, rdpContext* context);
typedef void (*pContextFree)(freerdp* instance, rdpContext* context);

typedef boolean (*pPreConnect)(freerdp* instance);
typedef boolean (*pPostConnect)(freerdp* instance);
typedef boolean (*pAuthenticate)(freerdp* instance, char** username, char** password, char** domain);
typedef boolean (*pVerifyCertificate)(freerdp* instance, char* subject, char* issuer, char* fingerprint);

typedef int (*pSendChannelData)(freerdp* instance, int channelId, uint8* data, int size);
typedef int (*pReceiveChannelData)(freerdp* instance, int channelId, uint8* data, int size, int flags, int total_size);

struct rdp_context
{
	freerdp* instance; /* 0 */
	freerdp_peer* peer; /* 1 */
	uint32 paddingA[16 - 2]; /* 2 */

	int argc; /* 16 */
	char** argv; /* 17 */
	uint32 paddingB[32 - 18]; /* 18 */

	rdpRdp* rdp; /* 32 */
	rdpGdi* gdi; /* 33 */
	rdpRail* rail; /* 34 */
	rdpCache* cache; /* 35 */
	rdpChannels* channels; /* 36 */
	rdpGraphics* graphics; /* 37 */
	uint32 paddingC[64 - 38]; /* 38 */
};

struct rdp_freerdp
{
	rdpContext* context; /* 0 */
	uint32 paddingA[16 - 1]; /* 1 */

	rdpInput* input; /* 16 */
	rdpUpdate* update; /* 17 */
	rdpSettings* settings; /* 18 */
	uint32 paddingB[32 - 19]; /* 19 */

	size_t context_size; /* 32 */
	pContextNew ContextNew; /* 33 */
	pContextFree ContextFree; /* 34 */
	uint32 paddingC[48 - 35]; /* 35 */

	pPreConnect PreConnect; /* 48 */
	pPostConnect PostConnect; /* 49 */
	pAuthenticate Authenticate; /* 50 */
	pVerifyCertificate VerifyCertificate; /* 51 */
	uint32 paddingD[64 - 52]; /* 52 */

	pSendChannelData SendChannelData; /* 64 */
	pReceiveChannelData ReceiveChannelData; /* 65 */
	uint32 paddingE[80 - 66]; /* 66 */
};

FREERDP_API void freerdp_context_new(freerdp* instance);
FREERDP_API void freerdp_context_free(freerdp* instance);

FREERDP_API boolean freerdp_connect(freerdp* instance);
FREERDP_API boolean freerdp_shall_disconnect(freerdp* instance);
FREERDP_API boolean freerdp_disconnect(freerdp* instance);

FREERDP_API boolean freerdp_get_fds(freerdp* instance, void** rfds, int* rcount, void** wfds, int* wcount);
FREERDP_API boolean freerdp_check_fds(freerdp* instance);

FREERDP_API void freerdp_send_keep_alive(freerdp* instance);
FREERDP_API uint32 freerdp_error_info(freerdp* instance);

FREERDP_API void freerdp_get_version(int* major, int* minor, int* revision);

FREERDP_API freerdp* freerdp_new();
FREERDP_API void freerdp_free(freerdp* instance);

#ifdef __cplusplus
}
#endif

#endif

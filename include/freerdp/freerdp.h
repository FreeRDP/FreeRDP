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
	freerdp* instance;
	freerdp_peer* peer;
	uint32 paddingA[16 - 4]; /* offset 64 */

	int argc;
	char** argv;
	uint32 paddingB[16 - 3]; /* offset 128 */

	rdpRdp* rdp;
	rdpGdi* gdi;
	rdpRail* rail;
	rdpCache* cache;
	rdpChannels* channels;
	rdpGraphics* graphics;
	uint32 paddingC[32 - 12]; /* offset 256 */
};

struct rdp_freerdp
{
	rdpContext* context;
	uint32 paddingA[16 - 2]; /* offset 64 */

	rdpInput* input;
	rdpUpdate* update;
	rdpSettings* settings;
	uint32 paddingB[16 - 6]; /* offset 128 */

	size_t context_size;
	pContextNew ContextNew;
	pContextFree ContextFree;
	uint32 paddingC[16 - 6]; /* offset 192 */

	pPreConnect PreConnect;
	pPostConnect PostConnect;
	pAuthenticate Authenticate;
	pVerifyCertificate VerifyCertificate;
	uint32 paddingD[16 - 8]; /* offset 256 */

	pSendChannelData SendChannelData;
	pReceiveChannelData ReceiveChannelData;
	uint32 paddingE[16 - 4]; /* offset 320 */
};

FREERDP_API void freerdp_context_new(freerdp* instance);
FREERDP_API void freerdp_context_free(freerdp* instance);

FREERDP_API boolean freerdp_connect(freerdp* instance);
FREERDP_API boolean freerdp_disconnect(freerdp* instance);

FREERDP_API boolean freerdp_get_fds(freerdp* instance, void** rfds, int* rcount, void** wfds, int* wcount);
FREERDP_API boolean freerdp_check_fds(freerdp* instance);

FREERDP_API uint32 freerdp_error_info(freerdp* instance);

FREERDP_API freerdp* freerdp_new();
FREERDP_API void freerdp_free(freerdp* instance);

#ifdef __cplusplus
}
#endif

#endif

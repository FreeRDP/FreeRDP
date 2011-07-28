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

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/settings.h>
#include <freerdp/extension.h>

#include <freerdp/input.h>
#include <freerdp/update.h>

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API boolean freerdp_global_init();
FREERDP_API void freerdp_global_finish();

typedef struct rdp_freerdp freerdp;

typedef int (*pcChannelDataInput)(freerdp* freerdp, int channelId, uint8* data, int size);

struct rdp_freerdp
{
	void* rdp;
	void* param1;
	void* param2;
	void* param3;
	void* param4;
	rdpInput* input;
	rdpUpdate* update;
	rdpSettings* settings;
	pcChannelDataInput ChannelDataInput;
};

FREERDP_API freerdp* freerdp_new();
FREERDP_API void freerdp_free(freerdp* instance);

#ifdef __cplusplus
}
#endif

#endif

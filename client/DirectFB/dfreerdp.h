/**
 * FreeRDP: A Remote Desktop Protocol Client
 * DirectFB Client
 *
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

#ifndef __DFREERDP_H
#define __DFREERDP_H

#include "gdi.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <directfb.h>
#include <freerdp/freerdp.h>
#include <freerdp/chanman.h>

#define SET_DFI(_instance, _dfi) (_instance)->param1 = _dfi
#define GET_DFI(_instance) ((dfInfo*) ((_instance)->param1))

#define SET_CHANMAN(_instance, _chanman) (_instance)->param2 = _chanman
#define GET_CHANMAN(_instance) ((rdpChanMan*) ((_instance)->param2))

struct df_info
{
	int read_fds;
	DFBResult err;
	IDirectFB* dfb;
	DFBEvent event;
	DFBRectangle update_rect;
	DFBSurfaceDescription dsc;
	IDirectFBSurface* primary;
	IDirectFBSurface* surface;
	IDirectFBDisplayLayer* layer;
	IDirectFBEventBuffer* event_buffer;
};
typedef struct df_info dfInfo;

#endif /* __DFREERDP_H */

/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Optimized Remoting Virtual Channel Extension
 *
 * Copyright 2017 David Fort <contact@hardening-consulting.com>
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

#ifndef FREERDP_CHANNELS_CLIENT_VIDEO_H
#define FREERDP_CHANNELS_CLIENT_VIDEO_H

#include <freerdp/client/geometry.h>
#include <freerdp/channels/video.h>

typedef struct _VideoClientContext VideoClientContext;
typedef struct _VideoClientContextPriv VideoClientContextPriv;
typedef struct _VideoSurface VideoSurface;


/** @brief an implementation of surface used by the video channel */
struct _VideoSurface
{
	UINT32 x, y, w, h;
	BYTE *data;
};

typedef void (*pcVideoTimer)(VideoClientContext *video, UINT64 now);
typedef void (*pcVideoSetGeometry)(VideoClientContext *video, GeometryClientContext *geometry);
typedef VideoSurface *(*pcVideoCreateSurface)(VideoClientContext *video, BYTE *data, UINT32 x, UINT32 y,
			UINT32 width, UINT32 height);
typedef BOOL (*pcVideoShowSurface)(VideoClientContext *video, VideoSurface *surface);
typedef BOOL (*pcVideoDeleteSurface)(VideoClientContext *video, VideoSurface *surface);

/** @brief context for the video (MS-RDPEVOR) channel */
struct _VideoClientContext
{
	void* handle;
	void* custom;
	VideoClientContextPriv *priv;

	pcVideoSetGeometry setGeometry;
	pcVideoTimer timer;
	pcVideoCreateSurface createSurface;
	pcVideoShowSurface showSurface;
	pcVideoDeleteSurface deleteSurface;
};


#endif /* FREERDP_CHANNELS_CLIENT_VIDEO_H */


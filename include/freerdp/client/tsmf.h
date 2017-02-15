/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Multimedia Redirection Virtual Channel Types
 *
 * Copyright 2011 Vic Lee
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

#ifndef FREERDP_CHANNEL_CLIENT_TSMF_H
#define FREERDP_CHANNEL_CLIENT_TSMF_H

#include <freerdp/codec/region.h>

#include <freerdp/channels/tsmf.h>

/* RDP_VIDEO_FRAME_EVENT.frame_pixfmt */
/* http://www.fourcc.org/yuv.php */
#define RDP_PIXFMT_I420		0x30323449
#define RDP_PIXFMT_YV12		0x32315659

struct _TSMF_VIDEO_FRAME_EVENT
{
	BYTE* frameData;
	UINT32 frameSize;
	UINT32 framePixFmt;
	INT16 frameWidth;
	INT16 frameHeight;
	INT16 x;
	INT16 y;
	INT16 width;
	INT16 height;
	UINT16 numVisibleRects;
	RECTANGLE_16* visibleRects;
};
typedef struct _TSMF_VIDEO_FRAME_EVENT TSMF_VIDEO_FRAME_EVENT;

/**
 * Client Interface
 */

typedef struct _tsmf_client_context TsmfClientContext;

typedef int (*pcTsmfFrameEvent)(TsmfClientContext* context, TSMF_VIDEO_FRAME_EVENT* event);

struct _tsmf_client_context
{
	void* handle;
	void* custom;

	pcTsmfFrameEvent FrameEvent;
};

#endif /* FREERDP_CHANNEL_CLIENT_TSMF_H */

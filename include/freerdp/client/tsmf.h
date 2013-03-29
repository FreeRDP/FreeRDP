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

struct _RDP_VIDEO_FRAME_EVENT
{
	wMessage event;
	BYTE* frame_data;
	UINT32 frame_size;
	UINT32 frame_pixfmt;
	INT16 frame_width;
	INT16 frame_height;
	INT16 x;
	INT16 y;
	INT16 width;
	INT16 height;
	UINT16 num_visible_rects;
	RDP_RECT* visible_rects;
};
typedef struct _RDP_VIDEO_FRAME_EVENT RDP_VIDEO_FRAME_EVENT;

struct _RDP_REDRAW_EVENT
{
	wMessage event;
	INT16 x;
	INT16 y;
	INT16 width;
	INT16 height;
};
typedef struct _RDP_REDRAW_EVENT RDP_REDRAW_EVENT;

/* RDP_VIDEO_FRAME_EVENT.frame_pixfmt */
/* http://www.fourcc.org/yuv.php */
#define RDP_PIXFMT_I420		0x30323449
#define RDP_PIXFMT_YV12		0x32315659

#endif /* FREERDP_CHANNEL_CLIENT_TSMF_H */

/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#ifndef __TSMF_PLUGIN
#define __TSMF_PLUGIN

/**
 * Event Types
 */
enum RDP_EVENT_TYPE_TSMF
{
	RDP_EVENT_TYPE_TSMF_VIDEO_FRAME = 1,
	RDP_EVENT_TYPE_TSMF_REDRAW
};

struct _RDP_VIDEO_FRAME_EVENT
{
	RDP_EVENT event;
	uint8* frame_data;
	uint32 frame_size;
	uint32 frame_pixfmt;
	sint16 frame_width;
	sint16 frame_height;
	sint16 x;
	sint16 y;
	sint16 width;
	sint16 height;
	uint16 num_visible_rects;
	RDP_RECT* visible_rects;
};
typedef struct _RDP_VIDEO_FRAME_EVENT RDP_VIDEO_FRAME_EVENT;

struct _RDP_REDRAW_EVENT
{
	RDP_EVENT event;
	sint16 x;
	sint16 y;
	sint16 width;
	sint16 height;
};
typedef struct _RDP_REDRAW_EVENT RDP_REDRAW_EVENT;

/* RDP_VIDEO_FRAME_EVENT.frame_pixfmt */
/* http://www.fourcc.org/yuv.php */
#define RDP_PIXFMT_I420		0x30323449
#define RDP_PIXFMT_YV12		0x32315659

#endif /* __TSMF_PLUGIN */

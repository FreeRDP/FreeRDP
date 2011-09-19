/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Video Redirection Virtual Channel - Media Container
 *
 * Copyright 2010-2011 Vic Lee
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

/**
 * The media container maintains a global list of presentations, and a list of
 * streams in each presentation.
 */

#ifndef __TSMF_MEDIA_H
#define __TSMF_MEDIA_H

typedef struct _TSMF_PRESENTATION TSMF_PRESENTATION;

typedef struct _TSMF_STREAM TSMF_STREAM;

typedef struct _TSMF_SAMPLE TSMF_SAMPLE;

TSMF_PRESENTATION* tsmf_presentation_new(const uint8* guid, IWTSVirtualChannelCallback* pChannelCallback);
TSMF_PRESENTATION* tsmf_presentation_find_by_id(const uint8* guid);
void tsmf_presentation_start(TSMF_PRESENTATION* presentation);
void tsmf_presentation_stop(TSMF_PRESENTATION* presentation);
void tsmf_presentation_set_geometry_info(TSMF_PRESENTATION* presentation,
	uint32 x, uint32 y, uint32 width, uint32 height,
	int num_rects, RDP_RECT* rects);
void tsmf_presentation_set_audio_device(TSMF_PRESENTATION* presentation,
	const char* name, const char* device);
void tsmf_presentation_flush(TSMF_PRESENTATION* presentation);
void tsmf_presentation_free(TSMF_PRESENTATION* presentation);

TSMF_STREAM* tsmf_stream_new(TSMF_PRESENTATION* presentation, uint32 stream_id);
TSMF_STREAM* tsmf_stream_find_by_id(TSMF_PRESENTATION* presentation, uint32 stream_id);
void tsmf_stream_set_format(TSMF_STREAM* stream, const char* name, STREAM* s);
void tsmf_stream_end(TSMF_STREAM* stream);
void tsmf_stream_free(TSMF_STREAM* stream);

void tsmf_stream_push_sample(TSMF_STREAM* stream, IWTSVirtualChannelCallback* pChannelCallback,
	uint32 sample_id, uint64 start_time, uint64 end_time, uint64 duration, uint32 extensions,
	uint32 data_size, uint8* data);

void tsmf_media_init(void);

#endif


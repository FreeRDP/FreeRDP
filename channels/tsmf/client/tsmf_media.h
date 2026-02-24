/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Redirection Virtual Channel - Media Container
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2012 Hewlett-Packard Development Company, L.P.
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#ifndef FREERDP_CHANNEL_TSMF_CLIENT_MEDIA_H
#define FREERDP_CHANNEL_TSMF_CLIENT_MEDIA_H

#include <freerdp/freerdp.h>

typedef struct S_TSMF_PRESENTATION TSMF_PRESENTATION;

typedef struct S_TSMF_STREAM TSMF_STREAM;

typedef struct S_TSMF_SAMPLE TSMF_SAMPLE;

FREERDP_LOCAL void tsmf_presentation_free(TSMF_PRESENTATION* presentation);

WINPR_ATTR_MALLOC(tsmf_presentation_free, 1)
WINPR_ATTR_NODISCARD FREERDP_LOCAL TSMF_PRESENTATION*
tsmf_presentation_new(const BYTE* guid, IWTSVirtualChannelCallback* pChannelCallback);

WINPR_ATTR_NODISCARD FREERDP_LOCAL TSMF_PRESENTATION*
tsmf_presentation_find_by_id(const BYTE* guid);

WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL tsmf_presentation_start(TSMF_PRESENTATION* presentation);

WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL tsmf_presentation_stop(TSMF_PRESENTATION* presentation);

WINPR_ATTR_NODISCARD FREERDP_LOCAL UINT tsmf_presentation_sync(TSMF_PRESENTATION* presentation);

WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL tsmf_presentation_paused(TSMF_PRESENTATION* presentation);

WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL
tsmf_presentation_restarted(TSMF_PRESENTATION* presentation);

WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL
tsmf_presentation_volume_changed(TSMF_PRESENTATION* presentation, UINT32 newVolume, UINT32 muted);

WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL tsmf_presentation_set_geometry_info(
    TSMF_PRESENTATION* presentation, UINT32 x, UINT32 y, UINT32 width, UINT32 height,
    UINT32 num_rects, const RECTANGLE_32* rects);

FREERDP_LOCAL
void tsmf_presentation_set_audio_device(TSMF_PRESENTATION* presentation, const char* name,
                                        const char* device);

FREERDP_LOCAL
void tsmf_stream_free(TSMF_STREAM* stream);

WINPR_ATTR_MALLOC(tsmf_stream_free, 1)
WINPR_ATTR_NODISCARD FREERDP_LOCAL TSMF_STREAM*
tsmf_stream_new(TSMF_PRESENTATION* presentation, UINT32 stream_id, rdpContext* rdpcontext);

WINPR_ATTR_NODISCARD FREERDP_LOCAL TSMF_STREAM*
tsmf_stream_find_by_id(TSMF_PRESENTATION* presentation, UINT32 stream_id);

WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL tsmf_stream_set_format(TSMF_STREAM* stream,
                                                               const char* name, wStream* s);

FREERDP_LOCAL
void tsmf_stream_end(TSMF_STREAM* stream, UINT32 message_id,
                     IWTSVirtualChannelCallback* pChannelCallback);

WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL tsmf_stream_flush(TSMF_STREAM* stream);

WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL
tsmf_stream_push_sample(TSMF_STREAM* stream, IWTSVirtualChannelCallback* pChannelCallback,
                        UINT32 sample_id, UINT64 start_time, UINT64 end_time, UINT64 duration,
                        UINT32 extensions, UINT32 data_size, BYTE* data);

WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL tsmf_media_init(void);

FREERDP_LOCAL
void tsmf_stream_start_threads(TSMF_STREAM* stream);

#endif /* FREERDP_CHANNEL_TSMF_CLIENT_MEDIA_H */

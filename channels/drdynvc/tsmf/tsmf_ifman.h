/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Video Redirection Virtual Channel - Interface Manipulation
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

#ifndef __TSMF_IFMAN_H
#define __TSMF_IFMAN_H

typedef struct _TSMF_IFMAN TSMF_IFMAN;
struct _TSMF_IFMAN
{
	IWTSVirtualChannelCallback* channel_callback;
	const char* decoder_name;
	const char* audio_name;
	const char* audio_device;
	uint8 presentation_id[16];
	uint32 stream_id;
	uint32 message_id;

	STREAM* input;
	uint32 input_size;
	STREAM* output;
	boolean output_pending;
	uint32 output_interface_id;
};

int tsmf_ifman_rim_exchange_capability_request(TSMF_IFMAN* ifman);
int tsmf_ifman_exchange_capability_request(TSMF_IFMAN* ifman);
int tsmf_ifman_check_format_support_request(TSMF_IFMAN* ifman);
int tsmf_ifman_on_new_presentation(TSMF_IFMAN* ifman);
int tsmf_ifman_add_stream(TSMF_IFMAN* ifman);
int tsmf_ifman_set_topology_request(TSMF_IFMAN* ifman);
int tsmf_ifman_remove_stream(TSMF_IFMAN* ifman);
int tsmf_ifman_shutdown_presentation(TSMF_IFMAN* ifman);
int tsmf_ifman_on_stream_volume(TSMF_IFMAN* ifman);
int tsmf_ifman_on_channel_volume(TSMF_IFMAN* ifman);
int tsmf_ifman_set_video_window(TSMF_IFMAN* ifman);
int tsmf_ifman_update_geometry_info(TSMF_IFMAN* ifman);
int tsmf_ifman_set_allocator(TSMF_IFMAN* ifman);
int tsmf_ifman_notify_preroll(TSMF_IFMAN* ifman);
int tsmf_ifman_on_sample(TSMF_IFMAN* ifman);
int tsmf_ifman_on_flush(TSMF_IFMAN* ifman);
int tsmf_ifman_on_end_of_stream(TSMF_IFMAN* ifman);
int tsmf_ifman_on_playback_started(TSMF_IFMAN* ifman);
int tsmf_ifman_on_playback_paused(TSMF_IFMAN* ifman);
int tsmf_ifman_on_playback_restarted(TSMF_IFMAN* ifman);
int tsmf_ifman_on_playback_stopped(TSMF_IFMAN* ifman);
int tsmf_ifman_on_playback_rate_changed(TSMF_IFMAN* ifman);

#endif


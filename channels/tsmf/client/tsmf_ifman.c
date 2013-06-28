/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Redirection Virtual Channel - Interface Manipulation
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2012 Hewlett-Packard Development Company, L.P.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>

#include <winpr/stream.h>

#include "tsmf_types.h"
#include "tsmf_constants.h"
#include "tsmf_media.h"
#include "tsmf_codec.h"

#include "tsmf_ifman.h"

int tsmf_ifman_rim_exchange_capability_request(TSMF_IFMAN* ifman)
{
	UINT32 CapabilityValue;

	Stream_Read_UINT32(ifman->input, CapabilityValue);
	DEBUG_DVC("server CapabilityValue %d", CapabilityValue);

	Stream_EnsureRemainingCapacity(ifman->output, 8);
	Stream_Write_UINT32(ifman->output, 1); /* CapabilityValue */
	Stream_Write_UINT32(ifman->output, 0); /* Result */

	return 0;
}

int tsmf_ifman_exchange_capability_request(TSMF_IFMAN* ifman)
{
	UINT32 i;
	UINT32 v;
	UINT32 pos;
	UINT32 CapabilityType;
	UINT32 cbCapabilityLength;
	UINT32 numHostCapabilities;

	pos = Stream_GetPosition(ifman->output);
	Stream_EnsureRemainingCapacity(ifman->output, ifman->input_size + 4);
	Stream_Copy(ifman->output, ifman->input, ifman->input_size);

	Stream_SetPosition(ifman->output, pos);
	Stream_Read_UINT32(ifman->output, numHostCapabilities);

	for (i = 0; i < numHostCapabilities; i++)
	{
		Stream_Read_UINT32(ifman->output, CapabilityType);
		Stream_Read_UINT32(ifman->output, cbCapabilityLength);
		pos = Stream_GetPosition(ifman->output);

		switch (CapabilityType)
		{
			case 1: /* Protocol version request */
				Stream_Read_UINT32(ifman->output, v);
				DEBUG_DVC("server protocol version %d", v);
				break;
			case 2: /* Supported platform */
				Stream_Peek_UINT32(ifman->output, v);
				DEBUG_DVC("server supported platform %d", v);
				/* Claim that we support both MF and DShow platforms. */
				Stream_Write_UINT32(ifman->output,
					MMREDIR_CAPABILITY_PLATFORM_MF | MMREDIR_CAPABILITY_PLATFORM_DSHOW);
				break;
			default:
				DEBUG_WARN("unknown capability type %d", CapabilityType);
				break;
		}
		Stream_SetPosition(ifman->output, pos + cbCapabilityLength);
	}
	Stream_Write_UINT32(ifman->output, 0); /* Result */

	ifman->output_interface_id = TSMF_INTERFACE_DEFAULT | STREAM_ID_STUB;

	return 0;
}

int tsmf_ifman_check_format_support_request(TSMF_IFMAN* ifman)
{
	UINT32 numMediaType;
	UINT32 PlatformCookie;
	UINT32 FormatSupported = 1;

	Stream_Read_UINT32(ifman->input, PlatformCookie);
	Stream_Seek_UINT32(ifman->input); /* NoRolloverFlags (4 bytes) */
	Stream_Read_UINT32(ifman->input, numMediaType);

	DEBUG_DVC("PlatformCookie %d numMediaType %d", PlatformCookie, numMediaType);

	if (!tsmf_codec_check_media_type(ifman->input))
		FormatSupported = 0;

	if (FormatSupported)
		DEBUG_DVC("format ok.");

	Stream_EnsureRemainingCapacity(ifman->output, 12);
	Stream_Write_UINT32(ifman->output, FormatSupported);
	Stream_Write_UINT32(ifman->output, PlatformCookie);
	Stream_Write_UINT32(ifman->output, 0); /* Result */

	ifman->output_interface_id = TSMF_INTERFACE_DEFAULT | STREAM_ID_STUB;

	return 0;
}

int tsmf_ifman_on_new_presentation(TSMF_IFMAN* ifman)
{
	int status = 0;
	TSMF_PRESENTATION* presentation;

	DEBUG_DVC("");

	presentation = tsmf_presentation_find_by_id(Stream_Pointer(ifman->input));
	if (presentation)
	{
		DEBUG_DVC("Presentation already exists");
		ifman->output_pending = FALSE;
		return 0;

	}

	presentation = tsmf_presentation_new(Stream_Pointer(ifman->input), ifman->channel_callback);

	if (presentation == NULL)
		status = 1;
	else
		tsmf_presentation_set_audio_device(presentation, ifman->audio_name, ifman->audio_device);

	ifman->output_pending = TRUE;

	return status;
}

int tsmf_ifman_add_stream(TSMF_IFMAN* ifman)
{
	UINT32 StreamId;
	int status = 0;
	TSMF_STREAM* stream;
	TSMF_PRESENTATION* presentation;

	DEBUG_DVC("");

	presentation = tsmf_presentation_find_by_id(Stream_Pointer(ifman->input));
	Stream_Seek(ifman->input, 16);

	if (presentation == NULL)
	{
		status = 1;
	}
	else
	{
		Stream_Read_UINT32(ifman->input, StreamId);
		Stream_Seek_UINT32(ifman->input); /* numMediaType */
		stream = tsmf_stream_new(presentation, StreamId);

		if (stream)
			tsmf_stream_set_format(stream, ifman->decoder_name, ifman->input);
	}

	ifman->output_pending = TRUE;

	return status;
}

int tsmf_ifman_set_topology_request(TSMF_IFMAN* ifman)
{
	DEBUG_DVC("");

	Stream_EnsureRemainingCapacity(ifman->output, 8);
	Stream_Write_UINT32(ifman->output, 1); /* TopologyReady */
	Stream_Write_UINT32(ifman->output, 0); /* Result */
	ifman->output_interface_id = TSMF_INTERFACE_DEFAULT | STREAM_ID_STUB;

	return 0;
}

int tsmf_ifman_remove_stream(TSMF_IFMAN* ifman)
{
	int status = 0;
	UINT32 StreamId;
	TSMF_STREAM* stream;
	TSMF_PRESENTATION* presentation;

	DEBUG_DVC("");

	presentation = tsmf_presentation_find_by_id(Stream_Pointer(ifman->input));
	Stream_Seek(ifman->input, 16);

	if (presentation == NULL)
	{
		status = 1;
	}
	else
	{
		Stream_Read_UINT32(ifman->input, StreamId);
		stream = tsmf_stream_find_by_id(presentation, StreamId);
		if (stream)
			tsmf_stream_free(stream);
		else
			status = 1;
	}

	ifman->output_pending = TRUE;

	return status;
}

float tsmf_stream_read_float(wStream* s)
{
	float fValue;
	UINT32 iValue;

	Stream_Read_UINT32(s, iValue);
	CopyMemory(&fValue, &iValue, 4);

	return fValue;
}

int tsmf_ifman_set_source_video_rect(TSMF_IFMAN* ifman)
{
	int status = 0;
	float Left, Top;
	float Right, Bottom;
	TSMF_PRESENTATION* presentation;

	DEBUG_DVC("");

	presentation = tsmf_presentation_find_by_id(Stream_Pointer(ifman->input));
	Stream_Seek(ifman->input, 16);

	if (!presentation)
	{
		status = 1;
	}
	else
	{
		Left = tsmf_stream_read_float(ifman->input); /* Left (4 bytes) */
		Top = tsmf_stream_read_float(ifman->input); /* Top (4 bytes) */
		Right = tsmf_stream_read_float(ifman->input); /* Right (4 bytes) */
		Bottom = tsmf_stream_read_float(ifman->input); /* Bottom (4 bytes) */

		DEBUG_DVC("SetSourceVideoRect: Left: %f Top: %f Right: %f Bottom: %f",
				Left, Top, Right, Bottom);
	}

	ifman->output_pending = TRUE;

	return status;
}

int tsmf_ifman_shutdown_presentation(TSMF_IFMAN* ifman)
{
	TSMF_PRESENTATION* presentation;

	DEBUG_DVC("");

	presentation = tsmf_presentation_find_by_id(Stream_Pointer(ifman->input));

	if (presentation)
		tsmf_presentation_free(presentation);
	else
		DEBUG_WARN("unknown presentation id");

	Stream_EnsureRemainingCapacity(ifman->output, 4);
	Stream_Write_UINT32(ifman->output, 0); /* Result */
	ifman->output_interface_id = TSMF_INTERFACE_DEFAULT | STREAM_ID_STUB;

	return 0;
}

int tsmf_ifman_on_stream_volume(TSMF_IFMAN* ifman)
{
	TSMF_PRESENTATION* presentation;

	DEBUG_DVC("on stream volume");

	presentation = tsmf_presentation_find_by_id(Stream_Pointer(ifman->input));

	if (presentation)
	{
		UINT32 newVolume;
		UINT32 muted;

		Stream_Seek(ifman->input, 16);
		Stream_Read_UINT32(ifman->input, newVolume);
		DEBUG_DVC("on stream volume: new volume=[%d]", newVolume);
		Stream_Read_UINT32(ifman->input, muted);
		DEBUG_DVC("on stream volume: muted=[%d]", muted);
		tsmf_presentation_volume_changed(presentation, newVolume, muted);
	}
	else
	{
		DEBUG_WARN("unknown presentation id");
	}

	ifman->output_pending = TRUE;

	return 0;
}

int tsmf_ifman_on_channel_volume(TSMF_IFMAN* ifman)
{
	TSMF_PRESENTATION* presentation;

	DEBUG_DVC("on channel volume");
	
	presentation = tsmf_presentation_find_by_id(Stream_Pointer(ifman->input));

	if (presentation)
	{
		UINT32 channelVolume;
		UINT32 changedChannel;

		Stream_Seek(ifman->input, 16);
		Stream_Read_UINT32(ifman->input, channelVolume);
		DEBUG_DVC("on channel volume: channel volume=[%d]", channelVolume);
		Stream_Read_UINT32(ifman->input, changedChannel);
		DEBUG_DVC("on stream volume: changed channel=[%d]", changedChannel);
	}
	
	ifman->output_pending = TRUE;
	
	return 0;
}

int tsmf_ifman_set_video_window(TSMF_IFMAN* ifman)
{
	DEBUG_DVC("");
	ifman->output_pending = TRUE;
	return 0;
}

int tsmf_ifman_update_geometry_info(TSMF_IFMAN* ifman)
{
	TSMF_PRESENTATION* presentation;
	UINT32 numGeometryInfo;
	UINT32 Left;
	UINT32 Top;
	UINT32 Width;
	UINT32 Height;
	UINT32 cbVisibleRect;
	RDP_RECT* rects = NULL;
	int num_rects = 0;
	int error = 0;
	int i;
	int pos;

	presentation = tsmf_presentation_find_by_id(Stream_Pointer(ifman->input));
	Stream_Seek(ifman->input, 16);

	Stream_Read_UINT32(ifman->input, numGeometryInfo);
	pos = Stream_GetPosition(ifman->input);

	Stream_Seek(ifman->input, 12); /* VideoWindowId (8 bytes), VideoWindowState (4 bytes) */
	Stream_Read_UINT32(ifman->input, Width);
	Stream_Read_UINT32(ifman->input, Height);
	Stream_Read_UINT32(ifman->input, Left);
	Stream_Read_UINT32(ifman->input, Top);

	Stream_SetPosition(ifman->input, pos + numGeometryInfo);
	Stream_Read_UINT32(ifman->input, cbVisibleRect);
	num_rects = cbVisibleRect / 16;

	DEBUG_DVC("numGeometryInfo %d Width %d Height %d Left %d Top %d cbVisibleRect %d num_rects %d",
		numGeometryInfo, Width, Height, Left, Top, cbVisibleRect, num_rects);

	if (presentation == NULL)
	{
		error = 1;
	}
	else
	{
		if (num_rects > 0)
		{
			rects = (RDP_RECT*) malloc(sizeof(RDP_RECT) * num_rects);
			ZeroMemory(rects, sizeof(RDP_RECT) * num_rects);

			for (i = 0; i < num_rects; i++)
			{
				Stream_Read_UINT16(ifman->input, rects[i].y); /* Top */
				Stream_Seek_UINT16(ifman->input);
				Stream_Read_UINT16(ifman->input, rects[i].x); /* Left */
				Stream_Seek_UINT16(ifman->input);
				Stream_Read_UINT16(ifman->input, rects[i].height); /* Bottom */
				Stream_Seek_UINT16(ifman->input);
				Stream_Read_UINT16(ifman->input, rects[i].width); /* Right */
				Stream_Seek_UINT16(ifman->input);
				rects[i].width -= rects[i].x;
				rects[i].height -= rects[i].y;

				DEBUG_DVC("rect %d: %d %d %d %d", i,
					rects[i].x, rects[i].y, rects[i].width, rects[i].height);
			}
		}
		tsmf_presentation_set_geometry_info(presentation, Left, Top, Width, Height, num_rects, rects);
	}
	
	ifman->output_pending = TRUE;

	return error;
}

int tsmf_ifman_set_allocator(TSMF_IFMAN* ifman)
{
	DEBUG_DVC("");
	ifman->output_pending = TRUE;
	return 0;
}

int tsmf_ifman_notify_preroll(TSMF_IFMAN* ifman)
{
	DEBUG_DVC("");
	ifman->output_pending = TRUE;
	return 0;
}

int tsmf_ifman_on_sample(TSMF_IFMAN* ifman)
{
	TSMF_PRESENTATION* presentation;
	TSMF_STREAM* stream;
	UINT32 StreamId;
	UINT64 SampleStartTime;
	UINT64 SampleEndTime;
	UINT64 ThrottleDuration;
	UINT32 SampleExtensions;
	UINT32 cbData;

	Stream_Seek(ifman->input, 16);
	Stream_Read_UINT32(ifman->input, StreamId);
	Stream_Seek_UINT32(ifman->input); /* numSample */
	Stream_Read_UINT64(ifman->input, SampleStartTime);
	Stream_Read_UINT64(ifman->input, SampleEndTime);
	Stream_Read_UINT64(ifman->input, ThrottleDuration);
	Stream_Seek_UINT32(ifman->input); /* SampleFlags */
	Stream_Read_UINT32(ifman->input, SampleExtensions);
	Stream_Read_UINT32(ifman->input, cbData);
	
	DEBUG_DVC("MessageId %d StreamId %d SampleStartTime %d SampleEndTime %d "
		"ThrottleDuration %d SampleExtensions %d cbData %d",
		ifman->message_id, StreamId, (int)SampleStartTime, (int)SampleEndTime,
		(int)ThrottleDuration, SampleExtensions, cbData);

	presentation = tsmf_presentation_find_by_id(ifman->presentation_id);

	if (presentation == NULL)
	{
		DEBUG_WARN("unknown presentation id");
		return 1;
	}

	stream = tsmf_stream_find_by_id(presentation, StreamId);

	if (stream == NULL)
	{
		DEBUG_WARN("unknown stream id");
		return 1;
	}

	tsmf_stream_push_sample(stream, ifman->channel_callback,
		ifman->message_id, SampleStartTime, SampleEndTime, ThrottleDuration, SampleExtensions,
		cbData, Stream_Pointer(ifman->input));

	ifman->output_pending = TRUE;

	return 0;
}

int tsmf_ifman_on_flush(TSMF_IFMAN* ifman)
{
	UINT32 StreamId;
	TSMF_PRESENTATION* presentation;

	Stream_Seek(ifman->input, 16);
	Stream_Read_UINT32(ifman->input, StreamId);
	DEBUG_DVC("StreamId %d", StreamId);

	presentation = tsmf_presentation_find_by_id(ifman->presentation_id);

	if (presentation == NULL)
	{
		DEBUG_WARN("unknown presentation id");
		return 1;
	}

	tsmf_presentation_flush(presentation);

	ifman->output_pending = TRUE;

	return 0;
}

int tsmf_ifman_on_end_of_stream(TSMF_IFMAN* ifman)
{
	UINT32 StreamId;
	TSMF_STREAM* stream;
	TSMF_PRESENTATION* presentation;

	presentation = tsmf_presentation_find_by_id(Stream_Pointer(ifman->input));
	Stream_Seek(ifman->input, 16);
	Stream_Read_UINT32(ifman->input, StreamId);

	if (presentation)
	{
		stream = tsmf_stream_find_by_id(presentation, StreamId);
		if (stream)
			tsmf_stream_end(stream);
	}
	DEBUG_DVC("StreamId %d", StreamId);

	Stream_EnsureRemainingCapacity(ifman->output, 16);
	Stream_Write_UINT32(ifman->output, CLIENT_EVENT_NOTIFICATION); /* FunctionId */
	Stream_Write_UINT32(ifman->output, StreamId); /* StreamId */
	Stream_Write_UINT32(ifman->output, TSMM_CLIENT_EVENT_ENDOFSTREAM); /* EventId */
	Stream_Write_UINT32(ifman->output, 0); /* cbData */
	ifman->output_interface_id = TSMF_INTERFACE_CLIENT_NOTIFICATIONS | STREAM_ID_PROXY;

	return 0;
}

int tsmf_ifman_on_playback_started(TSMF_IFMAN* ifman)
{
	TSMF_PRESENTATION* presentation;

	DEBUG_DVC("");

	presentation = tsmf_presentation_find_by_id(Stream_Pointer(ifman->input));

	if (presentation)
		tsmf_presentation_start(presentation);
	else
		DEBUG_WARN("unknown presentation id");

	Stream_EnsureRemainingCapacity(ifman->output, 16);
	Stream_Write_UINT32(ifman->output, CLIENT_EVENT_NOTIFICATION); /* FunctionId */
	Stream_Write_UINT32(ifman->output, 0); /* StreamId */
	Stream_Write_UINT32(ifman->output, TSMM_CLIENT_EVENT_START_COMPLETED); /* EventId */
	Stream_Write_UINT32(ifman->output, 0); /* cbData */
	ifman->output_interface_id = TSMF_INTERFACE_CLIENT_NOTIFICATIONS | STREAM_ID_PROXY;

	return 0;
}

int tsmf_ifman_on_playback_paused(TSMF_IFMAN* ifman)
{
	TSMF_PRESENTATION* presentation;

	DEBUG_DVC("");
	ifman->output_pending = TRUE;

	/* Added pause control so gstreamer pipeline can be paused accordingly */

	presentation = tsmf_presentation_find_by_id(Stream_Pointer(ifman->input));

	if (presentation)
		tsmf_presentation_paused(presentation);
	else
		DEBUG_WARN("unknown presentation id");

	return 0;
}

int tsmf_ifman_on_playback_restarted(TSMF_IFMAN* ifman)
{
	TSMF_PRESENTATION* presentation;

	DEBUG_DVC("");
	ifman->output_pending = TRUE;

	/* Added restart control so gstreamer pipeline can be resumed accordingly */

	presentation = tsmf_presentation_find_by_id(Stream_Pointer(ifman->input));

	if (presentation)
		tsmf_presentation_restarted(presentation);
	else
		DEBUG_WARN("unknown presentation id");
	
	return 0;
}

int tsmf_ifman_on_playback_stopped(TSMF_IFMAN* ifman)
{
	TSMF_PRESENTATION* presentation;

	DEBUG_DVC("");

	presentation = tsmf_presentation_find_by_id(Stream_Pointer(ifman->input));

	if (presentation)
		tsmf_presentation_stop(presentation);
	else
		DEBUG_WARN("unknown presentation id");

	Stream_EnsureRemainingCapacity(ifman->output, 16);
	Stream_Write_UINT32(ifman->output, CLIENT_EVENT_NOTIFICATION); /* FunctionId */
	Stream_Write_UINT32(ifman->output, 0); /* StreamId */
	Stream_Write_UINT32(ifman->output, TSMM_CLIENT_EVENT_STOP_COMPLETED); /* EventId */
	Stream_Write_UINT32(ifman->output, 0); /* cbData */
	ifman->output_interface_id = TSMF_INTERFACE_CLIENT_NOTIFICATIONS | STREAM_ID_PROXY;

	return 0;
}

int tsmf_ifman_on_playback_rate_changed(TSMF_IFMAN * ifman)
{
	DEBUG_DVC("");

	Stream_EnsureRemainingCapacity(ifman->output, 16);
	Stream_Write_UINT32(ifman->output, CLIENT_EVENT_NOTIFICATION); /* FunctionId */
	Stream_Write_UINT32(ifman->output, 0); /* StreamId */
	Stream_Write_UINT32(ifman->output, TSMM_CLIENT_EVENT_MONITORCHANGED); /* EventId */
	Stream_Write_UINT32(ifman->output, 0); /* cbData */
	ifman->output_interface_id = TSMF_INTERFACE_CLIENT_NOTIFICATIONS | STREAM_ID_PROXY;

	return 0;
}

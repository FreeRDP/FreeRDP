/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2026 Cong Zhang
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

#ifndef FREERDP_SERVER_SHADOW_MAC_CAPTURE_H
#define FREERDP_SERVER_SHADOW_MAC_CAPTURE_H

#include <winpr/wtypes.h>

#include <CoreGraphics/CoreGraphics.h>
#include <CoreVideo/CoreVideo.h>

/* The screen as ScreenCaptureKit gives it: a C interface, so that the rest of
 * the subsystem stays plain C. */
typedef struct mac_capture macCapture;

/* Called on the capture queue for every frame that changed something. frame
 * is a 32-bit BGRA buffer of the size asked for; the dirty rectangles are in
 * its pixels. The frame is only valid during the call: CFRetain it to keep it. */
typedef void (*macCaptureFrameFn)(void *context, CVPixelBufferRef frame, const CGRect *dirty,
                                  size_t count);

/* Called on the capture queue with interleaved 32-bit float samples. */
typedef void (*macCaptureAudioFn)(void *context, const float *samples, size_t frames,
                                  UINT32 channels, UINT32 rate);

/* The stream stopped by itself (the display went away, permission revoked). */
typedef void (*macCaptureStoppedFn)(void *context);

typedef struct
{
	CGDirectDisplayID display;
	size_t width;  /* of the frames, in pixels */
	size_t height; /* of the frames, in pixels */
	UINT32 fps;
	BOOL showCursor;
	BOOL audio;       /* capture the sound the computer plays (macOS 13) */
	UINT32 audioRate; /* e.g. 48000 */
	UINT32 audioChannels;
	macCaptureFrameFn onFrame;
	macCaptureAudioFn onAudio;
	macCaptureStoppedFn onStopped;
	void *context;
} macCaptureConfig;

#ifdef __cplusplus
extern "C"
{
#endif

	/* Screen Recording permission: TRUE when granted; when not, asks for it
	 * once (the system shows its own prompt) and returns FALSE. */
	BOOL mac_capture_check_access(void);

	macCapture *mac_capture_new(const macCaptureConfig *config);
	void mac_capture_free(macCapture *capture);

	BOOL mac_capture_start(macCapture *capture);
	void mac_capture_stop(macCapture *capture);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SERVER_SHADOW_MAC_CAPTURE_H */

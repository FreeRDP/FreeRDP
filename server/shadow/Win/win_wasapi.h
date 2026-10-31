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

#ifndef FREERDP_SERVER_SHADOW_WIN_WASAPI_H
#define FREERDP_SERVER_SHADOW_WIN_WASAPI_H

/* WASAPI loopback capture, kept apart from FreeRDP's audio headers: the
 * Windows ones (mmreg.h) define macros that clash with them */

#include <winpr/wtypes.h>
#include <winpr/stream.h>

/* What the capture delivers: 16-bit stereo at 48 kHz */
#define WIN_WASAPI_RATE 48000
#define WIN_WASAPI_CHANNELS 2
#define WIN_WASAPI_FRAME_BYTES 4

typedef struct win_wasapi_loopback winWasapiLoopback;

#ifdef __cplusplus
extern "C"
{
#endif

	/* Starts loopback capture on the default render endpoint: what the
	 * speakers play. The calling thread has COM initialized. log is FALSE
	 * while retrying after a failure that was reported. */
	winWasapiLoopback* win_wasapi_loopback_open(BOOL log);
	void win_wasapi_loopback_close(winWasapiLoopback* loopback);

	/* Appends what was captured since the last call to s. FALSE when the
	 * capture stopped: the endpoint went away (unplugged, the default
	 * changed, the audio service stopped) or memory ran out. */
	BOOL win_wasapi_loopback_read(winWasapiLoopback* loopback, wStream* s);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SERVER_SHADOW_WIN_WASAPI_H */

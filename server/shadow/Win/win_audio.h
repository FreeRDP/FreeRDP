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

#ifndef FREERDP_SERVER_SHADOW_WIN_AUDIO_H
#define FREERDP_SERVER_SHADOW_WIN_AUDIO_H

typedef struct win_shadow_subsystem winShadowSubsystem;

#ifdef __cplusplus
extern "C"
{
#endif

	/* The sound the computer plays, captured with WASAPI loopback on the
	 * default render endpoint and sent to the clients' rdpsnd channels */
	BOOL win_shadow_audio_init(winShadowSubsystem* subsystem);
	void win_shadow_audio_uninit(winShadowSubsystem* subsystem);
	BOOL win_shadow_audio_start(winShadowSubsystem* subsystem);
	void win_shadow_audio_stop(winShadowSubsystem* subsystem);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SERVER_SHADOW_WIN_AUDIO_H */

/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_AUDIOTRACK_WRAPPER_H
#define ANDROID_AUDIOTRACK_WRAPPER_H

#include <stdint.h>
#include <jni.h>

#define ANDROID_AUDIOTRACK_RESULT_SUCCESS				 0
#define ANDROID_AUDIOTRACK_RESULT_BAD_PARAMETER			-1
#define ANDROID_AUDIOTRACK_RESULT_JNI_EXCEPTION			-2
#define ANDROID_AUDIOTRACK_RESULT_ALLOCATION_FAILED		-3
#define ANDROID_AUDIOTRACK_RESULT_ERRNO					-4

enum stream_type {
	DEFAULT          =-1,
	VOICE_CALL       = 0,
	SYSTEM           = 1,
	RING             = 2,
	MUSIC            = 3,
	ALARM            = 4,
	NOTIFICATION     = 5,
	BLUETOOTH_SCO    = 6,
	ENFORCED_AUDIBLE = 7, // Sounds that cannot be muted by user and must be routed to speaker
	DTMF             = 8,
	TTS              = 9,
	NUM_STREAM_TYPES
};

enum {
	NO_MORE_BUFFERS = 0x80000001,
	STOPPED = 1
};

// Audio sub formats (see AudioSystem::audio_format).
enum pcm_sub_format {
	PCM_SUB_16_BIT          = 0x1, // must be 1 for backward compatibility
	PCM_SUB_8_BIT           = 0x2, // must be 2 for backward compatibility
};

// Audio format consists in a main format field (upper 8 bits) and a sub format field (lower 24 bits).
// The main format indicates the main codec type. The sub format field indicates options and parameters
// for each format. The sub format is mainly used for record to indicate for instance the requested bitrate
// or profile. It can also be used for certain formats to give informations not present in the encoded
// audio stream (e.g. octet alignement for AMR).
enum audio_format {
	INVALID_FORMAT      = -1,
	FORMAT_DEFAULT      = 0,
	PCM                 = 0x00000000, // must be 0 for backward compatibility
	MP3                 = 0x01000000,
	AMR_NB              = 0x02000000,
	AMR_WB              = 0x03000000,
	AAC                 = 0x04000000,
	HE_AAC_V1           = 0x05000000,
	HE_AAC_V2           = 0x06000000,
	VORBIS              = 0x07000000,
	MAIN_FORMAT_MASK    = 0xFF000000,
	SUB_FORMAT_MASK     = 0x00FFFFFF,
	// Aliases
	PCM_16_BIT          = (PCM|PCM_SUB_16_BIT),
	PCM_8_BIT          = (PCM|PCM_SUB_8_BIT)
};

// Channel mask definitions must be kept in sync with JAVA values in /media/java/android/media/AudioFormat.java
enum audio_channels {
	// output channels
	CHANNEL_OUT_FRONT_LEFT = 0x4,
	CHANNEL_OUT_FRONT_RIGHT = 0x8,
	CHANNEL_OUT_MONO = CHANNEL_OUT_FRONT_LEFT,
	CHANNEL_OUT_STEREO = (CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT)
};

#ifdef __cplusplus
extern "C" {
#endif

typedef void* AUDIO_DRIVER_HANDLE;

int freerdp_android_at_init_library();
int freerdp_android_at_open(AUDIO_DRIVER_HANDLE* outHandle);
int freerdp_android_at_set(AUDIO_DRIVER_HANDLE handle, int streamType, uint32_t sampleRate, int format, int channels);
int freerdp_android_at_set_volume(AUDIO_DRIVER_HANDLE handle, float left, float right);
int freerdp_android_at_close(AUDIO_DRIVER_HANDLE handle);
uint32_t freerdp_android_at_latency(AUDIO_DRIVER_HANDLE handle);
int freerdp_android_at_start(AUDIO_DRIVER_HANDLE handle);
int freerdp_android_at_write(AUDIO_DRIVER_HANDLE handle, void *buffer, int buffer_size);
int freerdp_android_at_flush(AUDIO_DRIVER_HANDLE handle);
int freerdp_android_at_stop(AUDIO_DRIVER_HANDLE handle);
int freerdp_android_at_reload(AUDIO_DRIVER_HANDLE handle);

#ifdef __cplusplus
}
#endif

#endif

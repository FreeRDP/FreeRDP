/*
opensl_io.c:
Android OpenSL input/output module header
Copyright (c) 2012, Victor Lazzarini
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef FREERDP_CHANNEL_RDPSND_CLIENT_OPENSL_IO_H
#define FREERDP_CHANNEL_RDPSND_CLIENT_OPENSL_IO_H

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <stdlib.h>
#include <winpr/synch.h>

#include <freerdp/api.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct opensl_stream
	{
		// engine interfaces
		SLObjectItf engineObject;
		SLEngineItf engineEngine;

		// output mix interfaces
		SLObjectItf outputMixObject;

		// buffer queue player interfaces
		SLObjectItf bqPlayerObject;
		SLPlayItf bqPlayerPlay;
		SLVolumeItf bqPlayerVolume;
		SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
		SLEffectSendItf bqPlayerEffectSend;

		unsigned int outchannels;
		unsigned int sr;

		unsigned int queuesize;
		wQueue* queue;
	} OPENSL_STREAM;

	/*
	Open the audio device with a given sampling rate (sr), output channels and IO buffer size
	in frames. Returns a handle to the OpenSL stream
	*/
	FREERDP_LOCAL OPENSL_STREAM* android_OpenAudioDevice(int sr, int outchannels, int bufferframes);
	/*
	Close the audio device
	*/
	FREERDP_LOCAL void android_CloseAudioDevice(OPENSL_STREAM* p);
	/*
	Write a buffer to the OpenSL stream *p, of size samples. Returns the number of samples written.
	*/
	FREERDP_LOCAL int android_AudioOut(OPENSL_STREAM* p, const short* buffer, int size);
	/*
	 * Set the volume input level.
	 */
	FREERDP_LOCAL void android_SetInputVolume(OPENSL_STREAM* p, int level);
	/*
	 * Get the current output mute setting.
	 */
	FREERDP_LOCAL int android_GetOutputMute(OPENSL_STREAM* p);
	/*
	 * Change the current output mute setting.
	 */
	FREERDP_LOCAL BOOL android_SetOutputMute(OPENSL_STREAM* p, BOOL mute);
	/*
	 * Get the current output volume level.
	 */
	FREERDP_LOCAL int android_GetOutputVolume(OPENSL_STREAM* p);
	/*
	 * Get the maximum output volume level.
	 */
	FREERDP_LOCAL int android_GetOutputVolumeMax(OPENSL_STREAM* p);

	/*
	 * Set the volume output level.
	 */
	FREERDP_LOCAL BOOL android_SetOutputVolume(OPENSL_STREAM* p, int level);
#ifdef __cplusplus
};
#endif

#endif /* FREERDP_CHANNEL_RDPSND_CLIENT_OPENSL_IO_H */

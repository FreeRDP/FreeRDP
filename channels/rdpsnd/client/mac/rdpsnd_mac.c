/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Output Virtual Channel
 *
 * Copyright 2012 Laxmikant Rashinkar <LK.Rashinkar@gmail.com>
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
 * Use AudioQueue to implement audio redirection
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>

#include <freerdp/types.h>
#include <freerdp/codec/dsp.h>
#include <freerdp/utils/svc_plugin.h>

#include <AudioToolbox/AudioToolbox.h>
#include <AudioToolbox/AudioQueue.h>

#include "rdpsnd_main.h"

#define AQ_NUM_BUFFERS  10
#define AQ_BUF_SIZE     (32 * 1024)

static void aq_playback_cb(void *user_data,
                           AudioQueueRef aq_ref,
                           AudioQueueBufferRef aq_buf_ref
                          );

struct rdpsnd_audio_q_plugin
{
	rdpsndDevicePlugin device;

	/* audio queue player state */
	int     is_open;        // true when audio_q has been inited
	char *  device_name;
	int     is_playing;
	int     buf_index;
    
	AudioStreamBasicDescription  data_format;
	AudioQueueRef                aq_ref;
	AudioQueueBufferRef          buffers[AQ_NUM_BUFFERS];
};
typedef struct rdpsnd_audio_q_plugin rdpsndAudioQPlugin;

static void rdpsnd_audio_close(rdpsndDevicePlugin* device)
{
	rdpsndAudioQPlugin* aq_plugin_p = (rdpsndAudioQPlugin*) device;
    
	AudioQueueStop(aq_plugin_p->aq_ref, 0);
	aq_plugin_p->is_open = 0;
}

static void rdpsnd_audio_open(rdpsndDevicePlugin* device, AUDIO_FORMAT* format, int latency)
{
	int rv;
	int i;
    
	rdpsndAudioQPlugin* aq_plugin_p = (rdpsndAudioQPlugin *) device;
	if (aq_plugin_p->is_open) {
		return;
	}
    
	aq_plugin_p->buf_index = 0;
    
	// setup AudioStreamBasicDescription
	aq_plugin_p->data_format.mSampleRate = 44100;
	aq_plugin_p->data_format.mFormatID = kAudioFormatLinearPCM;
	aq_plugin_p->data_format.mFormatFlags = kAudioFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    
	// until we know better, assume that one packet = one frame
	// one frame = bytes_per_sample x number_of_channels
	aq_plugin_p->data_format.mBytesPerPacket = 4;
	aq_plugin_p->data_format.mFramesPerPacket = 1;
	aq_plugin_p->data_format.mBytesPerFrame = 4;
	aq_plugin_p->data_format.mChannelsPerFrame = 2;
	aq_plugin_p->data_format.mBitsPerChannel = 16;
    
	rv = AudioQueueNewOutput(
                             &aq_plugin_p->data_format, // audio stream basic desc
                             aq_playback_cb,            // callback when more data is required
                             aq_plugin_p,               // data to pass to callback
                             CFRunLoopGetCurrent(),     // The current run loop, and the one on 
                             // which the audio queue playback callback
                             // will be invoked
                             kCFRunLoopCommonModes,     // run loop modes in which callbacks can
                             // be invoked
                             0,                         // flags - reserved
                             &aq_plugin_p->aq_ref
				);
	if (rv != 0) {
		fprintf(stderr, "rdpsnd_audio_open: AudioQueueNewOutput() failed with error %d\n", rv);
		aq_plugin_p->is_open = 1;
		return;
	}
    
	for (i = 0; i < AQ_NUM_BUFFERS; i++)
	{
		rv = AudioQueueAllocateBuffer(aq_plugin_p->aq_ref, AQ_BUF_SIZE, &aq_plugin_p->buffers[i]);
	}
    
	aq_plugin_p->is_open = 1;
}

static void rdpsnd_audio_free(rdpsndDevicePlugin* device)
{
}

static BOOL rdpsnd_audio_format_supported(rdpsndDevicePlugin* device, AUDIO_FORMAT* format)
{
	switch (format->wFormatTag)
	{
		case 1: /* PCM */
			if (format->cbSize == 0 &&
			   (format->nSamplesPerSec <= 48000) &&
			   (format->wBitsPerSample == 8 || format->wBitsPerSample == 16) &&
			   (format->nChannels == 1 || format->nChannels == 2))
		{
			return 1;
		}
		break;
	}
	return 0;
}

static void rdpsnd_audio_set_format(rdpsndDevicePlugin* device, AUDIO_FORMAT* format, int latency)
{
}

static void rdpsnd_audio_set_volume(rdpsndDevicePlugin* device, UINT32 value)
{
}

static void rdpsnd_audio_play(rdpsndDevicePlugin* device, BYTE* data, int size)
{
	rdpsndAudioQPlugin* aq_plugin_p = (rdpsndAudioQPlugin *) device;
	AudioQueueBufferRef aq_buf_ref;
	int                 len;
    
	if (!aq_plugin_p->is_open) {
		return;
	}

	/* get next empty buffer */
	aq_buf_ref = aq_plugin_p->buffers[aq_plugin_p->buf_index];
    
	// fill aq_buf_ref with audio data
	len = size > AQ_BUF_SIZE ? AQ_BUF_SIZE : size;
    
	memcpy(aq_buf_ref->mAudioData, (char *) data, len);
	aq_buf_ref->mAudioDataByteSize = len;
    
	// add buffer to audioqueue
	AudioQueueEnqueueBuffer(aq_plugin_p->aq_ref, aq_buf_ref, 0, 0);
    
	// update buf_index
	aq_plugin_p->buf_index++;
	if (aq_plugin_p->buf_index >= AQ_NUM_BUFFERS) {
		aq_plugin_p->buf_index = 0;
	}
}

static void rdpsnd_audio_start(rdpsndDevicePlugin* device)
{
	rdpsndAudioQPlugin* aq_plugin_p = (rdpsndAudioQPlugin *) device;

	AudioQueueStart(aq_plugin_p->aq_ref, NULL);
}

/**
 * AudioQueue Playback callback
 *
 * our job here is to fill aq_buf_ref with audio data and enqueue it
 */

static void aq_playback_cb(void* user_data, AudioQueueRef aq_ref, AudioQueueBufferRef aq_buf_ref)
{

}

#ifdef STATIC_CHANNELS
#define freerdp_rdpsnd_client_subsystem_entry	mac_freerdp_rdpsnd_client_subsystem_entry
#endif

int freerdp_rdpsnd_client_subsystem_entry(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints)
{
    fprintf(stderr, "freerdp_rdpsnd_client_subsystem_entry()\n\n");
    
	ADDIN_ARGV* args;
	rdpsndAudioQPlugin* aqPlugin;
    
	aqPlugin = (rdpsndAudioQPlugin*) malloc(sizeof(rdpsndAudioQPlugin));
	ZeroMemory(aqPlugin, sizeof(rdpsndAudioQPlugin));

	aqPlugin->device.Open = rdpsnd_audio_open;
	aqPlugin->device.FormatSupported = rdpsnd_audio_format_supported;
	aqPlugin->device.SetFormat = rdpsnd_audio_set_format;
	aqPlugin->device.SetVolume = rdpsnd_audio_set_volume;
	aqPlugin->device.Play = rdpsnd_audio_play;
	aqPlugin->device.Start = rdpsnd_audio_start;
	aqPlugin->device.Close = rdpsnd_audio_close;
	aqPlugin->device.Free = rdpsnd_audio_free;

	args = pEntryPoints->args;
    
	if (args->argc > 2)
	{
		/* TODO: parse device name */
	}

	pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, (rdpsndDevicePlugin*) aqPlugin);

	return 0;
}

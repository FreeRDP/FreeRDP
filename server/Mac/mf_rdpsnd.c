/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Mac OS X Server (Audio Output)
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/server/rdpsnd.h>

#include "mf_info.h"
#include "mf_rdpsnd.h"

#include <winpr/sysinfo.h>
#include <freerdp/log.h>
#define TAG SERVER_TAG("mac")

AQRecorderState recorderState;

static const AUDIO_FORMAT supported_audio_formats[] =
{
	{ WAVE_FORMAT_PCM, 2, 44100, 176400, 4, 16, 0, NULL },
	{ WAVE_FORMAT_ALAW, 2, 22050, 44100, 2, 8, 0, NULL }
};

static void mf_peer_rdpsnd_activated(RdpsndServerContext* context)
{
	OSStatus status;
	int i, j;
	BOOL formatAgreed = FALSE;
	AUDIO_FORMAT* agreedFormat = NULL;
	//we should actually loop through the list of client formats here
	//and see if we can send the client something that it supports...
	WLog_DBG(TAG, "Client supports the following %d formats: ", context->num_client_formats);

	for (i = 0; i < context->num_client_formats; i++)
	{
		/* TODO: improve the way we agree on a format */
		for (j = 0; j < context->num_server_formats; j++)
		{
			if ((context->client_formats[i].wFormatTag == context->server_formats[j].wFormatTag) &&
			    (context->client_formats[i].nChannels == context->server_formats[j].nChannels) &&
			    (context->client_formats[i].nSamplesPerSec == context->server_formats[j].nSamplesPerSec))
			{
				WLog_DBG(TAG, "agreed on format!");
				formatAgreed = TRUE;
				agreedFormat = (AUDIO_FORMAT*)&context->server_formats[j];
				break;
			}
		}

		if (formatAgreed == TRUE)
			break;
	}

	if (formatAgreed == FALSE)
	{
		WLog_DBG(TAG, "Could not agree on a audio format with the server");
		return;
	}

	context->SelectFormat(context, i);
	context->SetVolume(context, 0x7FFF, 0x7FFF);

	switch (agreedFormat->wFormatTag)
	{
		case WAVE_FORMAT_ALAW:
			recorderState.dataFormat.mFormatID = kAudioFormatDVIIntelIMA;
			break;

		case WAVE_FORMAT_PCM:
			recorderState.dataFormat.mFormatID = kAudioFormatLinearPCM;
			break;

		default:
			recorderState.dataFormat.mFormatID = kAudioFormatLinearPCM;
			break;
	}

	recorderState.dataFormat.mSampleRate = agreedFormat->nSamplesPerSec;
	recorderState.dataFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger |
	                                        kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;;
	recorderState.dataFormat.mBytesPerPacket = 4;
	recorderState.dataFormat.mFramesPerPacket = 1;
	recorderState.dataFormat.mBytesPerFrame = 4;
	recorderState.dataFormat.mChannelsPerFrame = agreedFormat->nChannels;
	recorderState.dataFormat.mBitsPerChannel = agreedFormat->wBitsPerSample;
	recorderState.snd_context = context;
	status = AudioQueueNewInput(&recorderState.dataFormat,
	                            mf_peer_rdpsnd_input_callback,
	                            &recorderState,
	                            NULL,
	                            kCFRunLoopCommonModes,
	                            0,
	                            &recorderState.queue);

	if (status != noErr)
	{
		WLog_DBG(TAG, "Failed to create a new Audio Queue. Status code: %"PRId32"", status);
	}

	UInt32 dataFormatSize = sizeof(recorderState.dataFormat);
	AudioQueueGetProperty(recorderState.queue,
	                      kAudioConverterCurrentInputStreamDescription,
	                      &recorderState.dataFormat,
	                      &dataFormatSize);
	mf_rdpsnd_derive_buffer_size(recorderState.queue, &recorderState.dataFormat, 0.05,
	                             &recorderState.bufferByteSize);

	for (i = 0; i < SND_NUMBUFFERS; ++i)
	{
		AudioQueueAllocateBuffer(recorderState.queue,
		                         recorderState.bufferByteSize,
		                         &recorderState.buffers[i]);
		AudioQueueEnqueueBuffer(recorderState.queue,
		                        recorderState.buffers[i],
		                        0,
		                        NULL);
	}

	recorderState.currentPacket = 0;
	recorderState.isRunning = true;
	AudioQueueStart(recorderState.queue, NULL);
}

BOOL mf_peer_rdpsnd_init(mfPeerContext* context)
{
	context->rdpsnd = rdpsnd_server_context_new(context->vcm);
	context->rdpsnd->rdpcontext = &context->_p;
	context->rdpsnd->data = context;
	context->rdpsnd->server_formats = supported_audio_formats;
	context->rdpsnd->num_server_formats = sizeof(supported_audio_formats) / sizeof(
	        supported_audio_formats[0]);
	context->rdpsnd->src_format.wFormatTag = 1;
	context->rdpsnd->src_format.nChannels = 2;
	context->rdpsnd->src_format.nSamplesPerSec = 44100;
	context->rdpsnd->src_format.wBitsPerSample = 16;
	context->rdpsnd->Activated = mf_peer_rdpsnd_activated;
	context->rdpsnd->Initialize(context->rdpsnd, TRUE);
	return TRUE;
}

BOOL mf_peer_rdpsnd_stop()
{
	recorderState.isRunning = false;
	AudioQueueStop(recorderState.queue, true);
	return TRUE;
}

void mf_peer_rdpsnd_input_callback(void*                                inUserData,
                                   AudioQueueRef                       inAQ,
                                   AudioQueueBufferRef                 inBuffer,
                                   const AudioTimeStamp*                inStartTime,
                                   UInt32                              inNumberPacketDescriptions,
                                   const AudioStreamPacketDescription*  inPacketDescs)
{
	OSStatus status;
	AQRecorderState* rState;
	rState = inUserData;

	if (inNumberPacketDescriptions == 0 && rState->dataFormat.mBytesPerPacket != 0)
	{
		inNumberPacketDescriptions = inBuffer->mAudioDataByteSize / rState->dataFormat.mBytesPerPacket;
	}

	if (rState->isRunning == 0)
	{
		return ;
	}

	rState->snd_context->SendSamples(rState->snd_context, inBuffer->mAudioData,
	                                 inBuffer->mAudioDataByteSize / 4, (UINT16)(GetTickCount() & 0xffff));
	status = AudioQueueEnqueueBuffer(
	             rState->queue,
	             inBuffer,
	             0,
	             NULL);

	if (status != noErr)
	{
		WLog_DBG(TAG, "AudioQueueEnqueueBuffer() returned status = %"PRId32"", status);
	}
}

void mf_rdpsnd_derive_buffer_size(AudioQueueRef                audioQueue,
                                  AudioStreamBasicDescription*  ASBDescription,
                                  Float64                      seconds,
                                  UInt32*                       outBufferSize)
{
	static const int maxBufferSize = 0x50000;
	int maxPacketSize = ASBDescription->mBytesPerPacket;

	if (maxPacketSize == 0)
	{
		UInt32 maxVBRPacketSize = sizeof(maxPacketSize);
		AudioQueueGetProperty(audioQueue,
		                      kAudioQueueProperty_MaximumOutputPacketSize,
		                      // in Mac OS X v10.5, instead use
		                      //   kAudioConverterPropertyMaximumOutputPacketSize
		                      &maxPacketSize,
		                      &maxVBRPacketSize
		                     );
	}

	Float64 numBytesForTime =
	    ASBDescription->mSampleRate * maxPacketSize * seconds;
	*outBufferSize = (UInt32)(numBytesForTime < maxBufferSize ? numBytesForTime : maxBufferSize);
}


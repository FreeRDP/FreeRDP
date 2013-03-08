/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Mac OS X Server (Audio Output)
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

AQRecorderState recorderState;

static const AUDIO_FORMAT audio_formats[] =
{
	{ 0x11, 2, 22050, 1024, 4, 0, NULL }, /* IMA ADPCM, 22050 Hz, 2 channels */
	{ 0x11, 1, 22050, 512, 4, 0, NULL }, /* IMA ADPCM, 22050 Hz, 1 channels */
	{ 0x01, 2, 22050, 4, 16, 0, NULL }, /* PCM, 22050 Hz, 2 channels, 16 bits */
	{ 0x01, 1, 22050, 2, 16, 0, NULL }, /* PCM, 22050 Hz, 1 channels, 16 bits */
	{ 0x01, 2, 44100, 4, 16, 0, NULL }, /* PCM, 44100 Hz, 2 channels, 16 bits */
	{ 0x01, 1, 44100, 2, 16, 0, NULL }, /* PCM, 44100 Hz, 1 channels, 16 bits */
	{ 0x01, 2, 11025, 4, 16, 0, NULL }, /* PCM, 11025 Hz, 2 channels, 16 bits */
	{ 0x01, 1, 11025, 2, 16, 0, NULL }, /* PCM, 11025 Hz, 1 channels, 16 bits */
	{ 0x01, 2, 8000, 4, 16, 0, NULL }, /* PCM, 8000 Hz, 2 channels, 16 bits */
	{ 0x01, 1, 8000, 2, 16, 0, NULL } /* PCM, 8000 Hz, 1 channels, 16 bits */
};

/*
 UINT16 wFormatTag;
 UINT16 nChannels;
 UINT32 nSamplesPerSec;
 UINT32 nAvgBytesPerSec;
 UINT16 nBlockAlign;
 UINT16 wBitsPerSample;
 UINT16 cbSize;
 BYTE* data;
 */
static const AUDIO_FORMAT supported_audio_formats[] =
{
	
	{ WAVE_FORMAT_PCM, 2, 44100, 176400, 4, 16, NULL },
	{ WAVE_FORMAT_ALAW, 2, 22050, 44100, 2, 8, NULL }
};


static void mf_peer_rdpsnd_activated(rdpsnd_server_context* context)
{
	OSStatus status;
	int i, j;
	BOOL formatAgreed = FALSE;
	AUDIO_FORMAT* agreedFormat = NULL;
	//we should actually loop through the list of client formats here
	//and see if we can send the client something that it supports...
	
	printf("Client supports the following %d formats: \n", context->num_client_formats);
	for(i = 0; i < context->num_client_formats; i++)
	{
		printf("\n%d)\nTag: %#0X\nChannels: %d\nSamples per sec: %d\nAvg bytes per sec: %d\nBlockAlign: %d\nBits per sample: %d\ncbSize: %0X\n",
		       i,
		       context->client_formats[i].wFormatTag,
		       context->client_formats[i].nChannels,
		       context->client_formats[i].nSamplesPerSec,
		       context->client_formats[i].nAvgBytesPerSec,
		       context->client_formats[i].nBlockAlign,
		       context->client_formats[i].wBitsPerSample,
		       context->client_formats[i].cbSize);
		
		for (j = 0; j < context->num_server_formats; j++)
		{
			if ((context->client_formats[i].wFormatTag == context->server_formats[j].wFormatTag) &&
			    (context->client_formats[i].nChannels == context->server_formats[j].nChannels) &&
			    (context->client_formats[i].nSamplesPerSec == context->server_formats[j].nSamplesPerSec))
			{
				printf("agreed on format!\n");
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
		printf("Could not agree on a audio format with the server\n");
		return;
	}
	
	context->SelectFormat(context, i);
	context->SetVolume(context, 0x7FFF, 0x7FFF);
	
	switch (agreedFormat->wFormatTag)
	{
		case WAVE_FORMAT_ALAW:
			recorderState.dataFormat.mFormatID = kAudioFormatDVIIntelIMA;
			recorderState.dataFormat.mBitsPerChannel = 16;

			break;
			
		case WAVE_FORMAT_PCM:
			recorderState.dataFormat.mFormatID = kAudioFormatLinearPCM;
			recorderState.dataFormat.mBitsPerChannel = 4;
			break;
			
		default:
			recorderState.dataFormat.mFormatID = kAudioFormatLinearPCM;
			break;
	}
	
	recorderState.dataFormat.mSampleRate = 22050.0;
	recorderState.dataFormat.mFormatID = kAudioFormatALaw;
	recorderState.dataFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;;
	recorderState.dataFormat.mBytesPerPacket = 4;
	recorderState.dataFormat.mFramesPerPacket = 1;
	recorderState.dataFormat.mBytesPerFrame = 4;
	recorderState.dataFormat.mChannelsPerFrame = 2;
	recorderState.dataFormat.mBitsPerChannel = 8;
	
	
	/*
	recorderState.dataFormat.mSampleRate = 44100.0;
	recorderState.dataFormat.mFormatID = kAudioFormatLinearPCM;
	recorderState.dataFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
	recorderState.dataFormat.mBytesPerPacket = 4;
	recorderState.dataFormat.mFramesPerPacket = 1;
	recorderState.dataFormat.mBytesPerFrame = 4;
	recorderState.dataFormat.mChannelsPerFrame = 2;
	recorderState.dataFormat.mBitsPerChannel = 16;
	*/
	
	
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
		printf("Failed to create a new Audio Queue. Status code: %d\n", status);
	}
	
	
	UInt32 dataFormatSize = sizeof (recorderState.dataFormat);
	
	AudioQueueGetProperty(recorderState.queue,
			      kAudioConverterCurrentInputStreamDescription,
			      &recorderState.dataFormat,
			      &dataFormatSize);
	
	
	mf_rdpsnd_derive_buffer_size(recorderState.queue, &recorderState.dataFormat, 0.05, &recorderState.bufferByteSize);
	
		
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
	
	AudioQueueStart (recorderState.queue, NULL);
	
}

BOOL mf_peer_rdpsnd_init(mfPeerContext* context)
{
	context->rdpsnd = rdpsnd_server_context_new(context->vcm);
	context->rdpsnd->data = context;
	
	context->rdpsnd->server_formats = supported_audio_formats;
	context->rdpsnd->num_server_formats = sizeof(supported_audio_formats) / sizeof(supported_audio_formats[0]);
	
	context->rdpsnd->src_format.wFormatTag = 1;
	context->rdpsnd->src_format.nChannels = 2;
	context->rdpsnd->src_format.nSamplesPerSec = 44100;
	context->rdpsnd->src_format.wBitsPerSample = 16;
	
	context->rdpsnd->Activated = mf_peer_rdpsnd_activated;
	
	context->rdpsnd->Initialize(context->rdpsnd);
	
	return TRUE;
}

BOOL mf_peer_rdpsnd_stop()
{
	recorderState.isRunning = false;
	AudioQueueStop(recorderState.queue, true);
	
	return TRUE;
}

void mf_peer_rdpsnd_input_callback (void                                *inUserData,
                                    AudioQueueRef                       inAQ,
                                    AudioQueueBufferRef                 inBuffer,
                                    const AudioTimeStamp                *inStartTime,
                                    UInt32                              inNumberPacketDescriptions,
                                    const AudioStreamPacketDescription  *inPacketDescs)
{
	OSStatus status;
	AQRecorderState * rState;
	rState = inUserData;
	
	
	if (inNumberPacketDescriptions == 0 && rState->dataFormat.mBytesPerPacket != 0)
	{
		inNumberPacketDescriptions = inBuffer->mAudioDataByteSize / rState->dataFormat.mBytesPerPacket;
	}
	
	
	if (rState->isRunning == 0)
	{
		return ;
	}
	
	rState->snd_context->SendSamples(rState->snd_context, inBuffer->mAudioData, inBuffer->mAudioDataByteSize/4);
	
	status = AudioQueueEnqueueBuffer(
					 rState->queue,
					 inBuffer,
					 0,
					 NULL);
	
	if (status != noErr)
	{
		printf("AudioQueueEnqueueBuffer() returned status = %d\n", status);
	}
	
}

void mf_rdpsnd_derive_buffer_size (AudioQueueRef                audioQueue,
                                   AudioStreamBasicDescription  *ASBDescription,
                                   Float64                      seconds,
                                   UInt32                       *outBufferSize)
{
	static const int maxBufferSize = 0x50000;
	
	int maxPacketSize = ASBDescription->mBytesPerPacket;
	if (maxPacketSize == 0)
	{
		UInt32 maxVBRPacketSize = sizeof(maxPacketSize);
		AudioQueueGetProperty (audioQueue,
				       kAudioQueueProperty_MaximumOutputPacketSize,
				       // in Mac OS X v10.5, instead use
				       //   kAudioConverterPropertyMaximumOutputPacketSize
				       &maxPacketSize,
				       &maxVBRPacketSize
				       );
	}
	
	Float64 numBytesForTime =
	ASBDescription->mSampleRate * maxPacketSize * seconds;
	*outBufferSize = (UInt32) (numBytesForTime < maxBufferSize ? numBytesForTime : maxBufferSize);
}


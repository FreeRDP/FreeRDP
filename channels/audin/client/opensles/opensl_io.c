/*
opensl_io.c:
Android OpenSL input/output module
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

#include <winpr/assert.h>

#include "audin_main.h"
#include "opensl_io.h"
#define CONV16BIT 32768
#define CONVMYFLT (1. / 32768.)

typedef struct
{
	size_t size;
	void* data;
} queue_element;

struct opensl_stream
{
	// engine interfaces
	SLObjectItf engineObject;
	SLEngineItf engineEngine;

	// device interfaces
	SLDeviceVolumeItf deviceVolume;

	// recorder interfaces
	SLObjectItf recorderObject;
	SLRecordItf recorderRecord;
	SLAndroidSimpleBufferQueueItf recorderBufferQueue;

	unsigned int inchannels;
	unsigned int sr;
	unsigned int buffersize;
	unsigned int bits_per_sample;

	queue_element* prep;
	queue_element* next;

	void* context;
	opensl_receive_t receive;
};

static void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void* context);

// creates the OpenSL ES audio engine
static SLresult openSLCreateEngine(OPENSL_STREAM* p)
{
	SLresult result;
	// create engine
	result = slCreateEngine(&(p->engineObject), 0, NULL, 0, NULL, NULL);

	if (result != SL_RESULT_SUCCESS)
		goto engine_end;

	// realize the engine
	result = (*p->engineObject)->Realize(p->engineObject, SL_BOOLEAN_FALSE);

	if (result != SL_RESULT_SUCCESS)
		goto engine_end;

	// get the engine interface, which is needed in order to create other objects
	result = (*p->engineObject)->GetInterface(p->engineObject, SL_IID_ENGINE, &(p->engineEngine));

	if (result != SL_RESULT_SUCCESS)
		goto engine_end;

	// get the volume interface - important, this is optional!
	result =
	    (*p->engineObject)->GetInterface(p->engineObject, SL_IID_DEVICEVOLUME, &(p->deviceVolume));

	if (result != SL_RESULT_SUCCESS)
	{
		p->deviceVolume = NULL;
		result = SL_RESULT_SUCCESS;
	}

engine_end:
	WINPR_ASSERT(SL_RESULT_SUCCESS == result);
	return result;
}

// Open the OpenSL ES device for input
static SLresult openSLRecOpen(OPENSL_STREAM* p)
{
	SLresult result;
	SLuint32 sr = p->sr;
	SLuint32 channels = p->inchannels;
	WINPR_ASSERT(!p->recorderObject);

	if (channels)
	{
		switch (sr)
		{
			case 8000:
				sr = SL_SAMPLINGRATE_8;
				break;

			case 11025:
				sr = SL_SAMPLINGRATE_11_025;
				break;

			case 16000:
				sr = SL_SAMPLINGRATE_16;
				break;

			case 22050:
				sr = SL_SAMPLINGRATE_22_05;
				break;

			case 24000:
				sr = SL_SAMPLINGRATE_24;
				break;

			case 32000:
				sr = SL_SAMPLINGRATE_32;
				break;

			case 44100:
				sr = SL_SAMPLINGRATE_44_1;
				break;

			case 48000:
				sr = SL_SAMPLINGRATE_48;
				break;

			case 64000:
				sr = SL_SAMPLINGRATE_64;
				break;

			case 88200:
				sr = SL_SAMPLINGRATE_88_2;
				break;

			case 96000:
				sr = SL_SAMPLINGRATE_96;
				break;

			case 192000:
				sr = SL_SAMPLINGRATE_192;
				break;

			default:
				return -1;
		}

		// configure audio source
		SLDataLocator_IODevice loc_dev = { SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
			                               SL_DEFAULTDEVICEID_AUDIOINPUT, NULL };
		SLDataSource audioSrc = { &loc_dev, NULL };
		// configure audio sink
		int speakers;

		if (channels > 1)
			speakers = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
		else
			speakers = SL_SPEAKER_FRONT_CENTER;

		SLDataLocator_AndroidSimpleBufferQueue loc_bq = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
			                                              2 };
		SLDataFormat_PCM format_pcm;
		format_pcm.formatType = SL_DATAFORMAT_PCM;
		format_pcm.numChannels = channels;
		format_pcm.samplesPerSec = sr;
		format_pcm.channelMask = speakers;
		format_pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;

		if (16 == p->bits_per_sample)
		{
			format_pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
			format_pcm.containerSize = 16;
		}
		else if (8 == p->bits_per_sample)
		{
			format_pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_8;
			format_pcm.containerSize = 8;
		}
		else
			WINPR_ASSERT(0);

		SLDataSink audioSnk = { &loc_bq, &format_pcm };
		// create audio recorder
		// (requires the RECORD_AUDIO permission)
		const SLInterfaceID id[] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE };
		const SLboolean req[] = { SL_BOOLEAN_TRUE };
		result = (*p->engineEngine)
		             ->CreateAudioRecorder(p->engineEngine, &(p->recorderObject), &audioSrc,
		                                   &audioSnk, 1, id, req);
		WINPR_ASSERT(!result);

		if (SL_RESULT_SUCCESS != result)
			goto end_recopen;

		// realize the audio recorder
		result = (*p->recorderObject)->Realize(p->recorderObject, SL_BOOLEAN_FALSE);
		WINPR_ASSERT(!result);

		if (SL_RESULT_SUCCESS != result)
			goto end_recopen;

		// get the record interface
		result = (*p->recorderObject)
		             ->GetInterface(p->recorderObject, SL_IID_RECORD, &(p->recorderRecord));
		WINPR_ASSERT(!result);

		if (SL_RESULT_SUCCESS != result)
			goto end_recopen;

		// get the buffer queue interface
		result = (*p->recorderObject)
		             ->GetInterface(p->recorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
		                            &(p->recorderBufferQueue));
		WINPR_ASSERT(!result);

		if (SL_RESULT_SUCCESS != result)
			goto end_recopen;

		// register callback on the buffer queue
		result = (*p->recorderBufferQueue)
		             ->RegisterCallback(p->recorderBufferQueue, bqRecorderCallback, p);
		WINPR_ASSERT(!result);

		if (SL_RESULT_SUCCESS != result)
			goto end_recopen;

	end_recopen:
		return result;
	}
	else
		return SL_RESULT_SUCCESS;
}

// close the OpenSL IO and destroy the audio engine
static void openSLDestroyEngine(OPENSL_STREAM* p)
{
	// destroy audio recorder object, and invalidate all associated interfaces
	if (p->recorderObject != NULL)
	{
		(*p->recorderObject)->Destroy(p->recorderObject);
		p->recorderObject = NULL;
		p->recorderRecord = NULL;
		p->recorderBufferQueue = NULL;
	}

	// destroy engine object, and invalidate all associated interfaces
	if (p->engineObject != NULL)
	{
		(*p->engineObject)->Destroy(p->engineObject);
		p->engineObject = NULL;
		p->engineEngine = NULL;
	}
}

static queue_element* opensles_queue_element_new(size_t size)
{
	queue_element* q = calloc(1, sizeof(queue_element));

	if (!q)
		goto fail;

	q->size = size;
	q->data = malloc(size);

	if (!q->data)
		goto fail;

	return q;
fail:
	free(q);
	return NULL;
}

static void opensles_queue_element_free(void* obj)
{
	queue_element* e = (queue_element*)obj;

	if (e)
		free(e->data);

	free(e);
}

// open the android audio device for input
OPENSL_STREAM* android_OpenRecDevice(void* context, opensl_receive_t receive, int sr,
                                     int inchannels, int bufferframes, int bits_per_sample)
{
	OPENSL_STREAM* p;

	if (!context || !receive)
		return NULL;

	p = (OPENSL_STREAM*)calloc(1, sizeof(OPENSL_STREAM));

	if (!p)
		return NULL;

	p->context = context;
	p->receive = receive;
	p->inchannels = inchannels;
	p->sr = sr;
	p->buffersize = bufferframes;
	p->bits_per_sample = bits_per_sample;

	if ((p->bits_per_sample != 8) && (p->bits_per_sample != 16))
		goto fail;

	if (openSLCreateEngine(p) != SL_RESULT_SUCCESS)
		goto fail;

	if (openSLRecOpen(p) != SL_RESULT_SUCCESS)
		goto fail;

	/* Create receive buffers, prepare them and start recording */
	p->prep = opensles_queue_element_new(p->buffersize * p->bits_per_sample / 8);
	p->next = opensles_queue_element_new(p->buffersize * p->bits_per_sample / 8);

	if (!p->prep || !p->next)
		goto fail;

	(*p->recorderBufferQueue)->Enqueue(p->recorderBufferQueue, p->next->data, p->next->size);
	(*p->recorderBufferQueue)->Enqueue(p->recorderBufferQueue, p->prep->data, p->prep->size);
	(*p->recorderRecord)->SetRecordState(p->recorderRecord, SL_RECORDSTATE_RECORDING);
	return p;
fail:
	android_CloseRecDevice(p);
	return NULL;
}

// close the android audio device
void android_CloseRecDevice(OPENSL_STREAM* p)
{
	if (p == NULL)
		return;

	opensles_queue_element_free(p->next);
	opensles_queue_element_free(p->prep);
	openSLDestroyEngine(p);
	free(p);
}

// this callback handler is called every time a buffer finishes recording
static void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void* context)
{
	OPENSL_STREAM* p = (OPENSL_STREAM*)context;
	queue_element* e;

	if (!p)
		return;

	e = p->next;

	if (!e)
		return;

	if (!p->context || !p->receive)
		WLog_WARN(TAG, "Missing receive callback=%p, context=%p", p->receive, p->context);
	else
		p->receive(p->context, e->data, e->size);

	p->next = p->prep;
	p->prep = e;
	(*p->recorderBufferQueue)->Enqueue(p->recorderBufferQueue, e->data, e->size);
}

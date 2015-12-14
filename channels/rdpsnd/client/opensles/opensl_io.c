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

#include <assert.h>

#include "rdpsnd_main.h"
#include "opensl_io.h"
#define CONV16BIT 32768
#define CONVMYFLT (1./32768.)

static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context);

// creates the OpenSL ES audio engine
static SLresult openSLCreateEngine(OPENSL_STREAM *p)
{
  SLresult result;
  // create engine
  result = slCreateEngine(&(p->engineObject), 0, NULL, 0, NULL, NULL);
	DEBUG_SND("engineObject=%p", p->engineObject);
  if(result != SL_RESULT_SUCCESS) goto  engine_end;

  // realize the engine 
  result = (*p->engineObject)->Realize(p->engineObject, SL_BOOLEAN_FALSE);
	DEBUG_SND("Realize=%d", result);
  if(result != SL_RESULT_SUCCESS) goto engine_end;

  // get the engine interface, which is needed in order to create other objects
  result = (*p->engineObject)->GetInterface(p->engineObject, SL_IID_ENGINE, &(p->engineEngine));
	DEBUG_SND("engineEngine=%p", p->engineEngine);
  if(result != SL_RESULT_SUCCESS) goto  engine_end;

 engine_end:
  return result;
}

// opens the OpenSL ES device for output
static SLresult openSLPlayOpen(OPENSL_STREAM *p)
{
  SLresult result;
  SLuint32 sr = p->sr;
  SLuint32  channels = p->outchannels;

	assert(p->engineObject);
	assert(p->engineEngine);

  if(channels){
    // configure audio source
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq =
		{
			SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
			p->queuesize
		};

    switch(sr){

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
   
    const SLInterfaceID ids[] = {SL_IID_VOLUME};
    const SLboolean req[] = {SL_BOOLEAN_FALSE};
    result = (*p->engineEngine)->CreateOutputMix(p->engineEngine, &(p->outputMixObject), 1, ids, req);
		DEBUG_SND("engineEngine=%p", p->engineEngine);
		assert(!result);
    if(result != SL_RESULT_SUCCESS) goto end_openaudio;

    // realize the output mix
    result = (*p->outputMixObject)->Realize(p->outputMixObject, SL_BOOLEAN_FALSE);
		DEBUG_SND("Realize=%d", result);
		assert(!result);
    if(result != SL_RESULT_SUCCESS) goto end_openaudio;
   
    int speakers;
    if(channels > 1) 
      speakers = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    else speakers = SL_SPEAKER_FRONT_CENTER;
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM,channels, sr,
				   SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
				   speakers, SL_BYTEORDER_LITTLEENDIAN};

    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, p->outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, NULL};

    // create audio player
    const SLInterfaceID ids1[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_VOLUME};
    const SLboolean req1[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    result = (*p->engineEngine)->CreateAudioPlayer(p->engineEngine,
				&(p->bqPlayerObject), &audioSrc, &audioSnk, 2, ids1, req1);
		DEBUG_SND("bqPlayerObject=%p", p->bqPlayerObject);
		assert(!result);
    if(result != SL_RESULT_SUCCESS) goto end_openaudio;

    // realize the player
    result = (*p->bqPlayerObject)->Realize(p->bqPlayerObject, SL_BOOLEAN_FALSE);
		DEBUG_SND("Realize=%d", result);
		assert(!result);
    if(result != SL_RESULT_SUCCESS) goto end_openaudio;

    // get the play interface
    result = (*p->bqPlayerObject)->GetInterface(p->bqPlayerObject, SL_IID_PLAY, &(p->bqPlayerPlay));
		DEBUG_SND("bqPlayerPlay=%p", p->bqPlayerPlay);
		assert(!result);
    if(result != SL_RESULT_SUCCESS) goto end_openaudio;

    // get the volume interface
    result = (*p->bqPlayerObject)->GetInterface(p->bqPlayerObject, SL_IID_VOLUME, &(p->bqPlayerVolume));
		DEBUG_SND("bqPlayerVolume=%p", p->bqPlayerVolume);
		assert(!result);
    if(result != SL_RESULT_SUCCESS) goto end_openaudio;

    // get the buffer queue interface
    result = (*p->bqPlayerObject)->GetInterface(p->bqPlayerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
						&(p->bqPlayerBufferQueue));
		DEBUG_SND("bqPlayerBufferQueue=%p", p->bqPlayerBufferQueue);
		assert(!result);
    if(result != SL_RESULT_SUCCESS) goto end_openaudio;

    // register callback on the buffer queue
    result = (*p->bqPlayerBufferQueue)->RegisterCallback(p->bqPlayerBufferQueue, bqPlayerCallback, p);
		DEBUG_SND("bqPlayerCallback=%p", p->bqPlayerCallback);
		assert(!result);
    if(result != SL_RESULT_SUCCESS) goto end_openaudio;

    // set the player's state to playing
    result = (*p->bqPlayerPlay)->SetPlayState(p->bqPlayerPlay, SL_PLAYSTATE_PLAYING);
		DEBUG_SND("SetPlayState=%d", result);
		assert(!result);
 
  end_openaudio:
		assert(!result);
    return result;
  }
  return SL_RESULT_SUCCESS;
}

// close the OpenSL IO and destroy the audio engine
static void openSLDestroyEngine(OPENSL_STREAM *p){

  // destroy buffer queue audio player object, and invalidate all associated interfaces
  if (p->bqPlayerObject != NULL) {
    (*p->bqPlayerObject)->Destroy(p->bqPlayerObject);
    p->bqPlayerObject = NULL;
    p->bqPlayerVolume = NULL;
    p->bqPlayerPlay = NULL;
    p->bqPlayerBufferQueue = NULL;
    p->bqPlayerEffectSend = NULL;
  }

  // destroy output mix object, and invalidate all associated interfaces
  if (p->outputMixObject != NULL) {
    (*p->outputMixObject)->Destroy(p->outputMixObject);
    p->outputMixObject = NULL;
  }

  // destroy engine object, and invalidate all associated interfaces
  if (p->engineObject != NULL) {
    (*p->engineObject)->Destroy(p->engineObject);
    p->engineObject = NULL;
    p->engineEngine = NULL;
  }

}


// open the android audio device for and/or output
OPENSL_STREAM *android_OpenAudioDevice(int sr, int outchannels, int bufferframes){
	OPENSL_STREAM *p;
	p = (OPENSL_STREAM *) calloc(sizeof(OPENSL_STREAM), 1);
	if (!p)
		return NULL;

	p->queuesize = bufferframes;
	p->outchannels = outchannels;
	p->sr = sr;

	if(openSLCreateEngine(p) != SL_RESULT_SUCCESS)
	{
		android_CloseAudioDevice(p);
		return NULL;
	}

	if(openSLPlayOpen(p) != SL_RESULT_SUCCESS)
	{
		android_CloseAudioDevice(p);
		return NULL;
	}

	p->queue = Queue_New(TRUE, -1, -1);
	if (!p->queue)
	{
		android_CloseAudioDevice(p);
		return NULL;
	}

	return p;
}

// close the android audio device
void android_CloseAudioDevice(OPENSL_STREAM *p){

  if (p == NULL)
    return;

  openSLDestroyEngine(p);
	if (p->queue)
		Queue_Free(p->queue);

  free(p);
}

// this callback handler is called every time a buffer finishes playing
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
  OPENSL_STREAM *p = (OPENSL_STREAM *) context;

	assert(p);
	assert(p->queue);

	void *data = Queue_Dequeue(p->queue);
	free(data);
}

// puts a buffer of size samples to the device
int android_AudioOut(OPENSL_STREAM *p, const short *buffer,int size)
{
	assert(p);
	assert(buffer);
	assert(size > 0);

	/* Assure, that the queue is not full. */
	if (p->queuesize <= Queue_Count(p->queue) && WaitForSingleObject(p->queue->event, INFINITE) == WAIT_FAILED)
    {
        DEBUG_SND("WaitForSingleObject failed!");
        return -1;
    }

	void *data = calloc(size, sizeof(short));
	if (!data)
	{
		DEBUG_SND("unable to allocate a buffer");
		return -1;
	}
	memcpy(data, buffer, size * sizeof(short));
	Queue_Enqueue(p->queue, data);
 	(*p->bqPlayerBufferQueue)->Enqueue(p->bqPlayerBufferQueue, 
	 	data, sizeof(short) * size);
  
	return size;
}

int android_GetOutputMute(OPENSL_STREAM *p) {
	SLboolean mute;

	assert(p);
	assert(p->bqPlayerVolume);

	SLresult rc = (*p->bqPlayerVolume)->GetMute(p->bqPlayerVolume, &mute);
	assert(SL_RESULT_SUCCESS == rc);

	return mute;
}

void android_SetOutputMute(OPENSL_STREAM *p, BOOL _mute) {
	SLboolean mute = _mute;

	assert(p);
	assert(p->bqPlayerVolume);

	SLresult rc = (*p->bqPlayerVolume)->SetMute(p->bqPlayerVolume, mute);
	assert(SL_RESULT_SUCCESS == rc);
}

int android_GetOutputVolume(OPENSL_STREAM *p){
	SLmillibel level;

	assert(p);
	assert(p->bqPlayerVolume);

	SLresult rc = (*p->bqPlayerVolume)->GetVolumeLevel(p->bqPlayerVolume, &level);
	assert(SL_RESULT_SUCCESS == rc);

	return level;
}

int android_GetOutputVolumeMax(OPENSL_STREAM *p){
	SLmillibel level;

	assert(p);
	assert(p->bqPlayerVolume);

	SLresult rc = (*p->bqPlayerVolume)->GetMaxVolumeLevel(p->bqPlayerVolume, &level);
	assert(SL_RESULT_SUCCESS == rc);

	return level;
}


void android_SetOutputVolume(OPENSL_STREAM *p, int level){
	SLresult rc = (*p->bqPlayerVolume)->SetVolumeLevel(p->bqPlayerVolume, level);
	assert(SL_RESULT_SUCCESS == rc);
}


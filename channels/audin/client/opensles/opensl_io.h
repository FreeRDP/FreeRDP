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

#ifndef OPENSL_IO
#define OPENSL_IO

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <pthread.h>
#include <stdlib.h>

typedef struct threadLock_{
  pthread_mutex_t m;
  pthread_cond_t  c;
  unsigned char   s;
} threadLock;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct opensl_stream {
  
  // engine interfaces
  SLObjectItf engineObject;
  SLEngineItf engineEngine;

  // output mix interfaces
  SLObjectItf outputMixObject;

  // buffer queue player interfaces
  SLObjectItf bqPlayerObject;
  SLPlayItf bqPlayerPlay;
  SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
  SLEffectSendItf bqPlayerEffectSend;

  // recorder interfaces
  SLObjectItf recorderObject;
  SLRecordItf recorderRecord;
  SLAndroidSimpleBufferQueueItf recorderBufferQueue;

  // buffer indexes
  int currentInputIndex;
  int currentOutputIndex;

  // current buffer half (0, 1)
  int currentOutputBuffer;
  int currentInputBuffer;
  
  // buffers
  short *outputBuffer[2];
  short *inputBuffer[2];

  // size of buffers
  int outBufSamples;
  int inBufSamples;

  // locks
  void*  inlock;
  void*  outlock;

  double time;
  int inchannels;
  int outchannels;
  int   sr;

} OPENSL_STREAM;

  /*
  Open the audio device with a given sampling rate (sr), input and output channels and IO buffer size
  in frames. Returns a handle to the OpenSL stream
  */
  OPENSL_STREAM* android_OpenAudioDevice(int sr, int inchannels, int outchannels, int bufferframes);
  /* 
  Close the audio device 
  */
  void android_CloseAudioDevice(OPENSL_STREAM *p);
  /* 
  Read a buffer from the OpenSL stream *p, of size samples. Returns the number of samples read.
  */
  int android_AudioIn(OPENSL_STREAM *p, float *buffer,int size);
  /*
  Write a buffer to the OpenSL stream *p, of size samples. Returns the number of samples written.
  */
  int android_AudioOut(OPENSL_STREAM *p, float *buffer,int size);
  /*
  Get the current IO block time in seconds
  */
  double android_GetTimestamp(OPENSL_STREAM *p);
  
#ifdef __cplusplus
};
#endif

#endif // #ifndef OPENSL_IO

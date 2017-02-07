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

#include <winpr/synch.h>
#include <winpr/collections.h>

#include <freerdp/api.h>

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	size_t size;
	void* data;
} queue_element;

typedef struct opensl_stream
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

	wQueue* queue;
	queue_element* prep;
	queue_element* next;
} OPENSL_STREAM;

/*
Open the audio device with a given sampling rate (sr), input and output channels and IO buffer size
in frames. Returns a handle to the OpenSL stream
*/
FREERDP_LOCAL OPENSL_STREAM* android_OpenRecDevice(char* name, int sr,
        int inchannels,
        int bufferframes, int bits_per_sample);
/*
Close the audio device
*/
FREERDP_LOCAL void android_CloseRecDevice(OPENSL_STREAM* p);
/*
Read a buffer from the OpenSL stream *p, of size samples. Returns the number of samples read.
*/
FREERDP_LOCAL int android_RecIn(OPENSL_STREAM* p, short* buffer, int size);
#ifdef __cplusplus
};
#endif

#endif // #ifndef OPENSL_IO

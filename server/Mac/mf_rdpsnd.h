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

#ifndef MF_RDPSND_H
#define MF_RDPSND_H

#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>

#include <freerdp/freerdp.h>
#include <freerdp/listener.h>
#include <freerdp/server/rdpsnd.h>

#include "mf_interface.h"
#include "mfreerdp.h"

void mf_rdpsnd_derive_buffer_size (AudioQueueRef                audioQueue,
                                   AudioStreamBasicDescription  *ASBDescription,
                                   Float64                      seconds,
                                   UInt32                       *outBufferSize);

void mf_peer_rdpsnd_input_callback (void                                *inUserData,
				    AudioQueueRef                       inAQ,
				    AudioQueueBufferRef                 inBuffer,
				    const AudioTimeStamp                *inStartTime,
				    UInt32                              inNumberPacketDescriptions,
				    const AudioStreamPacketDescription  *inPacketDescs);


#define SND_NUMBUFFERS  3
struct _AQRecorderState
{
	AudioStreamBasicDescription  dataFormat;
	AudioQueueRef                queue;
	AudioQueueBufferRef          buffers[SND_NUMBUFFERS];
	AudioFileID                  audioFile;
	UInt32                       bufferByteSize;
	SInt64                       currentPacket;
	bool                         isRunning;
	RdpsndServerContext*       snd_context;
	
};

typedef struct _AQRecorderState AQRecorderState;

BOOL mf_peer_rdpsnd_init(mfPeerContext* context);
BOOL mf_peer_rdpsnd_stop(void);

#endif /* MF_RDPSND_H */


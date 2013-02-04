/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Redirection Virtual Channel - Decoder
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2012 Hewlett-Packard Development Company, L.P.
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

#ifndef __TSMF_DECODER_H
#define __TSMF_DECODER_H

#include "tsmf_types.h"

typedef enum _ITSMFControlMsg
{
	Control_Pause,
	Control_Restart,
	Control_Flush,
	Control_EndOfStream
} ITSMFControlMsg;

typedef struct _ITSMFDecoder ITSMFDecoder;

struct _ITSMFDecoder
{
	/* Set the decoder format. Return true if supported. */
	BOOL (*SetFormat) (ITSMFDecoder* decoder, TS_AM_MEDIA_TYPE* media_type);
	/* Decode a sample. */
	BOOL (*Decode) (ITSMFDecoder* decoder, const BYTE* data, UINT32 data_size, UINT32 extensions);
	/* Get the decoded data */
	BYTE* (*GetDecodedData) (ITSMFDecoder* decoder, UINT32* size);
	/* Get the pixel format of decoded video frame */
	UINT32 (*GetDecodedFormat) (ITSMFDecoder* decoder);
	/* Get the width and height of decoded video frame */
	BOOL (*GetDecodedDimension) (ITSMFDecoder* decoder, UINT32* width, UINT32* height);
	/* Free the decoder */
	void (*Free) (ITSMFDecoder * decoder);
	/* Optional Contol function */
	void (*Control) (ITSMFDecoder * decoder, ITSMFControlMsg control_msg, UINT32 *arg);
	/* Decode a sample with extended interface. */
	int (*DecodeEx) (ITSMFDecoder * decoder, const BYTE * data, UINT32 data_size, UINT32 extensions,
        			UINT64 start_time, UINT64 end_time, UINT64 duration);
	/* Get current play time */
	UINT64 (*GetRunningTime) (ITSMFDecoder * decoder);
	/* Update Gstreamer Rendering Area */
	void (*UpdateRenderingArea) (ITSMFDecoder * decoder, int newX, int newY, int newWidth, int newHeight, int numRectangles, RDP_RECT *rectangles);
	/* Change Gstreamer Audio Volume */
	void (*ChangeVolume) (ITSMFDecoder * decoder, UINT32 newVolume, UINT32 muted);
	/* Check buffer level */
	UINT32 (*BufferLevel) (ITSMFDecoder * decoder);
};

#define TSMF_DECODER_EXPORT_FUNC_NAME "TSMFDecoderEntry"
typedef ITSMFDecoder* (*TSMF_DECODER_ENTRY) (void);

ITSMFDecoder* tsmf_load_decoder(const char* name, TS_AM_MEDIA_TYPE* media_type);

#endif


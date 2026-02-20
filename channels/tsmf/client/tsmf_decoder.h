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

#ifndef FREERDP_CHANNEL_TSMF_CLIENT_DECODER_H
#define FREERDP_CHANNEL_TSMF_CLIENT_DECODER_H

#include "tsmf_types.h"

typedef enum
{
	Control_Pause,
	Control_Resume,
	Control_Restart,
	Control_Stop
} ITSMFControlMsg;

typedef struct s_ITSMFDecoder ITSMFDecoder;

struct s_ITSMFDecoder
{
	/* Set the decoder format. Return true if supported. */
	WINPR_ATTR_NODISCARD BOOL (*SetFormat)(ITSMFDecoder* decoder, TS_AM_MEDIA_TYPE* media_type);
	/* Decode a sample. */
	WINPR_ATTR_NODISCARD BOOL (*Decode)(ITSMFDecoder* decoder, const BYTE* data, UINT32 data_size,
	                                    UINT32 extensions);
	/* Get the decoded data */
	WINPR_ATTR_NODISCARD BYTE* (*GetDecodedData)(ITSMFDecoder* decoder, UINT32* size);
	/* Get the pixel format of decoded video frame */
	WINPR_ATTR_NODISCARD UINT32 (*GetDecodedFormat)(ITSMFDecoder* decoder);
	/* Get the width and height of decoded video frame */
	WINPR_ATTR_NODISCARD BOOL (*GetDecodedDimension)(ITSMFDecoder* decoder, UINT32* width,
	                                                 UINT32* height);
	/* Free the decoder */
	void (*Free)(ITSMFDecoder* decoder);
	/* Optional Control function */
	WINPR_ATTR_NODISCARD BOOL (*Control)(ITSMFDecoder* decoder, ITSMFControlMsg control_msg,
	                                     UINT32* arg);
	/* Decode a sample with extended interface. */
	WINPR_ATTR_NODISCARD BOOL (*DecodeEx)(ITSMFDecoder* decoder, const BYTE* data, UINT32 data_size,
	                                      UINT32 extensions, UINT64 start_time, UINT64 end_time,
	                                      UINT64 duration);
	/* Get current play time */
	WINPR_ATTR_NODISCARD UINT64 (*GetRunningTime)(ITSMFDecoder* decoder);
	/* Update Gstreamer Rendering Area */
	WINPR_ATTR_NODISCARD BOOL (*UpdateRenderingArea)(ITSMFDecoder* decoder, UINT32 newX,
	                                                 UINT32 newY, UINT32 newWidth, UINT32 newHeight,
	                                                 UINT32 numRectangles,
	                                                 const RECTANGLE_32* rectangles);
	/* Change Gstreamer Audio Volume */
	WINPR_ATTR_NODISCARD BOOL (*ChangeVolume)(ITSMFDecoder* decoder, UINT32 newVolume,
	                                          UINT32 muted);
	/* Check buffer level */
	WINPR_ATTR_NODISCARD BOOL (*BufferLevel)(ITSMFDecoder* decoder);
	/* Register a callback for frame ack. */
	WINPR_ATTR_NODISCARD BOOL (*SetAckFunc)(ITSMFDecoder* decoder, BOOL (*cb)(void*, BOOL),
	                                        void* stream);
	/* Register a callback for stream seek detection. */
	WINPR_ATTR_NODISCARD BOOL (*SetSyncFunc)(ITSMFDecoder* decoder, void (*cb)(void*),
	                                         void* stream);
};

#define TSMF_DECODER_EXPORT_FUNC_NAME "TSMFDecoderEntry"
typedef UINT(VCAPITYPE* TSMF_DECODER_ENTRY)(ITSMFDecoder** decoder);

WINPR_ATTR_NODISCARD FREERDP_LOCAL ITSMFDecoder* tsmf_load_decoder(const char* name,
                                                                   TS_AM_MEDIA_TYPE* media_type);

WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL tsmf_check_decoder_available(const char* name);

#endif /* FREERDP_CHANNEL_TSMF_CLIENT_DECODER_H */

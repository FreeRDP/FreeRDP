/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Video Redirection Virtual Channel - Decoder
 *
 * Copyright 2010-2011 Vic Lee
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

#include "drdynvc_types.h"
#include "tsmf_types.h"

typedef struct _ITSMFDecoder ITSMFDecoder;

struct _ITSMFDecoder
{
	/* Set the decoder format. Return true if supported. */
	boolean (*SetFormat) (ITSMFDecoder* decoder, TS_AM_MEDIA_TYPE* media_type);
	/* Decode a sample. */
	boolean (*Decode) (ITSMFDecoder* decoder, const uint8* data, uint32 data_size, uint32 extensions);
	/* Get the decoded data */
	uint8* (*GetDecodedData) (ITSMFDecoder* decoder, uint32* size);
	/* Get the pixel format of decoded video frame */
	uint32 (*GetDecodedFormat) (ITSMFDecoder* decoder);
	/* Get the width and height of decoded video frame */
	boolean (*GetDecodedDimension) (ITSMFDecoder* decoder, uint32* width, uint32* height);
	/* Free the decoder */
	void (*Free) (ITSMFDecoder* decoder);
};

#define TSMF_DECODER_EXPORT_FUNC_NAME "TSMFDecoderEntry"
typedef ITSMFDecoder* (*TSMF_DECODER_ENTRY) (void);

ITSMFDecoder* tsmf_load_decoder(const char* name, TS_AM_MEDIA_TYPE* media_type);

#endif


/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Redirection Virtual Channel - Codec
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

#ifndef FREERDP_CHANNEL_TSMF_CLIENT_CODEC_H
#define FREERDP_CHANNEL_TSMF_CLIENT_CODEC_H

#include "tsmf_types.h"

BOOL tsmf_codec_parse_media_type(TS_AM_MEDIA_TYPE* mediatype, wStream* s);
BOOL tsmf_codec_check_media_type(const char* decoder_name, wStream* s);

#endif /* FREERDP_CHANNEL_TSMF_CLIENT_CODEC_H */

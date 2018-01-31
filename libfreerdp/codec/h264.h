/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RemoteFX Codec Library - Decode
 *
 * Copyright 2018 Armin Novak <anovak@thincast.com>
 * Copyright 2018 Thincast Technologies GmbH
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

#ifndef FREERDP_LIB_CODEC_H264_H
#define FREERDP_LIB_CODEC_H264_H

#include <freerdp/api.h>
#include <freerdp/codec/h264.h>

FREERDP_LOCAL BOOL avc420_ensure_buffer(H264_CONTEXT* h264,
	UINT32 stride, UINT32 width, UINT32 height);

#endif /* FREERDP_LIB_CODEC_H264_H */


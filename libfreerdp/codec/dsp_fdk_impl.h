/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Digital Sound Processing
 *
 * Copyright 2022 Armin Novak <anovak@thincast.com>
 * Copyright 2022 Thincast Technologies GmbH
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

#ifndef FREERDP_DSP_FDK_IMPL_H_
#define FREERDP_DSP_FDK_IMPL_H_

#include <stdlib.h>

typedef void (*fdk_log_fkt_t)(unsigned log_level, const char* fmt, ...);

int fdk_aac_dsp_impl_init(void** handle, int encoder, fdk_log_fkt_t log);
void fdk_aac_dsp_impl_uninit(void** handle, int encoder, fdk_log_fkt_t log);

ssize_t fdk_aac_dsp_impl_stream_info(void* handle, int encoder, fdk_log_fkt_t log);

int fdk_aac_dsp_impl_config(void* handle, size_t* pbuffersize, int encoder, unsigned samplerate,
                            unsigned channels, unsigned bytes_per_second,
                            unsigned frames_per_packet, fdk_log_fkt_t log);

ssize_t fdk_aac_dsp_impl_decode_fill(void* handle, const void* data, size_t size,
                                     fdk_log_fkt_t log);

ssize_t fdk_aac_dsp_impl_encode(void* handle, const void* data, size_t size, void* dst,
                                size_t dstSize, fdk_log_fkt_t log);

ssize_t fdk_aac_dsp_impl_decode_read(void* handle, void* dst, size_t dstSize, fdk_log_fkt_t log);

#endif

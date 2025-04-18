/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * NSCodec Library - NEON Optimizations
 *
 * Copyright 2012 Vic Lee
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

#ifndef FREERDP_LIB_CODEC_NSC_NEON_H
#define FREERDP_LIB_CODEC_NSC_NEON_H

#include <winpr/sysinfo.h>

#include <freerdp/codec/nsc.h>
#include <freerdp/api.h>

FREERDP_LOCAL void nsc_init_neon_int(NSC_CONTEXT* WINPR_RESTRICT context);
static inline void nsc_init_neon(NSC_CONTEXT* WINPR_RESTRICT context)
{
	if (!IsProcessorFeaturePresent(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE))
		return;

	nsc_init_neon_int(context);
}

#endif /* FREERDP_LIB_CODEC_NSC_NEON_H */

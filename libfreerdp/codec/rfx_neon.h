/*
   FreeRDP: A Remote Desktop Protocol client.
   RemoteFX Codec Library - NEON Optimizations

   Copyright 2011 Martin Fleisz <mfleisz@thinstuff.com>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#ifndef __RFX_NEON_H
#define __RFX_NEON_H

#include <freerdp/codec/rfx.h>

#if defined(__ARM_NEON__)

void rfx_init_neon(RFX_CONTEXT * context);

#ifndef RFX_INIT_SIMD
 #if defined(WITH_NEON)
  #define RFX_INIT_SIMD(_rfx_context) rfx_init_neon(_rfx_context)
 #endif
#endif

#endif // __ARM_NEON__

#endif /* __RFX_NEON_H */


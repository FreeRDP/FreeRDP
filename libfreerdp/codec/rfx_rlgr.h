/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RemoteFX Codec Library - RLGR
 *
 * Copyright 2011 Vic Lee
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

#ifndef __RFX_RLGR_H
#define __RFX_RLGR_H

#include <freerdp/codec/rfx.h>

int rfx_rlgr_encode(RLGR_MODE mode, const INT16* data, int data_size, BYTE* buffer, int buffer_size);

#endif /* __RFX_RLGR_H */

/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * OS X Server Event Handling
 *
 * Copyright 2012 Corey Clayton <can.of.tuna@gmail.com>
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

#ifndef MF_MLION_H
#define MF_MLION_H

#include <freerdp/codec/rfx.h>


int mf_mlion_display_info(UINT32* disp_width, UINT32* dispHeight, UINT32* scale);

int mf_mlion_screen_updates_init(void);

int mf_mlion_start_getting_screen_updates(void);
int mf_mlion_stop_getting_screen_updates(void);

int mf_mlion_get_dirty_region(RFX_RECT* invalid);
int mf_mlion_peek_dirty_region(RFX_RECT* invalid);
int mf_mlion_clear_dirty_region(void);

int mf_mlion_get_pixelData(long x, long y, long width, long height, BYTE **pxData);

#endif

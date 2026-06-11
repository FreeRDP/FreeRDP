/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Android Display Update Virtual Channel
 *
 * Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>
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

#ifndef FREERDP_CLIENT_ANDROID_DISP_H
#define FREERDP_CLIENT_ANDROID_DISP_H

#include <freerdp/client/disp.h>
#include <freerdp/api.h>

#include "android_freerdp.h"

FREERDP_LOCAL BOOL android_disp_init(androidContext* afc, DispClientContext* disp);
FREERDP_LOCAL BOOL android_disp_uninit(androidContext* afc, DispClientContext* disp);
FREERDP_LOCAL BOOL android_disp_send_monitor_layout(androidContext* afc, UINT32 width,
                                                    UINT32 height);

#endif /* FREERDP_CLIENT_ANDROID_DISP_H */

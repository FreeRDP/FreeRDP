/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Display update notifications
 *
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
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

#ifndef FREERDP_LIB_CORE_DISPLAY_H
#define FREERDP_LIB_CORE_DISPLAY_H

#include <freerdp/display.h>
#include "rdp.h"

FREERDP_LOCAL BOOL display_convert_rdp_monitor_to_monitor_def(UINT32 monitorCount,
                                                              const rdpMonitor* monitorDefArray,
                                                              MONITOR_DEF** result);

#endif /* FREERDP_LIB_CORE_DISPLAY_H */

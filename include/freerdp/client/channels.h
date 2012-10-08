/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Client Channels
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CHANNELS_CLIENT
#define FREERDP_CHANNELS_CLIENT

#include <freerdp/api.h>

void* freerdp_channels_find_static_virtual_channel_entry(const char* name);
void* freerdp_channels_find_static_device_service_entry(const char* name);
void* freerdp_channels_find_static_entry(const char* name, const char* entry);

#endif /* FREERDP_CHANNELS_CLIENT */


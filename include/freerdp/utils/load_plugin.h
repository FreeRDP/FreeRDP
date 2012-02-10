/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Plugin Loading Utils
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

#ifndef __LOAD_PLUGIN_UTILS_H
#define __LOAD_PLUGIN_UTILS_H

#include <freerdp/api.h>
#include <freerdp/settings.h>

FREERDP_API void* freerdp_open_library(const char* file);
FREERDP_API void* freerdp_get_library_symbol(void* library, const char* name);
FREERDP_API boolean freerdp_close_library(void* library);
FREERDP_API void* freerdp_load_library_symbol(const char* file, const char* name);
FREERDP_API void* freerdp_load_plugin(const char* name, const char* entry_name);
FREERDP_API void* freerdp_load_channel_plugin(rdpSettings* settings, const char* name, const char* entry_name);

#endif /* __LOAD_PLUGIN_UTILS_H */

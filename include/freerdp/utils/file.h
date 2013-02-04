/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * File Utils
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_UTILS_FILE_H
#define FREERDP_UTILS_FILE_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/settings.h>

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API void freerdp_mkdir(char* path);
FREERDP_API BOOL freerdp_check_file_exists(char* file);
FREERDP_API char* freerdp_get_home_path(rdpSettings* settings);
FREERDP_API char* freerdp_get_config_path(rdpSettings* settings);
FREERDP_API char* freerdp_get_current_path(rdpSettings* settings);
FREERDP_API char* freerdp_construct_path(char* base_path, char* relative_path);
FREERDP_API char* freerdp_append_shared_library_suffix(char* file_path);
FREERDP_API char* freerdp_get_parent_path(char* base_path, int depth);
FREERDP_API BOOL freerdp_path_contains_separator(char* path);
FREERDP_API void freerdp_detect_paths(rdpSettings* settings);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_UTILS_FILE_H */

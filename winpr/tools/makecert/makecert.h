/**
 * WinPR: Windows Portable Runtime
 * makecert replacement
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

#ifndef MAKECERT_TOOL_H
#define MAKECERT_TOOL_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

typedef struct _MAKECERT_CONTEXT MAKECERT_CONTEXT;

WINPR_API int makecert_context_process(MAKECERT_CONTEXT* context, int argc, char** argv);

WINPR_API int makecert_context_set_output_file_name(MAKECERT_CONTEXT* context, char* name);
WINPR_API int makecert_context_output_certificate_file(MAKECERT_CONTEXT* context, char* path);
WINPR_API int makecert_context_output_private_key_file(MAKECERT_CONTEXT* context, char* path);

WINPR_API MAKECERT_CONTEXT* makecert_context_new();
WINPR_API void makecert_context_free(MAKECERT_CONTEXT* context);

#endif /* MAKECERT_TOOL_H */

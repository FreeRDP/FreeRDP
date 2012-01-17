/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Arguments Parsing
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

#ifndef __ARGS_UTILS_H
#define __ARGS_UTILS_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/settings.h>


typedef enum _FREERDP_ARGS_PARSE_RESULT
{
	FREERDP_ARGS_PARSE_FAILURE = -1,
	FREERDP_ARGS_PARSE_HELP = -2,
	FREERDP_ARGS_PARSE_VERSION = -3,
} FREERDP_ARGS_PARSE_RESULT;


/* Returns 1 if succeed, otherwise returns zero */
typedef int (*ProcessPluginArgs) (rdpSettings* settings, const char* name,
	RDP_PLUGIN_DATA* plugin_data, void* user_data);

/* Returns number of arguments processed (1 or 2), otherwise returns zero */
typedef int (*ProcessUIArgs) (rdpSettings* settings, const char* opt,
	const char* val, void* user_data);

FREERDP_API int freerdp_parse_args(rdpSettings* settings, int argc, char** argv,
	ProcessPluginArgs plugin_callback, void* plugin_user_data,
	ProcessUIArgs ui_callback, void* ui_user_data);

#endif /* __ARGS_UTILS_H */

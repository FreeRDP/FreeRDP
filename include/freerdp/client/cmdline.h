/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Client Command-Line Interface
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

#ifndef FREERDP_CLIENT_CMDLINE_H
#define FREERDP_CLIENT_CMDLINE_H

#include <freerdp/api.h>
#include <freerdp/freerdp.h>

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API int freerdp_client_parse_command_line_arguments(int argc, char** argv, rdpSettings* settings);
FREERDP_API int freerdp_client_command_line_status_print(int argc, char** argv, rdpSettings* settings, int status);
FREERDP_API int freerdp_client_load_addins(rdpChannels* channels, rdpSettings* settings);

FREERDP_API int freerdp_client_print_version(void);
FREERDP_API int freerdp_client_print_command_line_help(int argc, char** argv);

FREERDP_API int freerdp_parse_username(char* username, char** user, char** domain);
FREERDP_API int freerdp_set_connection_type(rdpSettings* settings, int type);

FREERDP_API int freerdp_client_add_device_channel(rdpSettings* settings, int count, char** params);
FREERDP_API int freerdp_client_add_static_channel(rdpSettings* settings, int count, char** params);
FREERDP_API int freerdp_client_add_dynamic_channel(rdpSettings* settings, int count, char** params);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CLIENT_CMDLINE_H */

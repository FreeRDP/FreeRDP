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

#include <winpr/cmdline.h>

#include <freerdp/api.h>
#include <freerdp/freerdp.h>

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API int freerdp_client_settings_parse_command_line_arguments(
    rdpSettings* settings, int argc, char** argv, BOOL allowUnknown);
FREERDP_API int freerdp_client_settings_command_line_status_print(
    rdpSettings* settings, int status, int argc, char** argv);
FREERDP_API int freerdp_client_settings_command_line_status_print_ex(
    rdpSettings* settings, int status, int argc, char** argv,
    COMMAND_LINE_ARGUMENT_A* custom);
FREERDP_API BOOL freerdp_client_load_addins(rdpChannels* channels, rdpSettings* settings);

FREERDP_API BOOL freerdp_client_print_version(void);
FREERDP_API BOOL freerdp_client_print_buildconfig(void);
FREERDP_API BOOL freerdp_client_print_command_line_help(int argc, char** argv);
FREERDP_API BOOL freerdp_client_print_command_line_help_ex(
    int argc, char** argv, COMMAND_LINE_ARGUMENT_A* custom);

FREERDP_API BOOL freerdp_parse_username(char* username, char** user, char** domain);
FREERDP_API BOOL freerdp_parse_hostname(char* hostname, char** host, int* port);
FREERDP_API BOOL freerdp_set_connection_type(rdpSettings* settings, int type);

FREERDP_API BOOL freerdp_client_add_device_channel(rdpSettings* settings, int count, char** params);
FREERDP_API BOOL freerdp_client_add_static_channel(rdpSettings* settings, int count, char** params);
FREERDP_API BOOL freerdp_client_add_dynamic_channel(rdpSettings* settings, int count,
        char** params);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CLIENT_CMDLINE_H */

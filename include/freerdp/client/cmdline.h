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
#include <freerdp/types.h>
#include <freerdp/settings.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/** \brief parses command line arguments to appropriate settings values.
	 *
	 * \param settings The settings instance to store the parsed values to
	 * \param argc the number of argv values
	 * \param argv an array of strings (char pointer)
	 * \param allowUnknown Allow unknown command line arguments or \b FALSE if not.
	 *
	 * \return \b 0 in case of success, a negative number in case of failure.
	 */
	FREERDP_API int freerdp_client_settings_parse_command_line_arguments(rdpSettings* settings,
	                                                                     int argc, char** argv,
	                                                                     BOOL allowUnknown);

	/** \brief parses command line arguments to appropriate settings values. Additionally allows
	 * supplying custom command line arguments and a handler function.
	 *
	 * \param settings The settings instance to store the parsed values to
	 * \param argc the number of argv values
	 * \param argv an array of strings (char pointer)
	 * \param allowUnknown Allow unknown command line arguments or \b FALSE if not.
	 * \param args Pointer to the custom arguments
	 * \param count The number of custom arguments
	 * \param handle_option the handler function for custom arguments.
	 * \param handle_userdata custom data supplied to \b handle_option as context
	 *
	 * \since version 3.0.0
	 *
	 * \return \b 0 in case of success, a negative number in case of failure.
	 */
	FREERDP_API int freerdp_client_settings_parse_command_line_arguments_ex(
	    rdpSettings* settings, int argc, char** argv, BOOL allowUnknown,
	    COMMAND_LINE_ARGUMENT_A* args, size_t count,
	    int (*handle_option)(const COMMAND_LINE_ARGUMENT_A* arg, void* custom),
	    void* handle_userdata);

	FREERDP_API int freerdp_client_settings_command_line_status_print(rdpSettings* settings,
	                                                                  int status, int argc,
	                                                                  char** argv);
	FREERDP_API int
	freerdp_client_settings_command_line_status_print_ex(rdpSettings* settings, int status,
	                                                     int argc, char** argv,
	                                                     const COMMAND_LINE_ARGUMENT_A* custom);
	FREERDP_API BOOL freerdp_client_load_addins(rdpChannels* channels, rdpSettings* settings);

	/** Print a command line warning about the component being unmaintained.
	 *
	 *  \since version 3.0.0
	 */
	FREERDP_API void freerdp_client_warn_unmaintained(int argc, char* argv[]);

	/** Print a command line warning about the component being experimental.
	 *
	 *  \since version 3.0.0
	 */
	FREERDP_API void freerdp_client_warn_experimental(int argc, char* argv[]);

	/** Print a command line warning about the component being deprecated.
	 *
	 *  \since version 3.0.0
	 */
	FREERDP_API void freerdp_client_warn_deprecated(int argc, char* argv[]);

	FREERDP_API BOOL freerdp_client_print_version(void);
	FREERDP_API BOOL freerdp_client_print_buildconfig(void);
	FREERDP_API BOOL freerdp_client_print_command_line_help(int argc, char** argv);
	FREERDP_API BOOL freerdp_client_print_command_line_help_ex(
	    int argc, char** argv, const COMMAND_LINE_ARGUMENT_A* custom);

	FREERDP_API BOOL freerdp_parse_username(const char* username, char** user, char** domain);
	FREERDP_API BOOL freerdp_parse_hostname(const char* hostname, char** host, int* port);
	FREERDP_API BOOL freerdp_set_connection_type(rdpSettings* settings, UINT32 type);

	FREERDP_API BOOL freerdp_client_add_device_channel(rdpSettings* settings, size_t count,
	                                                   const char** params);
	FREERDP_API BOOL freerdp_client_add_static_channel(rdpSettings* settings, size_t count,
	                                                   const char** params);
	FREERDP_API BOOL freerdp_client_del_static_channel(rdpSettings* settings, const char* name);
	FREERDP_API BOOL freerdp_client_add_dynamic_channel(rdpSettings* settings, size_t count,
	                                                    const char** params);
	FREERDP_API BOOL freerdp_client_del_dynamic_channel(rdpSettings* settings, const char* name);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CLIENT_CMDLINE_H */

/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Passphrase Handling Utils
 *
 * Copyright 2011 Shea Levy <shea@shealevy.com>
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

#ifndef FREERDP_UTILS_PASSPHRASE_H
#define FREERDP_UTILS_PASSPHRASE_H

#include <stdlib.h>
#include <stdio.h>

#include <freerdp/api.h>
#include <freerdp/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/** @brief Read a single character from a stream.
	 * This function will check \ref freerdp_shall_disconnect_context and abort when the session
	 * terminates.
	 *
	 * The function will abort on any of <ctrl>+[cdz] or end of stream conditions as well as when
	 * the RDP session terminates
	 *
	 * @param context The RDP context to work on
	 * @param stream the \ref FILE to read the data from
	 *
	 * @return The character read or \ref EOF in case of any failures
	 */
	FREERDP_API int freerdp_interruptible_getc(rdpContext* context, FILE* stream);

	/** @brief read a line from \ref stream with (optinal) default value that can be manipulated.
	 * This function will check \ref freerdp_shall_disconnect_context and abort when the session
	 * terminates.
	 *
	 * @param context The RDP context to work on
	 * @param lineptr on input a suggested default value, on output the result. Must be an allocated
	 * string on input, free the memory later with \ref free
	 * @param size on input the \ref strlen of the suggested default, on output \ref strlen of the
	 * result
	 * @param stream the \ref FILE to read the data from
	 *
	 * @return \b -1 in case of failure, otherwise \ref strlen of the result
	 */
	FREERDP_API SSIZE_T freerdp_interruptible_get_line(rdpContext* context, char** lineptr,
	                                                   size_t* size, FILE* stream);

	/** @brief similar to \ref freerdp_interruptible_get_line but disables echo to terminal.
	 *
	 * @param context The RDP context to work on
	 * @param prompt The prompt to show to the user
	 * @param buf A pointer to a buffer that will receive the output
	 * @param bufsiz The size of the buffer in bytes
	 * @param from_stdin
	 *
	 * @return A pointer to \ref buf containing the password or \ref NULL in case of an error.
	 */
	FREERDP_API const char* freerdp_passphrase_read(rdpContext* context, const char* prompt,
	                                                char* buf, size_t bufsiz, int from_stdin);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_UTILS_PASSPHRASE_H */

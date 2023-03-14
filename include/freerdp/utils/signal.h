/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Signal handling
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

#ifndef FREERDP_UTILS_SIGNAL_H
#define FREERDP_UTILS_SIGNAL_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef void (*freerdp_signal_handler_t)(int signum, const char* signame, void* context);

	FREERDP_API int freerdp_handle_signals(void);

	/** \brief registers a cleanup handler for non fatal signals.
	 *
	 *  This allows cleaning up resources like with \b atexit but for signals.
	 *
	 *  \param context a context for the clenaup handler.
	 *  \param handler the function to call on cleanup. Must not be \b NULL
	 *
	 *  \return \b TRUE if registered successfully, \b FALSE otherwise.
	 */
	FREERDP_API BOOL freerdp_add_signal_cleanup_handler(void* context,
	                                                    freerdp_signal_handler_t handler);

	/** \brief unregisters a cleanup handler for non fatal signals.
	 *
	 *  This allows removal of a cleanup handler for signals.
	 *
	 *  \param context a context for the clenaup handler.
	 *  \param handler the function to call on cleanup. Must not be \b NULL
	 *
	 *  \return \b TRUE if unregistered successfully, \b FALSE otherwise.
	 */
	FREERDP_API BOOL freerdp_del_signal_cleanup_handler(void* context,
	                                                    freerdp_signal_handler_t handler);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_UTILS_SIGNAL_H */

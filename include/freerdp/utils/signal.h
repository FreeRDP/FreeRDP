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
#include <freerdp/freerdp.h>

#ifndef _WIN32
#include <signal.h>
#include <termios.h>

FREERDP_API extern volatile sig_atomic_t terminal_needs_reset;
FREERDP_API extern int terminal_fildes;
FREERDP_API extern struct termios orig_flags;
FREERDP_API extern struct termios new_flags;
#endif

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API int freerdp_handle_signals(void);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_UTILS_SIGNAL_H */

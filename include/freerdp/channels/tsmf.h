/*
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Redirection Virtual Channel - Callback interface
 *
 * (C) Copyright 2014 Thincast Technologies GmbH
 * (C) Copyright 2014 Armin Novak <armin.novak@thincast.com>
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

#ifndef _TSMF_H_
#define _TSMF_H_

#include <freerdp/types.h>

/* Callback function setup order:
 *
 * When the channel is loaded, it calls TSMF_REGISTER to register the
 * decoder handle with the client.
 * The client then stores the handle and calls TSMF_REGISTER_INSTANCE
 * to give the channel the current handle to the session necessary
 * to call other functions.
 * After this initial setup the other functions can be used.
 */
/* Functions called from client -> registered by channel */
#define TSMF_GET_INSTANCE "tsmf_get_instance"
typedef void (*tsmf_get_instance)(void *instance, void *decoder);

#define TSMF_ADD_WINDOW_HANDLE "tsmf_add_window_handle"
typedef void (*tsmf_add_window_handle)(void *instance, void *decoder, void *window);

#define TSMF_DEL_WINDOW_HANDLE "tsmf_del_window_handle"
typedef void (*tsmf_del_window_handle)(void *instance, void *decoder);

/* Functions called from channel -> registered by client */
#define TSMF_REGISTER "tsmf_register"
typedef void (*tsmf_register)(void *instance, void *decoder);

#define TSMF_DESTROY "tsmf_destroy"
typedef void (*tsmf_destroy)(void *instance, void *decoder);

#define TSMF_PLAY "tsmf_play"
typedef void (*tsmf_play)(void *instance, void *decoder);

#define TSMF_PAUSE "tsmf_pause"
typedef void (*tsmf_pause)(void *instance, void *decoder);

#define TSMF_RESIZE_WINDOW "tsmf_resize_window"
typedef void (*tsmf_resize_window)(void *instance, void *decoder, int x, int y, int width,
								   int height, int nr_rect, RDP_RECT *visible);

#endif


/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RAIL Window List
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

#ifndef __WINDOW_LIST_H
#define __WINDOW_LIST_H

#include <freerdp/types.h>
#include <freerdp/update.h>
#include <freerdp/utils/stream.h>

#include <freerdp/rail/window.h>

typedef struct rdp_window_list rdpWindowList;

struct rdp_window_list
{
	rdpWindow* head;
	rdpWindow* tail;
};

void window_list_create(rdpWindowList* list, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* window_state);
void window_list_update(rdpWindowList* list, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* window_state);
void window_list_delete(rdpWindowList* list, WINDOW_ORDER_INFO* orderInfo);

rdpWindowList* window_list_new();
void window_list_free(rdpWindowList* list);

#endif /* __WINDOW_LIST_H */

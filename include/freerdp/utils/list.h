/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Double-linked List Utils
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

#ifndef __LIST_UTILS_H
#define __LIST_UTILS_H

#include <freerdp/api.h>
#include <freerdp/utils/memory.h>

typedef struct _LIST_ITEM LIST_ITEM;
struct _LIST_ITEM
{
	void* data;
	LIST_ITEM* prev;
	LIST_ITEM* next;
};

typedef struct _LIST LIST;
struct _LIST
{
	int count;
	LIST_ITEM* head;
	LIST_ITEM* tail;
};

FREERDP_API LIST* list_new(void);
FREERDP_API void list_free(LIST* list);
FREERDP_API void list_enqueue(LIST* list, void* data);
FREERDP_API void* list_dequeue(LIST* list);
FREERDP_API void* list_peek(LIST* list);
FREERDP_API void* list_next(LIST* list, void* data);
#define list_add(_l, _d) list_enqueue(_l, _d)
FREERDP_API void* list_remove(LIST* list, void* data);
FREERDP_API int list_size(LIST* list);

#endif /* __LIST_UTILS_H */

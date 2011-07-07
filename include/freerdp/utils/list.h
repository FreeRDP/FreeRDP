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

#define DEFINE_LIST_TYPE(_list_type, _item_type) \
\
struct _item_type##_full \
{ \
	struct _item_type item; \
	struct _item_type* prev; \
	struct _item_type* next; \
}; \
\
static struct _item_type* _item_type##_new(void) \
{ \
	struct _item_type* item; \
	item = (struct _item_type*)malloc(sizeof(struct _item_type##_full));\
	memset(item, 0, sizeof(struct _item_type##_full)); \
	return item; \
} \
\
static void _item_type##_free(struct _item_type* item); \
\
static struct _item_type* _item_type##_next(struct _item_type* item) \
{ \
	return ((struct _item_type##_full*)item)->next; \
} \
\
static struct _item_type* _item_type##_prev(struct _item_type* item) \
{ \
	return ((struct _item_type##_full*)item)->prev; \
} \
\
struct _list_type \
{ \
	struct _item_type* head; \
	struct _item_type* tail; \
}; \
\
static struct _list_type* _list_type##_new(void) \
{ \
	struct _list_type* list; \
	list = (struct _list_type*)malloc(sizeof(struct _list_type)); \
	memset(list, 0, sizeof(struct _list_type)); \
	return list; \
} \
\
static void _list_type##_enqueue(struct _list_type* list, struct _item_type* item) \
{ \
	if (list->tail == NULL) \
	{ \
		list->head = item; \
		list->tail = item; \
	} \
	else \
	{ \
		((struct _item_type##_full*)item)->prev = list->tail; \
		((struct _item_type##_full*)(list->tail))->next = item; \
		list->tail = item; \
	} \
} \
\
static struct _item_type* _list_type##_dequeue(struct _list_type* list) \
{ \
	struct _item_type* item; \
	item = list->head; \
	if (item != NULL) \
	{ \
		list->head = ((struct _item_type##_full*)item)->next; \
		((struct _item_type##_full*)item)->next = NULL; \
		if (list->head == NULL) \
			list->tail = NULL; \
		else \
			((struct _item_type##_full*)(list->head))->prev = NULL; \
	} \
	return item; \
} \
\
void _list_type##_free(struct _list_type* list) \
{ \
	struct _item_type* item; \
	while (list->head) \
	{ \
		item = _list_type##_dequeue(list); \
		_item_type##_free(item); \
		free(item); \
	} \
	free(list); \
}

#endif

/**
 * FreeRDP: A Remote Desktop Protocol Client
 * List Unit Tests
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <freerdp/freerdp.h>
#include <freerdp/utils/list.h>

#include "test_list.h"

int init_list_suite(void)
{
	return 0;
}

int clean_list_suite(void)
{
	return 0;
}

int add_list_suite(void)
{
	add_test_suite(list);

	add_test_function(list);

	return 0;
}

struct _my_list_item
{
	uint32 a;
	uint32 b;
};
typedef struct _my_list_item my_list_item;

void test_list(void)
{
	LIST* list;
	LIST_ITEM* list_item;
	my_list_item* item;
	my_list_item* item1;
	my_list_item* item2;
	int i;

	list = list_new();

	for (i = 0; i < 10; i++)
	{
		item = xnew(my_list_item);
		item->a = i;
		item->b = i * i;
		list_enqueue(list, item);
	}

	for (i = 0, list_item = list->head; list_item; i++, list_item = list_item->next)
	{
		CU_ASSERT(((my_list_item*)list_item->data)->a == i);
		CU_ASSERT(((my_list_item*)list_item->data)->b == i * i);
		/*printf("%d %d\n", item->a, item->b);*/
	}

	item1 = xnew(my_list_item);
	list_add(list, item1);
	item2 = xnew(my_list_item);
	list_add(list, item2);

	CU_ASSERT(list_remove(list, item1) == item1);
	xfree(item1);
	CU_ASSERT(list_remove(list, item2) == item2);
	CU_ASSERT(list_remove(list, item2) == NULL);
	xfree(item2);

	while ((item = list_dequeue(list)) != NULL)
		xfree(item);
	list_free(list);
}

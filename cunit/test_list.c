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

struct my_list_item
{
	uint32 a;
	uint32 b;
};

DEFINE_LIST_TYPE(my_list, my_list_item);

void my_list_item_free(struct my_list_item* item)
{
	item->a = 0;
	item->b = 0;
}

void test_list(void)
{
	struct my_list* list;
	struct my_list_item* item;
	int i;

	list = my_list_new();

	for (i = 0; i < 10; i++)
	{
		item = my_list_item_new();
		item->a = i;
		item->b = i * i;
		my_list_enqueue(list, item);
	}

	for (i = 0, item = list->head; item; i++, item = my_list_item_next(item))
	{
		CU_ASSERT(item->a == i);
		CU_ASSERT(item->b == i * i);
		/*printf("%d %d\n", item->a, item->b);*/
	}

	my_list_free(list);
}

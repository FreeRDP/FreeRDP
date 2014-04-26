/**
 * WinPR: Windows Portable Runtime
 * System.Collections.Stack
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/collections.h>

/**
 * C equivalent of the C# Stack Class:
 * http://msdn.microsoft.com/en-us/library/system.collections.stack.aspx
 */

/**
 * Properties
 */

/**
 * Gets the number of elements contained in the Stack.
 */

int Stack_Count(wStack* stack)
{
	int ret;

	if (stack->synchronized)
		EnterCriticalSection(&stack->lock);

	ret = stack->size;

	if (stack->synchronized)
		LeaveCriticalSection(&stack->lock);

	return ret;
}

/**
 * Gets a value indicating whether access to the Stack is synchronized (thread safe).
 */

BOOL Stack_IsSynchronized(wStack* stack)
{
	return stack->synchronized;
}

/**
 * Methods
 */

/**
 * Removes all objects from the Stack.
 */

void Stack_Clear(wStack* stack)
{
	int index;

	if (stack->synchronized)
		EnterCriticalSection(&stack->lock);

	for (index = 0; index < stack->size; index++)
	{
		if (stack->object.fnObjectFree)
			stack->object.fnObjectFree(stack->array[index]);

		stack->array[index] = NULL;
	}

	stack->size = 0;

	if (stack->synchronized)
		LeaveCriticalSection(&stack->lock);
}

/**
 * Determines whether an element is in the Stack.
 */

BOOL Stack_Contains(wStack* stack, void* obj)
{
	int i;
	BOOL found = FALSE;

	if (stack->synchronized)
		EnterCriticalSection(&stack->lock);

	for (i = 0; i < stack->size; i++)
	{
		if (stack->object.fnObjectEquals(stack->array[i], obj))
		{
			found = TRUE;
			break;
		}
	}

	if (stack->synchronized)
		LeaveCriticalSection(&stack->lock);

	return found;
}

/**
 * Inserts an object at the top of the Stack.
 */

void Stack_Push(wStack* stack, void* obj)
{
	if (stack->synchronized)
		EnterCriticalSection(&stack->lock);

	if ((stack->size + 1) >= stack->capacity)
	{
		stack->capacity *= 2;
		stack->array = (void**) realloc(stack->array, sizeof(void*) * stack->capacity);
	}

	stack->array[(stack->size)++] = obj;

	if (stack->synchronized)
		LeaveCriticalSection(&stack->lock);
}

/**
 * Removes and returns the object at the top of the Stack.
 */

void* Stack_Pop(wStack* stack)
{
	void* obj = NULL;

	if (stack->synchronized)
		EnterCriticalSection(&stack->lock);

	if (stack->size > 0)
		obj = stack->array[--(stack->size)];

	if (stack->synchronized)
		LeaveCriticalSection(&stack->lock);

	return obj;
}

/**
 * Returns the object at the top of the Stack without removing it.
 */

void* Stack_Peek(wStack* stack)
{
	void* obj = NULL;

	if (stack->synchronized)
		EnterCriticalSection(&stack->lock);

	if (stack->size > 0)
		obj = stack->array[stack->size];

	if (stack->synchronized)
		LeaveCriticalSection(&stack->lock);

	return obj;
}


static BOOL default_stack_equals(void *obj1, void *obj2)
{
	return (obj1 == obj2);
}

/**
 * Construction, Destruction
 */

wStack* Stack_New(BOOL synchronized)
{
	wStack* stack = NULL;

	stack = (wStack *)calloc(1, sizeof(wStack));
	if (!stack)
		return NULL;

	stack->object.fnObjectEquals = default_stack_equals;
	stack->synchronized = synchronized;

	stack->capacity = 32;
	stack->array = (void**) malloc(sizeof(void*) * stack->capacity);
	if (!stack->array)
		goto out_free;

	if (stack->synchronized && !InitializeCriticalSectionAndSpinCount(&stack->lock, 4000))
			goto out_free_array;

	return stack;

out_free_array:
	free(stack->array);
out_free:
	free(stack);
	return NULL;
}

void Stack_Free(wStack* stack)
{
	if (!stack)
	{
		if (stack->synchronized)
			DeleteCriticalSection(&stack->lock);

		free(stack->array);

		free(stack);
	}
}

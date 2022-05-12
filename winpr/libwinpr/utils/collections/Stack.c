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

#include <winpr/config.h>

#include <winpr/collections.h>
#include <winpr/assert.h>

struct s_wStack
{
	size_t size;
	size_t capacity;
	void** array;
	CRITICAL_SECTION lock;
	BOOL synchronized;
	wObject object;
};

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

size_t Stack_Count(wStack* stack)
{
	size_t ret;
	WINPR_ASSERT(stack);
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
	WINPR_ASSERT(stack);
	return stack->synchronized;
}

wObject* Stack_Object(wStack* stack)
{
	WINPR_ASSERT(stack);
	return &stack->object;
}

/**
 * Methods
 */

/**
 * Removes all objects from the Stack.
 */

void Stack_Clear(wStack* stack)
{
	size_t index;

	WINPR_ASSERT(stack);
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

BOOL Stack_Contains(wStack* stack, const void* obj)
{
	size_t i;
	BOOL found = FALSE;

	WINPR_ASSERT(stack);
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
	WINPR_ASSERT(stack);
	if (stack->synchronized)
		EnterCriticalSection(&stack->lock);

	if ((stack->size + 1) >= stack->capacity)
	{
		const size_t new_cap = stack->capacity * 2;
		void** new_arr = (void**)realloc(stack->array, sizeof(void*) * new_cap);

		if (!new_arr)
			goto end;

		stack->array = new_arr;
		stack->capacity = new_cap;
	}

	stack->array[(stack->size)++] = obj;

end:
	if (stack->synchronized)
		LeaveCriticalSection(&stack->lock);
}

/**
 * Removes and returns the object at the top of the Stack.
 */

void* Stack_Pop(wStack* stack)
{
	void* obj = NULL;

	WINPR_ASSERT(stack);
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

	WINPR_ASSERT(stack);
	if (stack->synchronized)
		EnterCriticalSection(&stack->lock);

	if (stack->size > 0)
		obj = stack->array[stack->size - 1];

	if (stack->synchronized)
		LeaveCriticalSection(&stack->lock);

	return obj;
}

static BOOL default_stack_equals(const void* obj1, const void* obj2)
{
	return (obj1 == obj2);
}

/**
 * Construction, Destruction
 */

wStack* Stack_New(BOOL synchronized)
{
	wStack* stack = NULL;
	stack = (wStack*)calloc(1, sizeof(wStack));

	if (!stack)
		return NULL;

	stack->object.fnObjectEquals = default_stack_equals;
	stack->synchronized = synchronized;
	stack->capacity = 32;
	stack->array = (void**)calloc(stack->capacity, sizeof(void*));

	if (!stack->array)
		goto out_free;

	if (stack->synchronized && !InitializeCriticalSectionAndSpinCount(&stack->lock, 4000))
		goto out_free;

	return stack;
out_free:
	Stack_Free(stack);
	return NULL;
}

void Stack_Free(wStack* stack)
{
	if (stack)
	{
		if (stack->synchronized)
			DeleteCriticalSection(&stack->lock);

		free(stack->array);
		free(stack);
	}
}

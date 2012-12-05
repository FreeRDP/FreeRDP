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
	if (stack->synchronized)
	{

	}

	return 0;
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
	if (stack->synchronized)
	{

	}
}

/**
 * Determines whether an element is in the Stack.
 */

BOOL Stack_Contains(wStack* stack, void* obj)
{
	if (stack->synchronized)
	{

	}

	return FALSE;
}

/**
 * Inserts an object at the top of the Stack.
 */

void Stack_Push(wStack* stack, void* obj)
{
	if (stack->synchronized)
	{

	}
}

/**
 * Removes and returns the object at the top of the Stack.
 */

void* Stack_Pop(wStack* stack)
{
	if (stack->synchronized)
	{

	}

	return NULL;
}

/**
 * Returns the object at the top of the Stack without removing it.
 */

void* Stack_Peek(wStack* stack)
{
	if (stack->synchronized)
	{

	}

	return NULL;
}

/**
 * Construction, Destruction
 */

wStack* Stack_New(BOOL bSynchronized)
{
	wStack* stack = NULL;

	stack = (wStack*) malloc(sizeof(wStack));

	if (stack)
	{
		stack->synchronized = bSynchronized;
	}

	return stack;
}

void Stack_Free(wStack* stack)
{
	free(stack);
}

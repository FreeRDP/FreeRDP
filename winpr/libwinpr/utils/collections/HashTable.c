/**
 * WinPR: Windows Portable Runtime
 * System.Collections.Hashtable
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>

#include <winpr/collections.h>

/**
 * This implementation is based on the public domain
 * hash table implementation made by Keith Pomakis:
 *
 * http://www.pomakis.com/hashtable/hashtable.c
 * http://www.pomakis.com/hashtable/hashtable.h
 */

static int isProbablePrime(long oddNumber)
{
	long i;

	for (i = 3; i < 51; i += 2)
	{
		if (oddNumber == i)
			return 1;
		else if (oddNumber % i == 0)
			return 0;
	}

	return 1; /* maybe */
}

static long calculateIdealNumOfBuckets(wHashTable* table)
{
	long idealNumOfBuckets = table->numOfElements / ((long) table->idealRatio);

	if (idealNumOfBuckets < 5)
		idealNumOfBuckets = 5;
	else
		idealNumOfBuckets |= 0x01;

	while (!isProbablePrime(idealNumOfBuckets))
		idealNumOfBuckets += 2;

	return idealNumOfBuckets;
}

void HashTable_Rehash(wHashTable* table, long numOfBuckets)
{
	int index;
	long hashValue;
	wKeyValuePair* pair;
	wKeyValuePair* nextPair;
	wKeyValuePair** newBucketArray;

	if (numOfBuckets == 0)
		numOfBuckets = calculateIdealNumOfBuckets(table);

	if (numOfBuckets == table->numOfBuckets)
		return; /* already the right size! */

	newBucketArray = (wKeyValuePair**) malloc(numOfBuckets * sizeof(wKeyValuePair*));

	if (newBucketArray == NULL)
	{
		/* Couldn't allocate memory for the new array.  This isn't a fatal
		 * error; we just can't perform the rehash. */
		return;
	}

	for (index = 0; index < numOfBuckets; index++)
		newBucketArray[index] = NULL;

	for (index = 0; index < table->numOfBuckets; index++)
	{
		pair = table->bucketArray[index];

		while (pair != NULL)
		{
			nextPair = pair->next;
			hashValue = table->hashFunction(pair->key) % numOfBuckets;
			pair->next = newBucketArray[hashValue];
			newBucketArray[hashValue] = pair;
			pair = nextPair;
		}
	}

	free(table->bucketArray);
	table->bucketArray = newBucketArray;
	table->numOfBuckets = numOfBuckets;
}

void HashTable_SetIdealRatio(wHashTable* table, float idealRatio,
		float lowerRehashThreshold, float upperRehashThreshold)
{
	table->idealRatio = idealRatio;
	table->lowerRehashThreshold = lowerRehashThreshold;
	table->upperRehashThreshold = upperRehashThreshold;
}

unsigned long HashTable_StringHashFunction(void* key)
{
	int c;
	unsigned long hash = 5381;
	unsigned char* str = (unsigned char*) key;

	/* djb2 algorithm */
	while ((c = *str++) != '\0')
		hash = (hash * 33) + c;

	return hash;
}

wKeyValuePair* HashTable_Get(wHashTable* table, void* key)
{
	long hashValue = table->hashFunction(key) % table->numOfBuckets;
	wKeyValuePair* pair = table->bucketArray[hashValue];

	while (pair != NULL && table->keycmp(key, pair->key) != 0)
		pair = pair->next;

	return pair;
}

static int pointercmp(void* pointer1, void* pointer2)
{
	return (pointer1 != pointer2);
}

static unsigned long pointerHashFunction(void* pointer)
{
	return ((unsigned long) pointer) >> 4;
}

/**
 * C equivalent of the C# Hashtable Class:
 * http://msdn.microsoft.com/en-us/library/system.collections.hashtable.aspx
 */

/**
 * Properties
 */

/**
 * Gets the number of key/value pairs contained in the HashTable.
 */

int HashTable_Count(wHashTable* table)
{
	return table->numOfElements;
}

/**
 * Methods
 */

/**
 * Adds an element with the specified key and value into the HashTable.
 */

int HashTable_Add(wHashTable* table, void* key, void* value)
{
	int status = 0;
	long hashValue;
	wKeyValuePair* pair;
	wKeyValuePair* newPair;

	if (!key || !value)
		return -1;

	if (table->synchronized)
		EnterCriticalSection(&table->lock);

	hashValue = table->hashFunction(key) % table->numOfBuckets;
	pair = table->bucketArray[hashValue];

	while (pair != NULL && table->keycmp(key, pair->key) != 0)
		pair = pair->next;

	if (pair)
	{
		if (pair->key != key)
		{
			if (table->keyDeallocator)
				table->keyDeallocator((void*) pair->key);
			pair->key = key;
		}

		if (pair->value != value)
		{
			if (table->valueDeallocator)
				table->valueDeallocator(pair->value);
			pair->value = value;
		}
	}
	else
	{
		newPair = (wKeyValuePair*) malloc(sizeof(wKeyValuePair));

		if (!newPair)
		{
			status = -1;
		}
		else
		{
			newPair->key = key;
			newPair->value = value;
			newPair->next = table->bucketArray[hashValue];
			table->bucketArray[hashValue] = newPair;
			table->numOfElements++;

			if (table->upperRehashThreshold > table->idealRatio)
			{
				float elementToBucketRatio = (float) table->numOfElements / (float) table->numOfBuckets;

				if (elementToBucketRatio > table->upperRehashThreshold)
					HashTable_Rehash(table, 0);
			}
		}
	}

	if (table->synchronized)
		LeaveCriticalSection(&table->lock);

	return status;
}

/**
 * Removes the element with the specified key from the HashTable.
 */

BOOL HashTable_Remove(wHashTable* table, void* key)
{
	BOOL status = TRUE;
	long hashValue = table->hashFunction(key) % table->numOfBuckets;
	wKeyValuePair* pair = table->bucketArray[hashValue];
	wKeyValuePair* previousPair = NULL;

	if (table->synchronized)
		EnterCriticalSection(&table->lock);

	while (pair && table->keycmp(key, pair->key) != 0)
	{
		previousPair = pair;
		pair = pair->next;
	}

	if (!pair)
	{
		status = FALSE;
	}
	else
	{
		if (table->keyDeallocator)
			table->keyDeallocator((void*) pair->key);

		if (table->valueDeallocator)
			table->valueDeallocator(pair->value);

		if (previousPair)
			previousPair->next = pair->next;
		else
			table->bucketArray[hashValue] = pair->next;

		free(pair);

		table->numOfElements--;

		if (table->lowerRehashThreshold > 0.0)
		{
			float elementToBucketRatio = (float) table->numOfElements / (float) table->numOfBuckets;

			if (elementToBucketRatio < table->lowerRehashThreshold)
				HashTable_Rehash(table, 0);
		}
	}

	if (table->synchronized)
		LeaveCriticalSection(&table->lock);

	return status;
}

/**
 * Get an item value using key
 */

void* HashTable_GetItemValue(wHashTable* table, void* key)
{
	void* value = NULL;
	wKeyValuePair* pair;

	if (table->synchronized)
		EnterCriticalSection(&table->lock);

	pair = HashTable_Get(table, key);

	if (pair)
		value = pair->value;

	if (table->synchronized)
		LeaveCriticalSection(&table->lock);

	return value;
}

/**
 * Set an item value using key
 */

BOOL HashTable_SetItemValue(wHashTable* table, void* key, void* value)
{
	BOOL status = TRUE;
	wKeyValuePair* pair;

	if (table->synchronized)
		EnterCriticalSection(&table->lock);

	pair = HashTable_Get(table, key);

	if (!pair)
		status = FALSE;
	else
		pair->value = value;

	if (table->synchronized)
		LeaveCriticalSection(&table->lock);

	return status;
}

/**
 * Removes all elements from the HashTable.
 */

void HashTable_Clear(wHashTable* table)
{
	int index;
	wKeyValuePair* pair;
	wKeyValuePair* nextPair;

	if (table->synchronized)
		EnterCriticalSection(&table->lock);

	for (index = 0; index < table->numOfBuckets; index++)
	{
		pair = table->bucketArray[index];

		while (pair)
		{
			nextPair = pair->next;

			if (table->pfnKeyValueFree)
				table->pfnKeyValueFree(table->context, pair->key, pair->value);

			if (table->keyDeallocator)
				table->keyDeallocator((void*) pair->key);

			if (table->valueDeallocator)
				table->valueDeallocator(pair->value);

			free(pair);

			pair = nextPair;
		}

		table->bucketArray[index] = NULL;
	}

	table->numOfElements = 0;
	HashTable_Rehash(table, 5);

	if (table->synchronized)
		LeaveCriticalSection(&table->lock);
}

/**
 * Gets the list of keys as an array
 */

int HashTable_GetKeys(wHashTable* table, ULONG_PTR** ppKeys)
{
	int iKey;
	int count;
	int index;
	ULONG_PTR* pKeys;
	wKeyValuePair* pair;
	wKeyValuePair* nextPair;

	if (table->synchronized)
		EnterCriticalSection(&table->lock);

	iKey = 0;
	count = table->numOfElements;
	pKeys = (ULONG_PTR*) calloc(count, sizeof(ULONG_PTR));

	for (index = 0; index < table->numOfBuckets; index++)
	{
		pair = table->bucketArray[index];

		while (pair)
		{
			nextPair = pair->next;

			pKeys[iKey++] = (ULONG_PTR) pair->key;

			pair = nextPair;
		}
	}

	if (table->synchronized)
		LeaveCriticalSection(&table->lock);

	*ppKeys = pKeys;

	return count;
}

/**
 * Determines whether the HashTable contains a specific key.
 */

BOOL HashTable_Contains(wHashTable* table, void* key)
{
	BOOL status;

	if (table->synchronized)
		EnterCriticalSection(&table->lock);

	status = (HashTable_Get(table, key) != NULL) ? TRUE : FALSE;

	if (table->synchronized)
		LeaveCriticalSection(&table->lock);

	return status;
}

/**
 * Determines whether the HashTable contains a specific key.
 */

BOOL HashTable_ContainsKey(wHashTable* table, void* key)
{
	BOOL status;

	if (table->synchronized)
		EnterCriticalSection(&table->lock);

	status = (HashTable_Get(table, key) != NULL) ? TRUE : FALSE;

	if (table->synchronized)
		LeaveCriticalSection(&table->lock);

	return status;
}

/**
 * Determines whether the HashTable contains a specific value.
 */

BOOL HashTable_ContainsValue(wHashTable* table, void* value)
{
	int index;
	BOOL status = FALSE;
	wKeyValuePair* pair;

	if (table->synchronized)
		EnterCriticalSection(&table->lock);

	for (index = 0; index < table->numOfBuckets; index++)
	{
		pair = table->bucketArray[index];

		while (pair)
		{
			if (table->valuecmp(value, pair->value) == 0)
			{
				status = TRUE;
				break;
			}

			pair = pair->next;
		}

		if (status)
			break;
	}

	if (table->synchronized)
		LeaveCriticalSection(&table->lock);

	return status;
}

void HashTable_SetFreeFunction(wHashTable* table, void* context, KEY_VALUE_FREE_FN pfnKeyValueFree)
{
	table->context = context;
	table->pfnKeyValueFree = pfnKeyValueFree;
}

/**
 * Construction, Destruction
 */

wHashTable* HashTable_New(BOOL synchronized)
{
	int index;
	wHashTable* table;

	table = (wHashTable*) calloc(1, sizeof(wHashTable));

	if (table)
	{
		table->synchronized = synchronized;
		InitializeCriticalSectionAndSpinCount(&(table->lock), 4000);

		table->numOfBuckets = 64;
		table->numOfElements = 0;

		table->bucketArray = (wKeyValuePair**) malloc(table->numOfBuckets * sizeof(wKeyValuePair*));

		if (!table->bucketArray)
		{
			free(table);
			return NULL;
		}

		for (index = 0; index < table->numOfBuckets; index++)
			table->bucketArray[index] = NULL;

		table->idealRatio = 3.0;
		table->lowerRehashThreshold = 0.0;
		table->upperRehashThreshold = 15.0;

		table->keycmp = pointercmp;
		table->valuecmp = pointercmp;
		table->hashFunction = pointerHashFunction;
		table->keyDeallocator = NULL;
		table->valueDeallocator = NULL;
	}

	return table;
}

void HashTable_Free(wHashTable* table)
{
	int index;
	wKeyValuePair* pair;
	wKeyValuePair* nextPair;

	if (table)
	{
		for (index = 0; index < table->numOfBuckets; index++)
		{
			pair = table->bucketArray[index];

			while (pair)
			{
				nextPair = pair->next;

				if (table->keyDeallocator)
					table->keyDeallocator((void*) pair->key);

				if (table->valueDeallocator)
					table->valueDeallocator(pair->value);

				free(pair);

				pair = nextPair;
			}
		}

		DeleteCriticalSection(&(table->lock));

		free(table->bucketArray);
		free(table);
	}
}

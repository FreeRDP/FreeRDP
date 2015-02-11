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

BOOL HashTable_PointerCompare(void* pointer1, void* pointer2)
{
	return (pointer1 == pointer2);
}

UINT32 HashTable_PointerHash(void* pointer)
{
	return ((UINT32) (UINT_PTR) pointer) >> 4;
}

BOOL HashTable_StringCompare(void* string1, void* string2)
{
	if (!string1 || !string2)
		return (string1 == string2);

	return (strcmp((char*) string1, (char*) string2) == 0);
}

UINT32 HashTable_StringHash(void* key)
{
	UINT32 c;
	UINT32 hash = 5381;
	BYTE* str = (BYTE*) key;

	/* djb2 algorithm */
	while ((c = *str++) != '\0')
		hash = (hash * 33) + c;

	return hash;
}

void* HashTable_StringClone(void* str)
{
	return _strdup((char*) str);
}

void HashTable_StringFree(void* str)
{
	free(str);
}

static int HashTable_IsProbablePrime(int oddNumber)
{
	int i;

	for (i = 3; i < 51; i += 2)
	{
		if (oddNumber == i)
			return 1;
		else if (oddNumber % i == 0)
			return 0;
	}

	return 1; /* maybe */
}

static long HashTable_CalculateIdealNumOfBuckets(wHashTable* table)
{
	int idealNumOfBuckets = table->numOfElements / ((int) table->idealRatio);

	if (idealNumOfBuckets < 5)
		idealNumOfBuckets = 5;
	else
		idealNumOfBuckets |= 0x01;

	while (!HashTable_IsProbablePrime(idealNumOfBuckets))
		idealNumOfBuckets += 2;

	return idealNumOfBuckets;
}

void HashTable_Rehash(wHashTable* table, int numOfBuckets)
{
	int index;
	UINT32 hashValue;
	wKeyValuePair* pair;
	wKeyValuePair* nextPair;
	wKeyValuePair** newBucketArray;

	if (numOfBuckets == 0)
		numOfBuckets = HashTable_CalculateIdealNumOfBuckets(table);

	if (numOfBuckets == table->numOfBuckets)
		return; /* already the right size! */

	newBucketArray = (wKeyValuePair**) calloc(numOfBuckets, sizeof(wKeyValuePair*));

	if (!newBucketArray)
	{
		/*
		 * Couldn't allocate memory for the new array.
		 * This isn't a fatal error; we just can't perform the rehash.
		 */
		return;
	}

	for (index = 0; index < table->numOfBuckets; index++)
	{
		pair = table->bucketArray[index];

		while (pair)
		{
			nextPair = pair->next;
			hashValue = table->hash(pair->key) % numOfBuckets;
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

wKeyValuePair* HashTable_Get(wHashTable* table, void* key)
{
	UINT32 hashValue;
	wKeyValuePair* pair;

	hashValue = table->hash(key) % table->numOfBuckets;

	pair = table->bucketArray[hashValue];

	while (pair && !table->keyCompare(key, pair->key))
		pair = pair->next;

	return pair;
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
	UINT32 hashValue;
	wKeyValuePair* pair;
	wKeyValuePair* newPair;

	if (!key || !value)
		return -1;

	if (table->keyClone)
	{
		key = table->keyClone(key);

		if (!key)
			return -1;
	}

	if (table->valueClone)
	{
		value = table->valueClone(value);

		if (!value)
			return -1;
	}

	if (table->synchronized)
		EnterCriticalSection(&table->lock);

	hashValue = table->hash(key) % table->numOfBuckets;
	pair = table->bucketArray[hashValue];

	while (pair && !table->keyCompare(key, pair->key))
		pair = pair->next;

	if (pair)
	{
		if (pair->key != key)
		{
			if (table->keyFree)
				table->keyFree(pair->key);
			pair->key = key;
		}

		if (pair->value != value)
		{
			if (table->valueFree)
				table->valueFree(pair->value);
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
	UINT32 hashValue;
	BOOL status = TRUE;
	wKeyValuePair* pair = NULL;
	wKeyValuePair* previousPair = NULL;

	if (table->synchronized)
		EnterCriticalSection(&table->lock);

	hashValue = table->hash(key) % table->numOfBuckets;

	pair = table->bucketArray[hashValue];

	while (pair && !table->keyCompare(key, pair->key))
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
		if (table->keyFree)
			table->keyFree(pair->key);

		if (table->valueFree)
			table->valueFree(pair->value);

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

	if (table->valueClone && value)
	{
		value = table->valueClone(value);

		if (!value)
			return FALSE;
	}

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

			if (table->keyFree)
				table->keyFree(pair->key);

			if (table->valueFree)
				table->valueFree(pair->value);

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

	if (count < 1)
	{
		if (table->synchronized)
			LeaveCriticalSection(&table->lock);

		return 0;
	}

	pKeys = (ULONG_PTR*) calloc(count, sizeof(ULONG_PTR));

	if (!pKeys)
	{
		if (table->synchronized)
			LeaveCriticalSection(&table->lock);

		return -1;
	}

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
			if (table->valueCompare(value, pair->value))
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

/**
 * Construction, Destruction
 */

wHashTable* HashTable_New(BOOL synchronized)
{
	wHashTable* table;

	table = (wHashTable*) calloc(1, sizeof(wHashTable));

	if (table)
	{
		table->synchronized = synchronized;

		InitializeCriticalSectionAndSpinCount(&(table->lock), 4000);

		table->numOfBuckets = 64;
		table->numOfElements = 0;

		table->bucketArray = (wKeyValuePair**) calloc(table->numOfBuckets, sizeof(wKeyValuePair*));

		if (!table->bucketArray)
		{
			free(table);
			return NULL;
		}

		table->idealRatio = 3.0;
		table->lowerRehashThreshold = 0.0;
		table->upperRehashThreshold = 15.0;

		table->hash = HashTable_PointerHash;
		table->keyCompare = HashTable_PointerCompare;
		table->valueCompare = HashTable_PointerCompare;
		table->keyClone = NULL;
		table->valueClone = NULL;
		table->keyFree = NULL;
		table->valueFree = NULL;
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

				if (table->keyFree)
					table->keyFree(pair->key);

				if (table->valueFree)
					table->valueFree(pair->value);

				free(pair);

				pair = nextPair;
			}
		}

		DeleteCriticalSection(&(table->lock));

		free(table->bucketArray);
		free(table);
	}
}

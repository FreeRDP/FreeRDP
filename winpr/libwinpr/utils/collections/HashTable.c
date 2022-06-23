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

#include <winpr/config.h>

#include <winpr/crt.h>
#include <winpr/assert.h>

#include <winpr/collections.h>

/**
 * This implementation is based on the public domain
 * hash table implementation made by Keith Pomakis:
 *
 * http://www.pomakis.com/hashtable/hashtable.c
 * http://www.pomakis.com/hashtable/hashtable.h
 */

typedef struct s_wKeyValuePair wKeyValuePair;

struct s_wKeyValuePair
{
	void* key;
	void* value;

	wKeyValuePair* next;
	BOOL markedForRemove;
};

struct s_wHashTable
{
	BOOL synchronized;
	CRITICAL_SECTION lock;

	size_t numOfBuckets;
	size_t numOfElements;
	float idealRatio;
	float lowerRehashThreshold;
	float upperRehashThreshold;
	wKeyValuePair** bucketArray;

	HASH_TABLE_HASH_FN hash;
	wObject key;
	wObject value;

	DWORD foreachRecursionLevel;
	DWORD pendingRemoves;
};

BOOL HashTable_PointerCompare(const void* pointer1, const void* pointer2)
{
	return (pointer1 == pointer2);
}

UINT32 HashTable_PointerHash(const void* pointer)
{
	return ((UINT32)(UINT_PTR)pointer) >> 4;
}

BOOL HashTable_StringCompare(const void* string1, const void* string2)
{
	if (!string1 || !string2)
		return (string1 == string2);

	return (strcmp((const char*)string1, (const char*)string2) == 0);
}

UINT32 HashTable_StringHash(const void* key)
{
	UINT32 c;
	UINT32 hash = 5381;
	const BYTE* str = (const BYTE*)key;

	/* djb2 algorithm */
	while ((c = *str++) != '\0')
		hash = (hash * 33) + c;

	return hash;
}

void* HashTable_StringClone(const void* str)
{
	return _strdup((const char*)str);
}

void HashTable_StringFree(void* str)
{
	free(str);
}

static INLINE BOOL HashTable_IsProbablePrime(size_t oddNumber)
{
	size_t i;

	for (i = 3; i < 51; i += 2)
	{
		if (oddNumber == i)
			return TRUE;
		else if (oddNumber % i == 0)
			return FALSE;
	}

	return TRUE; /* maybe */
}

static INLINE size_t HashTable_CalculateIdealNumOfBuckets(wHashTable* table)
{
	float tmp;
	size_t idealNumOfBuckets;

	WINPR_ASSERT(table);

	tmp = (table->numOfElements / table->idealRatio);
	idealNumOfBuckets = (size_t)tmp;

	if (idealNumOfBuckets < 5)
		idealNumOfBuckets = 5;
	else
		idealNumOfBuckets |= 0x01;

	while (!HashTable_IsProbablePrime(idealNumOfBuckets))
		idealNumOfBuckets += 2;

	return idealNumOfBuckets;
}

static INLINE void HashTable_Rehash(wHashTable* table, size_t numOfBuckets)
{
	size_t index;
	UINT32 hashValue;
	wKeyValuePair* pair;
	wKeyValuePair* nextPair;
	wKeyValuePair** newBucketArray;

	WINPR_ASSERT(table);
	if (numOfBuckets == 0)
		numOfBuckets = HashTable_CalculateIdealNumOfBuckets(table);

	if (numOfBuckets == table->numOfBuckets)
		return; /* already the right size! */

	newBucketArray = (wKeyValuePair**)calloc(numOfBuckets, sizeof(wKeyValuePair*));

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

static INLINE BOOL HashTable_Equals(wHashTable* table, const wKeyValuePair* pair, const void* key)
{
	WINPR_ASSERT(table);
	WINPR_ASSERT(pair);
	WINPR_ASSERT(key);
	return table->key.fnObjectEquals(key, pair->key);
}

static INLINE wKeyValuePair* HashTable_Get(wHashTable* table, const void* key)
{
	UINT32 hashValue;
	wKeyValuePair* pair;

	WINPR_ASSERT(table);
	if (!key)
		return NULL;

	hashValue = table->hash(key) % table->numOfBuckets;
	pair = table->bucketArray[hashValue];

	while (pair && !HashTable_Equals(table, pair, key))
		pair = pair->next;

	return pair;
}

static INLINE void disposeKey(wHashTable* table, void* key)
{
	WINPR_ASSERT(table);
	if (table->key.fnObjectFree)
		table->key.fnObjectFree(key);
}

static INLINE void disposeValue(wHashTable* table, void* value)
{
	WINPR_ASSERT(table);
	if (table->value.fnObjectFree)
		table->value.fnObjectFree(value);
}

static INLINE void disposePair(wHashTable* table, wKeyValuePair* pair)
{
	WINPR_ASSERT(table);
	if (!pair)
		return;
	disposeKey(table, pair->key);
	disposeValue(table, pair->value);
	free(pair);
}

static INLINE void setKey(wHashTable* table, wKeyValuePair* pair, const void* key)
{
	WINPR_ASSERT(table);
	if (!pair)
		return;
	disposeKey(table, pair->key);
	if (table->key.fnObjectNew)
		pair->key = table->key.fnObjectNew(key);
	else
	{
		union
		{
			const void* cpv;
			void* pv;
		} cnv;
		cnv.cpv = key;
		pair->key = cnv.pv;
	}
}

static INLINE void setValue(wHashTable* table, wKeyValuePair* pair, const void* value)
{
	WINPR_ASSERT(table);
	if (!pair)
		return;
	disposeValue(table, pair->value);
	if (table->value.fnObjectNew)
		pair->value = table->value.fnObjectNew(value);
	else
	{
		union
		{
			const void* cpv;
			void* pv;
		} cnv;
		cnv.cpv = value;
		pair->value = cnv.pv;
	}
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

size_t HashTable_Count(wHashTable* table)
{
	WINPR_ASSERT(table);
	return table->numOfElements;
}

/**
 * Methods
 */

/**
 * Adds an element with the specified key and value into the HashTable.
 */
#if defined(WITH_WINPR_DEPRECATED)
int HashTable_Add(wHashTable* table, const void* key, const void* value)
{
	if (!HashTable_Insert(table, key, value))
		return -1;
	return 0;
}
#endif

BOOL HashTable_Insert(wHashTable* table, const void* key, const void* value)
{
	BOOL rc = FALSE;
	UINT32 hashValue;
	wKeyValuePair* pair;
	wKeyValuePair* newPair;

	WINPR_ASSERT(table);
	if (!key || !value)
		return FALSE;

	if (table->synchronized)
		EnterCriticalSection(&table->lock);

	hashValue = table->hash(key) % table->numOfBuckets;
	pair = table->bucketArray[hashValue];

	while (pair && !HashTable_Equals(table, pair, key))
		pair = pair->next;

	if (pair)
	{
		if (pair->markedForRemove)
		{
			/* this entry was set to be removed but will be recycled instead */
			table->pendingRemoves--;
			pair->markedForRemove = FALSE;
			table->numOfElements++;
		}

		if (pair->key != key)
		{
			setKey(table, pair, key);
		}

		if (pair->value != value)
		{
			setValue(table, pair, value);
		}
		rc = TRUE;
	}
	else
	{
		newPair = (wKeyValuePair*)calloc(1, sizeof(wKeyValuePair));

		if (newPair)
		{
			setKey(table, newPair, key);
			setValue(table, newPair, value);
			newPair->next = table->bucketArray[hashValue];
			newPair->markedForRemove = FALSE;
			table->bucketArray[hashValue] = newPair;
			table->numOfElements++;

			if (!table->foreachRecursionLevel && table->upperRehashThreshold > table->idealRatio)
			{
				float elementToBucketRatio =
				    (float)table->numOfElements / (float)table->numOfBuckets;

				if (elementToBucketRatio > table->upperRehashThreshold)
					HashTable_Rehash(table, 0);
			}
			rc = TRUE;
		}
	}

	if (table->synchronized)
		LeaveCriticalSection(&table->lock);

	return rc;
}

/**
 * Removes the element with the specified key from the HashTable.
 */

BOOL HashTable_Remove(wHashTable* table, const void* key)
{
	UINT32 hashValue;
	BOOL status = TRUE;
	wKeyValuePair* pair = NULL;
	wKeyValuePair* previousPair = NULL;

	WINPR_ASSERT(table);
	if (!key)
		return FALSE;

	if (table->synchronized)
		EnterCriticalSection(&table->lock);

	hashValue = table->hash(key) % table->numOfBuckets;
	pair = table->bucketArray[hashValue];

	while (pair && !HashTable_Equals(table, pair, key))
	{
		previousPair = pair;
		pair = pair->next;
	}

	if (!pair)
	{
		status = FALSE;
		goto out;
	}

	if (table->foreachRecursionLevel)
	{
		/* if we are running a HashTable_Foreach, just mark the entry for removal */
		pair->markedForRemove = TRUE;
		table->pendingRemoves++;
		table->numOfElements--;
		goto out;
	}

	if (previousPair)
		previousPair->next = pair->next;
	else
		table->bucketArray[hashValue] = pair->next;

	disposePair(table, pair);
	table->numOfElements--;

	if (!table->foreachRecursionLevel && table->lowerRehashThreshold > 0.0f)
	{
		float elementToBucketRatio = (float)table->numOfElements / (float)table->numOfBuckets;

		if (elementToBucketRatio < table->lowerRehashThreshold)
			HashTable_Rehash(table, 0);
	}

out:
	if (table->synchronized)
		LeaveCriticalSection(&table->lock);

	return status;
}

/**
 * Get an item value using key
 */

void* HashTable_GetItemValue(wHashTable* table, const void* key)
{
	void* value = NULL;
	wKeyValuePair* pair;

	WINPR_ASSERT(table);
	if (!key)
		return NULL;

	if (table->synchronized)
		EnterCriticalSection(&table->lock);

	pair = HashTable_Get(table, key);

	if (pair && !pair->markedForRemove)
		value = pair->value;

	if (table->synchronized)
		LeaveCriticalSection(&table->lock);

	return value;
}

/**
 * Set an item value using key
 */

BOOL HashTable_SetItemValue(wHashTable* table, const void* key, const void* value)
{
	BOOL status = TRUE;
	wKeyValuePair* pair;

	WINPR_ASSERT(table);
	if (!key)
		return FALSE;

	if (table->synchronized)
		EnterCriticalSection(&table->lock);

	pair = HashTable_Get(table, key);

	if (!pair || pair->markedForRemove)
		status = FALSE;
	else
	{
		setValue(table, pair, value);
	}

	if (table->synchronized)
		LeaveCriticalSection(&table->lock);

	return status;
}

/**
 * Removes all elements from the HashTable.
 */

void HashTable_Clear(wHashTable* table)
{
	size_t index;
	wKeyValuePair* pair;
	wKeyValuePair* nextPair;

	WINPR_ASSERT(table);

	if (table->synchronized)
		EnterCriticalSection(&table->lock);

	for (index = 0; index < table->numOfBuckets; index++)
	{
		pair = table->bucketArray[index];

		while (pair)
		{
			nextPair = pair->next;

			if (table->foreachRecursionLevel)
			{
				/* if we're in a foreach we just mark the entry for removal */
				pair->markedForRemove = TRUE;
				table->pendingRemoves++;
			}
			else
			{
				disposePair(table, pair);
				pair = nextPair;
			}
		}

		table->bucketArray[index] = NULL;
	}

	table->numOfElements = 0;
	if (table->foreachRecursionLevel == 0)
		HashTable_Rehash(table, 5);

	if (table->synchronized)
		LeaveCriticalSection(&table->lock);
}

/**
 * Gets the list of keys as an array
 */

size_t HashTable_GetKeys(wHashTable* table, ULONG_PTR** ppKeys)
{
	size_t iKey;
	size_t count;
	size_t index;
	ULONG_PTR* pKeys;
	wKeyValuePair* pair;
	wKeyValuePair* nextPair;

	WINPR_ASSERT(table);

	if (table->synchronized)
		EnterCriticalSection(&table->lock);

	iKey = 0;
	count = table->numOfElements;
	*ppKeys = NULL;

	if (count < 1)
	{
		if (table->synchronized)
			LeaveCriticalSection(&table->lock);

		return 0;
	}

	pKeys = (ULONG_PTR*)calloc(count, sizeof(ULONG_PTR));

	if (!pKeys)
	{
		if (table->synchronized)
			LeaveCriticalSection(&table->lock);

		return 0;
	}

	for (index = 0; index < table->numOfBuckets; index++)
	{
		pair = table->bucketArray[index];

		while (pair)
		{
			nextPair = pair->next;
			if (!pair->markedForRemove)
				pKeys[iKey++] = (ULONG_PTR)pair->key;
			pair = nextPair;
		}
	}

	if (table->synchronized)
		LeaveCriticalSection(&table->lock);

	if (ppKeys)
		*ppKeys = pKeys;
	else
		free(pKeys);
	return count;
}

BOOL HashTable_Foreach(wHashTable* table, HASH_TABLE_FOREACH_FN fn, VOID* arg)
{
	BOOL ret = TRUE;
	size_t index;
	wKeyValuePair* pair;

	WINPR_ASSERT(table);
	WINPR_ASSERT(fn);

	if (table->synchronized)
		EnterCriticalSection(&table->lock);

	table->foreachRecursionLevel++;
	for (index = 0; index < table->numOfBuckets; index++)
	{
		for (pair = table->bucketArray[index]; pair; pair = pair->next)
		{
			if (!pair->markedForRemove && !fn(pair->key, pair->value, arg))
			{
				ret = FALSE;
				goto out;
			}
		}
	}
	table->foreachRecursionLevel--;

	if (!table->foreachRecursionLevel && table->pendingRemoves)
	{
		/* if we're the last recursive foreach call, let's do the cleanup if needed */
		wKeyValuePair** prevPtr;
		for (index = 0; index < table->numOfBuckets; index++)
		{
			wKeyValuePair* nextPair;
			prevPtr = &table->bucketArray[index];
			for (pair = table->bucketArray[index]; pair;)
			{
				nextPair = pair->next;

				if (pair->markedForRemove)
				{
					disposePair(table, pair);
					*prevPtr = nextPair;
				}
				else
				{
					prevPtr = &pair->next;
				}
				pair = nextPair;
			}
		}
		table->pendingRemoves = 0;
	}

out:
	if (table->synchronized)
		LeaveCriticalSection(&table->lock);
	return ret;
}

/**
 * Determines whether the HashTable contains a specific key.
 */

BOOL HashTable_Contains(wHashTable* table, const void* key)
{
	BOOL status;
	wKeyValuePair* pair;

	WINPR_ASSERT(table);
	if (!key)
		return FALSE;

	if (table->synchronized)
		EnterCriticalSection(&table->lock);

	pair = HashTable_Get(table, key);
	status = (pair && !pair->markedForRemove);

	if (table->synchronized)
		LeaveCriticalSection(&table->lock);

	return status;
}

/**
 * Determines whether the HashTable contains a specific key.
 */

BOOL HashTable_ContainsKey(wHashTable* table, const void* key)
{
	BOOL status;
	wKeyValuePair* pair;

	WINPR_ASSERT(table);
	if (!key)
		return FALSE;

	if (table->synchronized)
		EnterCriticalSection(&table->lock);

	pair = HashTable_Get(table, key);
	status = (pair && !pair->markedForRemove);

	if (table->synchronized)
		LeaveCriticalSection(&table->lock);

	return status;
}

/**
 * Determines whether the HashTable contains a specific value.
 */

BOOL HashTable_ContainsValue(wHashTable* table, const void* value)
{
	size_t index;
	BOOL status = FALSE;
	wKeyValuePair* pair;

	WINPR_ASSERT(table);
	if (!value)
		return FALSE;

	if (table->synchronized)
		EnterCriticalSection(&table->lock);

	for (index = 0; index < table->numOfBuckets; index++)
	{
		pair = table->bucketArray[index];

		while (pair)
		{
			if (!pair->markedForRemove && HashTable_Equals(table, pair, value))
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
	wHashTable* table = (wHashTable*)calloc(1, sizeof(wHashTable));

	if (!table)
		goto fail;

	table->synchronized = synchronized;
	InitializeCriticalSectionAndSpinCount(&(table->lock), 4000);
	table->numOfBuckets = 64;
	table->numOfElements = 0;
	table->bucketArray = (wKeyValuePair**)calloc(table->numOfBuckets, sizeof(wKeyValuePair*));

	if (!table->bucketArray)
		goto fail;

	table->idealRatio = 3.0;
	table->lowerRehashThreshold = 0.0;
	table->upperRehashThreshold = 15.0;
	table->hash = HashTable_PointerHash;
	table->key.fnObjectEquals = HashTable_PointerCompare;
	table->value.fnObjectEquals = HashTable_PointerCompare;

	return table;
fail:
	HashTable_Free(table);
	return NULL;
}

void HashTable_Free(wHashTable* table)
{
	size_t index;
	wKeyValuePair* pair;
	wKeyValuePair* nextPair;

	if (!table)
		return;

	if (table->bucketArray)
	{
		for (index = 0; index < table->numOfBuckets; index++)
		{
			pair = table->bucketArray[index];

			while (pair)
			{
				nextPair = pair->next;

				disposePair(table, pair);
				pair = nextPair;
			}
		}
		free(table->bucketArray);
	}
	DeleteCriticalSection(&(table->lock));

	free(table);
}

wObject* HashTable_KeyObject(wHashTable* table)
{
	WINPR_ASSERT(table);
	return &table->key;
}

wObject* HashTable_ValueObject(wHashTable* table)
{
	WINPR_ASSERT(table);
	return &table->value;
}

BOOL HashTable_SetHashFunction(wHashTable* table, HASH_TABLE_HASH_FN fn)
{
	WINPR_ASSERT(table);
	table->hash = fn;
	return fn != NULL;
}

BOOL HashTable_SetupForStringData(wHashTable* table, BOOL stringValues)
{
	wObject* obj;

	if (!HashTable_SetHashFunction(table, HashTable_StringHash))
		return FALSE;

	obj = HashTable_KeyObject(table);
	obj->fnObjectEquals = HashTable_StringCompare;
	obj->fnObjectNew = HashTable_StringClone;
	obj->fnObjectFree = HashTable_StringFree;

	if (stringValues)
	{
		obj = HashTable_ValueObject(table);
		obj->fnObjectEquals = HashTable_StringCompare;
		obj->fnObjectNew = HashTable_StringClone;
		obj->fnObjectFree = HashTable_StringFree;
	}
	return TRUE;
}

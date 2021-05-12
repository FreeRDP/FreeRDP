
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/collections.h>

static char* key1 = "key1";
static char* key2 = "key2";
static char* key3 = "key3";

static char* val1 = "val1";
static char* val2 = "val2";
static char* val3 = "val3";

static int test_hash_table_pointer(void)
{
	int rc = -1;
	int count;
	char* value;
	wHashTable* table;
	table = HashTable_New(TRUE);

	if (!table)
		return -1;

	HashTable_Add(table, key1, val1);
	HashTable_Add(table, key2, val2);
	HashTable_Add(table, key3, val3);
	count = HashTable_Count(table);

	if (count != 3)
	{
		printf("HashTable_Count: Expected : 3, Actual: %d\n", count);
		goto fail;
	}

	HashTable_Remove(table, key2);
	count = HashTable_Count(table);

	if (count != 2)
	{
		printf("HashTable_Count: Expected : 2, Actual: %d\n", count);
		goto fail;
	}

	HashTable_Remove(table, key3);
	count = HashTable_Count(table);

	if (count != 1)
	{
		printf("HashTable_Count: Expected : 1, Actual: %d\n", count);
		goto fail;
	}

	HashTable_Remove(table, key1);
	count = HashTable_Count(table);

	if (count != 0)
	{
		printf("HashTable_Count: Expected : 0, Actual: %d\n", count);
		goto fail;
	}

	HashTable_Add(table, key1, val1);
	HashTable_Add(table, key2, val2);
	HashTable_Add(table, key3, val3);
	count = HashTable_Count(table);

	if (count != 3)
	{
		printf("HashTable_Count: Expected : 3, Actual: %d\n", count);
		goto fail;
	}

	value = (char*)HashTable_GetItemValue(table, key1);

	if (strcmp(value, val1) != 0)
	{
		printf("HashTable_GetItemValue: Expected : %s, Actual: %s\n", val1, value);
		goto fail;
	}

	value = (char*)HashTable_GetItemValue(table, key2);

	if (strcmp(value, val2) != 0)
	{
		printf("HashTable_GetItemValue: Expected : %s, Actual: %s\n", val2, value);
		goto fail;
	}

	value = (char*)HashTable_GetItemValue(table, key3);

	if (strcmp(value, val3) != 0)
	{
		printf("HashTable_GetItemValue: Expected : %s, Actual: %s\n", val3, value);
		goto fail;
	}

	HashTable_SetItemValue(table, key2, "apple");
	value = (char*)HashTable_GetItemValue(table, key2);

	if (strcmp(value, "apple") != 0)
	{
		printf("HashTable_GetItemValue: Expected : %s, Actual: %s\n", "apple", value);
		goto fail;
	}

	if (!HashTable_Contains(table, key2))
	{
		printf("HashTable_Contains: Expected : TRUE, Actual: FALSE\n");
		goto fail;
	}

	if (!HashTable_Remove(table, key2))
	{
		printf("HashTable_Remove: Expected : TRUE, Actual: FALSE\n");
		goto fail;
	}

	if (HashTable_Remove(table, key2))
	{
		printf("HashTable_Remove: Expected : FALSE, Actual: TRUE\n");
		goto fail;
	}

	HashTable_Clear(table);
	count = HashTable_Count(table);

	if (count != 0)
	{
		printf("HashTable_Count: Expected : 0, Actual: %d\n", count);
		goto fail;
	}

	rc = 1;
fail:
	HashTable_Free(table);
	return rc;
}

static int test_hash_table_string(void)
{
	int rc = -1;
	int count;
	char* value;
	wHashTable* table;
	table = HashTable_New(TRUE);

	if (!table)
		return -1;

	table->hash = HashTable_StringHash;
	table->keyCompare = HashTable_StringCompare;
	table->valueCompare = HashTable_StringCompare;
	table->keyClone = HashTable_StringClone;
	table->valueClone = HashTable_StringClone;
	table->keyFree = HashTable_StringFree;
	table->valueFree = HashTable_StringFree;
	HashTable_Add(table, key1, val1);
	HashTable_Add(table, key2, val2);
	HashTable_Add(table, key3, val3);
	count = HashTable_Count(table);

	if (count != 3)
	{
		printf("HashTable_Count: Expected : 3, Actual: %d\n", count);
		goto fail;
	}

	HashTable_Remove(table, key2);
	count = HashTable_Count(table);

	if (count != 2)
	{
		printf("HashTable_Count: Expected : 3, Actual: %d\n", count);
		goto fail;
	}

	HashTable_Remove(table, key3);
	count = HashTable_Count(table);

	if (count != 1)
	{
		printf("HashTable_Count: Expected : 1, Actual: %d\n", count);
		goto fail;
	}

	HashTable_Remove(table, key1);
	count = HashTable_Count(table);

	if (count != 0)
	{
		printf("HashTable_Count: Expected : 0, Actual: %d\n", count);
		goto fail;
	}

	HashTable_Add(table, key1, val1);
	HashTable_Add(table, key2, val2);
	HashTable_Add(table, key3, val3);
	count = HashTable_Count(table);

	if (count != 3)
	{
		printf("HashTable_Count: Expected : 3, Actual: %d\n", count);
		goto fail;
	}

	value = (char*)HashTable_GetItemValue(table, key1);

	if (strcmp(value, val1) != 0)
	{
		printf("HashTable_GetItemValue: Expected : %s, Actual: %s\n", val1, value);
		goto fail;
	}

	value = (char*)HashTable_GetItemValue(table, key2);

	if (strcmp(value, val2) != 0)
	{
		printf("HashTable_GetItemValue: Expected : %s, Actual: %s\n", val2, value);
		goto fail;
	}

	value = (char*)HashTable_GetItemValue(table, key3);

	if (strcmp(value, val3) != 0)
	{
		printf("HashTable_GetItemValue: Expected : %s, Actual: %s\n", val3, value);
		goto fail;
	}

	HashTable_SetItemValue(table, key2, "apple");
	value = (char*)HashTable_GetItemValue(table, key2);

	if (strcmp(value, "apple") != 0)
	{
		printf("HashTable_GetItemValue: Expected : %s, Actual: %s\n", "apple", value);
		goto fail;
	}

	if (!HashTable_Contains(table, key2))
	{
		printf("HashTable_Contains: Expected : TRUE, Actual: FALSE\n");
		goto fail;
	}

	if (!HashTable_Remove(table, key2))
	{
		printf("HashTable_Remove: Expected : TRUE, Actual: FALSE\n");
		goto fail;
	}

	if (HashTable_Remove(table, key2))
	{
		printf("HashTable_Remove: Expected : FALSE, Actual: TRUE\n");
		goto fail;
	}

	HashTable_Clear(table);
	count = HashTable_Count(table);

	if (count != 0)
	{
		printf("HashTable_Count: Expected : 0, Actual: %d\n", count);
		goto fail;
	}

	rc = 1;
fail:
	HashTable_Free(table);
	return rc;
}

typedef struct
{
	wHashTable* table;
	int strlenCounter;
	int foreachCalls;

	BOOL test3error;
} ForeachData;

BOOL foreachFn1(const void* key, void* value, void* arg)
{
	ForeachData* d = (ForeachData*)arg;
	d->strlenCounter += strlen((const char*)value);
	return TRUE;
}

BOOL foreachFn2(const void* key, void* value, void* arg)
{
	ForeachData* d = (ForeachData*)arg;
	d->foreachCalls++;

	if (d->foreachCalls == 2)
		return FALSE;
	return TRUE;
}

BOOL foreachFn3(const void* key, void* value, void* arg)
{
	char* keyStr = (char*)key;

	ForeachData* d = (ForeachData*)arg;
	ForeachData d2;

	if (strcmp(keyStr, "key1") == 0)
	{
		/* when we pass on key1, let's remove key2 and check that the value is not
		 * visible anymore (even if has just been marked for removal)*/
		HashTable_Remove(d->table, "key2");

		if (HashTable_Contains(d->table, "key2"))
		{
			d->test3error = TRUE;
			return FALSE;
		}

		if (HashTable_ContainsValue(d->table, "value2"))
		{
			d->test3error = TRUE;
			return FALSE;
		}

		/* number of elements of the table shall be correct too */
		if (HashTable_Count(d->table) != 2)
		{
			d->test3error = TRUE;
			return FALSE;
		}

		/* we try recursive HashTable_Foreach */
		d2.table = d->table;
		d2.strlenCounter = 0;

		if (!HashTable_Foreach(d->table, foreachFn1, &d2))
		{
			d->test3error = TRUE;
			return FALSE;
		}
		if (d2.strlenCounter != 8)
		{
			d->test3error = TRUE;
			return FALSE;
		}
	}
	return TRUE;
}

int test_hash_foreach(void)
{
	ForeachData foreachData;
	wHashTable* table;
	int retCode = 0;

	foreachData.table = table = HashTable_New(TRUE);
	if (!table)
		return -1;

	table->hash = HashTable_StringHash;
	table->keyCompare = HashTable_StringCompare;
	table->valueCompare = HashTable_StringCompare;
	table->keyClone = HashTable_StringClone;
	table->valueClone = HashTable_StringClone;
	table->keyFree = HashTable_StringFree;
	table->valueFree = HashTable_StringFree;

	if (HashTable_Add(table, key1, val1) < 0 || HashTable_Add(table, key2, val2) < 0 ||
	    HashTable_Add(table, key3, val3) < 0)
	{
		retCode = -2;
		goto out;
	}

	/* let's try a first trivial foreach */
	foreachData.strlenCounter = 0;
	if (!HashTable_Foreach(table, foreachFn1, &foreachData))
	{
		retCode = -10;
		goto out;
	}
	if (foreachData.strlenCounter != 12)
	{
		retCode = -11;
		goto out;
	}

	/* interrupted foreach */
	foreachData.foreachCalls = 0;
	if (HashTable_Foreach(table, foreachFn2, &foreachData))
	{
		retCode = -20;
		goto out;
	}
	if (foreachData.foreachCalls != 2)
	{
		retCode = -21;
		goto out;
	}

	/* let's try a foreach() call that will remove a value from the table in the callback */
	foreachData.test3error = FALSE;
	if (!HashTable_Foreach(table, foreachFn3, &foreachData))
	{
		retCode = -30;
		goto out;
	}
	if (foreachData.test3error)
	{
		retCode = -31;
		goto out;
	}

out:
	HashTable_Free(table);
	return retCode;
}

int TestHashTable(int argc, char* argv[])
{
	if (test_hash_table_pointer() < 0)
		return 1;

	if (test_hash_table_string() < 0)
		return 2;

	if (test_hash_foreach() < 0)
		return 3;
	return 0;
}

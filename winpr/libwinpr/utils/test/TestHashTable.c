
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/collections.h>

static char* key1 = "key1";
static char* key2 = "key2";
static char* key3 = "key3";

static char* val1 = "val1";
static char* val2 = "val2";
static char* val3 = "val3";

int TestHashTable(int argc, char* argv[])
{
	int count;
	char* value;
	wHashTable* table;

	table = HashTable_New(TRUE);

	HashTable_Add(table, key1, val1);
	HashTable_Add(table, key2, val2);
	HashTable_Add(table, key3, val3);

	count = HashTable_Count(table);

	if (count != 3)
	{
		printf("HashTable_Count: Expected : %d, Actual: %d\n", 3, count);
		return -1;
	}

	HashTable_Remove(table, key2);

	count = HashTable_Count(table);

	if (count != 2)
	{
		printf("HashTable_Count: Expected : %d, Actual: %d\n", 2, count);
		return -1;
	}

	HashTable_Remove(table, key3);

	count = HashTable_Count(table);

	if (count != 1)
	{
		printf("HashTable_Count: Expected : %d, Actual: %d\n", 1, count);
		return -1;
	}

	HashTable_Remove(table, key1);

	count = HashTable_Count(table);

	if (count != 0)
	{
		printf("HashTable_Count: Expected : %d, Actual: %d\n", 0, count);
		return -1;
	}

	HashTable_Add(table, key1, val1);
	HashTable_Add(table, key2, val2);
	HashTable_Add(table, key3, val3);

	count = HashTable_Count(table);

	if (count != 3)
	{
		printf("HashTable_Count: Expected : %d, Actual: %d\n", 3, count);
		return -1;
	}

	value = (char*) HashTable_GetItemValue(table, key1);

	if (strcmp(value, val1) != 0)
	{
		printf("HashTable_GetItemValue: Expected : %s, Actual: %s\n", val1, value);
		return -1;
	}

	value = (char*) HashTable_GetItemValue(table, key2);

	if (strcmp(value, val2) != 0)
	{
		printf("HashTable_GetItemValue: Expected : %s, Actual: %s\n", val2, value);
		return -1;
	}

	value = (char*) HashTable_GetItemValue(table, key3);

	if (strcmp(value, val3) != 0)
	{
		printf("HashTable_GetItemValue: Expected : %s, Actual: %s\n", val3, value);
		return -1;
	}

	HashTable_SetItemValue(table, key2, "apple");

	value = (char*) HashTable_GetItemValue(table, key2);

	if (strcmp(value, "apple") != 0)
	{
		printf("HashTable_GetItemValue: Expected : %s, Actual: %s\n", "apple", value);
		return -1;
	}

	if (!HashTable_Contains(table, key2))
	{
		printf("HashTable_Contains: Expected : %d, Actual: %d\n", TRUE, FALSE);
		return -1;
	}

	if (!HashTable_Remove(table, key2))
	{
		printf("HashTable_Remove: Expected : %d, Actual: %d\n", TRUE, FALSE);
		return -1;
	}

	if (HashTable_Remove(table, key2))
	{
		printf("HashTable_Remove: Expected : %d, Actual: %d\n", FALSE, TRUE);
		return -1;
	}

	HashTable_Clear(table);

	count = HashTable_Count(table);

	if (count != 0)
	{
		printf("HashTable_Count: Expected : %d, Actual: %d\n", 0, count);
		return -1;
	}

	HashTable_Free(table);

	return 0;
}

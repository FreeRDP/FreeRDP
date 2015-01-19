
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/collections.h>

static char* key1 = "key1";
static char* key2 = "key2";
static char* key3 = "key3";

static char* val1 = "val1";
static char* val2 = "val2";
static char* val3 = "val3";

int TestListDictionary(int argc, char* argv[])
{
	int count;
	char* value;
	wListDictionary* list;

	list = ListDictionary_New(TRUE);

	ListDictionary_Add(list, key1, val1);
	ListDictionary_Add(list, key2, val2);
	ListDictionary_Add(list, key3, val3);

	count = ListDictionary_Count(list);

	if (count != 3)
	{
		printf("ListDictionary_Count: Expected : %d, Actual: %d\n", 3, count);
		return -1;
	}

	ListDictionary_Remove(list, key2);

	count = ListDictionary_Count(list);

	if (count != 2)
	{
		printf("ListDictionary_Count: Expected : %d, Actual: %d\n", 2, count);
		return -1;
	}

	ListDictionary_Remove(list, key3);

	count = ListDictionary_Count(list);

	if (count != 1)
	{
		printf("ListDictionary_Count: Expected : %d, Actual: %d\n", 1, count);
		return -1;
	}

	ListDictionary_Remove(list, key1);

	count = ListDictionary_Count(list);

	if (count != 0)
	{
		printf("ListDictionary_Count: Expected : %d, Actual: %d\n", 0, count);
		return -1;
	}

	ListDictionary_Add(list, key1, val1);
	ListDictionary_Add(list, key2, val2);
	ListDictionary_Add(list, key3, val3);

	count = ListDictionary_Count(list);

	if (count != 3)
	{
		printf("ListDictionary_Count: Expected : %d, Actual: %d\n", 3, count);
		return -1;
	}

	value = (char*) ListDictionary_GetItemValue(list, key1);

	if (strcmp(value, val1) != 0)
	{
		printf("ListDictionary_GetItemValue: Expected : %d, Actual: %d\n",
				(int) (size_t) val1, (int) (size_t) value);
		return -1;
	}

	value = (char*) ListDictionary_GetItemValue(list, key2);

	if (strcmp(value, val2) != 0)
	{
		printf("ListDictionary_GetItemValue: Expected : %d, Actual: %d\n",
				(int) (size_t) val2, (int) (size_t) value);
		return -1;
	}

	value = (char*) ListDictionary_GetItemValue(list, key3);

	if (strcmp(value, val3) != 0)
	{
		printf("ListDictionary_GetItemValue: Expected : %d, Actual: %d\n",
				(int) (size_t) val3, (int) (size_t) value);
		return -1;
	}

	ListDictionary_SetItemValue(list, key2, "apple");

	value = (char*) ListDictionary_GetItemValue(list, key2);

	if (strcmp(value, "apple") != 0)
	{
		printf("ListDictionary_GetItemValue: Expected : %s, Actual: %s\n", "apple", value);
		return -1;
	}

	if (!ListDictionary_Contains(list, key2))
	{
		printf("ListDictionary_Contains: Expected : %d, Actual: %d\n", TRUE, FALSE);
		return -1;
	}

	if (!ListDictionary_Remove(list, key2))
	{
		printf("ListDictionary_Remove: Expected : %d, Actual: %d\n", TRUE, FALSE);
		return -1;
	}

	if (ListDictionary_Remove(list, key2))
	{
		printf("ListDictionary_Remove: Expected : %d, Actual: %d\n", FALSE, TRUE);
		return -1;
	}

	value = ListDictionary_Remove_Head(list);
	count = ListDictionary_Count(list);
	if (strncmp(value, val1, 4) || count != 1)
	{
		printf("ListDictionary_Remove_Head: Expected : %s, Actual: %s Count: %d\n", val1, value, count);
		return -1;
	}

	value = ListDictionary_Remove_Head(list);
	count = ListDictionary_Count(list);
	if (strncmp(value, val3, 4) || count != 0)
	{
		printf("ListDictionary_Remove_Head: Expected : %s, Actual: %s Count: %d\n", val3, value, count);
		return -1;
	}

	value = ListDictionary_Remove_Head(list);
	if (value)
	{
		printf("ListDictionary_Remove_Head: Expected : (null), Actual: %s\n", value);
		return -1;
	}

	ListDictionary_Add(list, key1, val1);
	ListDictionary_Add(list, key2, val2);
	ListDictionary_Add(list, key3, val3);

	ListDictionary_Clear(list);

	count = ListDictionary_Count(list);

	if (count != 0)
	{
		printf("ListDictionary_Count: Expected : %d, Actual: %d\n", 0, count);
		return -1;
	}

	ListDictionary_Free(list);

	return 0;
}


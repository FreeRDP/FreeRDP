
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/collections.h>

int TestArrayList(int argc, char* argv[])
{
	int index;
	int count;
	size_t val;
	wArrayList* arrayList;
	const int elemsToInsert = 10;

	arrayList = ArrayList_New(TRUE);

	for (index = 0; index < elemsToInsert; index++)
	{
		ArrayList_Add(arrayList, (void*) (size_t) index);
	}

	count = ArrayList_Count(arrayList);

	printf("ArrayList count: %d\n", count);

	index = ArrayList_IndexOf(arrayList, (void*) (size_t) 6, -1, -1);

	printf("ArrayList index: %d\n", index);

	if (index != 6)
		return -1;

	ArrayList_Insert(arrayList, 5, (void*) (size_t) 100);

	index = ArrayList_IndexOf(arrayList, (void*) (size_t) 6, -1, -1);
	printf("ArrayList index: %d\n", index);

	if (index != 7)
		return -1;

	ArrayList_Remove(arrayList, (void*) (size_t) 100);

	index = ArrayList_IndexOf(arrayList, (void*) (size_t) 6, -1, -1);
	printf("ArrayList index: %d\n", index);

	if (index != 6)
		return -1;

	for (index = 0; index < elemsToInsert; index++) {
		val = (size_t)ArrayList_GetItem(arrayList, 0);
		ArrayList_RemoveAt(arrayList, 0);
		if (val != index)
		{
			printf("ArrayList: shifted %d entries, expected value %d, got %ld\n", index, index, (long int)val);
			return -1;
		}
	}

	index = ArrayList_IndexOf(arrayList, (void*) (size_t) elemsToInsert, -1, -1);
	printf("ArrayList index: %d\n", index);
	if (index != -1)
		return -1;

	count = ArrayList_Count(arrayList);
	printf("ArrayList count: %d\n", count);
	if (count != 0)
		return -1;

	ArrayList_Clear(arrayList);
	ArrayList_Free(arrayList);

	return 0;
}

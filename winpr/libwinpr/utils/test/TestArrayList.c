
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/collections.h>

int TestArrayList(int argc, char* argv[])
{
	SSIZE_T rc = 0;
	size_t val = 0;
	const size_t elemsToInsert = 10;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	wArrayList* arrayList = ArrayList_New(TRUE);
	if (!arrayList)
		return -1;

	for (size_t index = 0; index < elemsToInsert; index++)
	{
		if (!ArrayList_Append(arrayList, (void*)index))
			return -1;
	}

	size_t count = ArrayList_Count(arrayList);

	printf("ArrayList count: %" PRIuz "\n", count);

	SSIZE_T index = ArrayList_IndexOf(arrayList, (void*)(size_t)6, -1, -1);

	printf("ArrayList index: %" PRIdz "\n", index);

	if (index != 6)
		return -1;

	ArrayList_Insert(arrayList, 5, (void*)(size_t)100);

	index = ArrayList_IndexOf(arrayList, (void*)(size_t)6, -1, -1);
	printf("ArrayList index: %" PRIdz "\n", index);

	if (index != 7)
		return -1;

	ArrayList_Remove(arrayList, (void*)(size_t)100);

	rc = ArrayList_IndexOf(arrayList, (void*)(size_t)6, -1, -1);
	printf("ArrayList index: %d\n", rc);

	if (rc != 6)
		return -1;

	for (size_t index = 0; index < elemsToInsert; index++)
	{
		val = (size_t)ArrayList_GetItem(arrayList, 0);
		if (!ArrayList_RemoveAt(arrayList, 0))
			return -1;
		if (val != index)
		{
			printf("ArrayList: shifted %" PRIdz " entries, expected value %" PRIdz ", got %" PRIdz
			       "\n",
			       index, index, val);
			return -1;
		}
	}

	rc = ArrayList_IndexOf(arrayList, (void*)elemsToInsert, -1, -1);
	printf("ArrayList index: %d\n", rc);
	if (rc != -1)
		return -1;

	count = ArrayList_Count(arrayList);
	printf("ArrayList count: %" PRIuz "\n", count);
	if (count != 0)
		return -1;

	ArrayList_Clear(arrayList);
	ArrayList_Free(arrayList);

	return 0;
}

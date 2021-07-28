
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/collections.h>

int TestLinkedList(int argc, char* argv[])
{
	int count;
	wLinkedList* list;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	list = LinkedList_New();
	if (!list)
		return -1;

	if (!LinkedList_AddFirst(list, (void*)(size_t)1))
		return -1;
	if (!LinkedList_AddLast(list, (void*)(size_t)2))
		return -1;
	if (!LinkedList_AddLast(list, (void*)(size_t)3))
		return -1;
	count = LinkedList_Count(list);

	if (count != 3)
	{
		printf("LinkedList_Count: expected 3, actual: %d\n", count);
		return -1;
	}

	LinkedList_Enumerator_Reset(list);

	while (LinkedList_Enumerator_MoveNext(list))
	{
		printf("\t%p\n", LinkedList_Enumerator_Current(list));
	}

	printf("\n");
	printf("LinkedList First: %p Last: %p\n", LinkedList_First(list), LinkedList_Last(list));
	LinkedList_RemoveFirst(list);
	LinkedList_RemoveLast(list);
	count = LinkedList_Count(list);

	if (count != 1)
	{
		printf("LinkedList_Count: expected 1, actual: %d\n", count);
		return -1;
	}

	LinkedList_Enumerator_Reset(list);

	while (LinkedList_Enumerator_MoveNext(list))
	{
		printf("\t%p\n", LinkedList_Enumerator_Current(list));
	}

	printf("\n");
	printf("LinkedList First: %p Last: %p\n", LinkedList_First(list), LinkedList_Last(list));
	LinkedList_RemoveFirst(list);
	LinkedList_RemoveLast(list);
	count = LinkedList_Count(list);

	if (count != 0)
	{
		printf("LinkedList_Count: expected 0, actual: %d\n", count);
		return -1;
	}

	if (!LinkedList_AddFirst(list, (void*)(size_t)4))
		return -1;
	if (!LinkedList_AddLast(list, (void*)(size_t)5))
		return -1;
	if (!LinkedList_AddLast(list, (void*)(size_t)6))
		return -1;
	count = LinkedList_Count(list);

	if (count != 3)
	{
		printf("LinkedList_Count: expected 3, actual: %d\n", count);
		return -1;
	}

	LinkedList_Enumerator_Reset(list);

	while (LinkedList_Enumerator_MoveNext(list))
	{
		printf("\t%p\n", LinkedList_Enumerator_Current(list));
	}

	printf("\n");
	printf("LinkedList First: %p Last: %p\n", LinkedList_First(list), LinkedList_Last(list));
	if (!LinkedList_Remove(list, (void*)(size_t)5))
		return -1;
	LinkedList_Enumerator_Reset(list);

	while (LinkedList_Enumerator_MoveNext(list))
	{
		printf("\t%p\n", LinkedList_Enumerator_Current(list));
	}

	printf("\n");
	printf("LinkedList First: %p Last: %p\n", LinkedList_First(list), LinkedList_Last(list));
	LinkedList_Free(list);
	/* Test enumerator robustness */
	/* enumerator on an empty list */
	list = LinkedList_New();
	if (!list)
		return -1;
	LinkedList_Enumerator_Reset(list);

	while (LinkedList_Enumerator_MoveNext(list))
	{
		printf("\terror: %p\n", LinkedList_Enumerator_Current(list));
	}

	printf("\n");
	LinkedList_Free(list);
	/* Use an enumerator without reset */
	list = LinkedList_New();
	if (!list)
		return -1;
	if (!LinkedList_AddFirst(list, (void*)(size_t)4))
		return -1;
	if (!LinkedList_AddLast(list, (void*)(size_t)5))
		return -1;
	if (!LinkedList_AddLast(list, (void*)(size_t)6))
		return -1;

	while (LinkedList_Enumerator_MoveNext(list))
	{
		printf("\t%p\n", LinkedList_Enumerator_Current(list));
	}

	printf("\n");
	LinkedList_Free(list);
	return 0;
}

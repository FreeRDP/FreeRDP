
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/collections.h>

int TestLinkedList(int argc, char* argv[])
{
	int count;
	wLinkedList* list;
	list = LinkedList_New();
	LinkedList_AddFirst(list, (void*)(size_t)1);
	LinkedList_AddLast(list, (void*)(size_t)2);
	LinkedList_AddLast(list, (void*)(size_t)3);
	count = LinkedList_Count(list);

	if (count != 3)
	{
		printf("LinkedList_Count: expected 3, actual: %d\n", count);
		return -1;
	}

	LinkedList_Enumerator_Reset(list);

	while (LinkedList_Enumerator_MoveNext(list))
	{
		printf("\t%" PRIuz "\n", (size_t)LinkedList_Enumerator_Current(list));
	}

	printf("\n");
	printf("LinkedList First: %" PRIuz " Last: %" PRIuz "\n", (size_t)LinkedList_First(list),
	       (size_t)LinkedList_Last(list));
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
		printf("\t%" PRIuz "\n", (size_t)LinkedList_Enumerator_Current(list));
	}

	printf("\n");
	printf("LinkedList First: %" PRIuz " Last: %" PRIuz "\n", (size_t)LinkedList_First(list),
	       (size_t)LinkedList_Last(list));
	LinkedList_RemoveFirst(list);
	LinkedList_RemoveLast(list);
	count = LinkedList_Count(list);

	if (count != 0)
	{
		printf("LinkedList_Count: expected 0, actual: %d\n", count);
		return -1;
	}

	LinkedList_AddFirst(list, (void*)(size_t)4);
	LinkedList_AddLast(list, (void*)(size_t)5);
	LinkedList_AddLast(list, (void*)(size_t)6);
	count = LinkedList_Count(list);

	if (count != 3)
	{
		printf("LinkedList_Count: expected 3, actual: %d\n", count);
		return -1;
	}

	LinkedList_Enumerator_Reset(list);

	while (LinkedList_Enumerator_MoveNext(list))
	{
		printf("\t%" PRIuz "\n", (size_t)LinkedList_Enumerator_Current(list));
	}

	printf("\n");
	printf("LinkedList First: %" PRIuz " Last: %" PRIuz "\n", (size_t)LinkedList_First(list),
	       (size_t)LinkedList_Last(list));
	LinkedList_Remove(list, (void*)(size_t)5);
	LinkedList_Enumerator_Reset(list);

	while (LinkedList_Enumerator_MoveNext(list))
	{
		printf("\t%" PRIuz "\n", (size_t)LinkedList_Enumerator_Current(list));
	}

	printf("\n");
	printf("LinkedList First: %" PRIuz " Last: %" PRIuz "\n", (size_t)LinkedList_First(list),
	       (size_t)LinkedList_Last(list));
	LinkedList_Free(list);
	/* Test enumerator robustness */
	/* enumerator on an empty list */
	list = LinkedList_New();
	LinkedList_Enumerator_Reset(list);

	while (LinkedList_Enumerator_MoveNext(list))
	{
		printf("\terror: %" PRIuz "\n", (size_t)LinkedList_Enumerator_Current(list));
	}

	printf("\n");
	LinkedList_Free(list);
	/* Use an enumerator without reset */
	list = LinkedList_New();
	LinkedList_AddFirst(list, (void*)(size_t)4);
	LinkedList_AddLast(list, (void*)(size_t)5);
	LinkedList_AddLast(list, (void*)(size_t)6);

	while (LinkedList_Enumerator_MoveNext(list))
	{
		printf("\t%" PRIuz "\n", (size_t)LinkedList_Enumerator_Current(list));
	}

	printf("\n");
	LinkedList_Free(list);
	return 0;
}

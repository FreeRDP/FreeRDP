
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/collections.h>

int TestLinkedList(int argc, char* argv[])
{
	int count;
	int number;
	wLinkedList* list;

	list = LinkedList_New();

	LinkedList_AddFirst(list, (void*) (size_t) 1);
	LinkedList_AddLast(list, (void*) (size_t) 2);
	LinkedList_AddLast(list, (void*) (size_t) 3);

	count = LinkedList_Count(list);

	if (count != 3)
	{
		printf("LinkedList_Count: expected %d, actual: %d\n", 3, count);
		return -1;
	}

	LinkedList_Enumerator_Reset(list);

	while (LinkedList_Enumerator_MoveNext(list))
	{
		number = (int) (size_t) LinkedList_Enumerator_Current(list);
		printf("\t%d\n", number);
	}
	printf("\n");

	printf("LinkedList First: %d Last: %d\n",
			(int) (size_t) LinkedList_First(list), (int) (size_t) LinkedList_Last(list));

	LinkedList_RemoveFirst(list);
	LinkedList_RemoveLast(list);

	count = LinkedList_Count(list);

	if (count != 1)
	{
		printf("LinkedList_Count: expected %d, actual: %d\n", 1, count);
		return -1;
	}

	LinkedList_Enumerator_Reset(list);

	while (LinkedList_Enumerator_MoveNext(list))
	{
		number = (int) (size_t) LinkedList_Enumerator_Current(list);
		printf("\t%d\n", number);
	}
	printf("\n");

	printf("LinkedList First: %d Last: %d\n",
			(int) (size_t) LinkedList_First(list), (int) (size_t) LinkedList_Last(list));

	LinkedList_RemoveFirst(list);
	LinkedList_RemoveLast(list);

	count = LinkedList_Count(list);

	if (count != 0)
	{
		printf("LinkedList_Count: expected %d, actual: %d\n", 0, count);
		return -1;
	}

	LinkedList_AddFirst(list, (void*) (size_t) 4);
	LinkedList_AddLast(list, (void*) (size_t) 5);
	LinkedList_AddLast(list, (void*) (size_t) 6);

	count = LinkedList_Count(list);

	if (count != 3)
	{
		printf("LinkedList_Count: expected %d, actual: %d\n", 3, count);
		return -1;
	}

	LinkedList_Enumerator_Reset(list);

	while (LinkedList_Enumerator_MoveNext(list))
	{
		number = (int) (size_t) LinkedList_Enumerator_Current(list);
		printf("\t%d\n", number);
	}
	printf("\n");

	printf("LinkedList First: %d Last: %d\n",
			(int) (size_t) LinkedList_First(list), (int) (size_t) LinkedList_Last(list));

	LinkedList_Remove(list, (void*) (size_t) 5);

	LinkedList_Enumerator_Reset(list);

	while (LinkedList_Enumerator_MoveNext(list))
	{
		number = (int) (size_t) LinkedList_Enumerator_Current(list);
		printf("\t%d\n", number);
	}
	printf("\n");

	printf("LinkedList First: %d Last: %d\n",
			(int) (size_t) LinkedList_First(list), (int) (size_t) LinkedList_Last(list));

	LinkedList_Free(list);

	return 0;
}


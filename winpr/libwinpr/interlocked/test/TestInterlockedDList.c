
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/windows.h>
#include <winpr/interlocked.h>

typedef struct
{
	WINPR_LIST_ENTRY ItemEntry;
	ULONG Signature;
} LIST_ITEM, *PLIST_ITEM;

int TestInterlockedDList(int argc, char* argv[])
{
	ULONG Count;
	PLIST_ITEM pListItem;
	WINPR_PLIST_ENTRY pListHead;
	WINPR_PLIST_ENTRY pListEntry;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	pListHead = (WINPR_PLIST_ENTRY)winpr_aligned_malloc(sizeof(WINPR_LIST_ENTRY),
	                                                    MEMORY_ALLOCATION_ALIGNMENT);

	if (!pListHead)
	{
		printf("Memory allocation failed.\n");
		return -1;
	}

	InitializeListHead(pListHead);

	if (!IsListEmpty(pListHead))
	{
		printf("Expected empty list\n");
		return -1;
	}

	/* InsertHeadList / RemoveHeadList */

	printf("InsertHeadList / RemoveHeadList\n");

	for (Count = 1; Count <= 10; Count += 1)
	{
		pListItem =
		    (PLIST_ITEM)winpr_aligned_malloc(sizeof(LIST_ITEM), MEMORY_ALLOCATION_ALIGNMENT);
		pListItem->Signature = Count;
		InsertHeadList(pListHead, &(pListItem->ItemEntry));
	}

	for (Count = 10; Count >= 1; Count -= 1)
	{
		pListEntry = RemoveHeadList(pListHead);
		pListItem = (PLIST_ITEM)pListEntry;
		winpr_aligned_free(pListItem);
	}

	/* InsertTailList / RemoveTailList */

	printf("InsertTailList / RemoveTailList\n");

	for (Count = 1; Count <= 10; Count += 1)
	{
		pListItem =
		    (PLIST_ITEM)winpr_aligned_malloc(sizeof(LIST_ITEM), MEMORY_ALLOCATION_ALIGNMENT);
		pListItem->Signature = Count;
		InsertTailList(pListHead, &(pListItem->ItemEntry));
	}

	for (Count = 10; Count >= 1; Count -= 1)
	{
		pListEntry = RemoveTailList(pListHead);
		pListItem = (PLIST_ITEM)pListEntry;
		winpr_aligned_free(pListItem);
	}

	winpr_aligned_free(pListHead);

	return 0;
}

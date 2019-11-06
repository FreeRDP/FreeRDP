
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/windows.h>
#include <winpr/interlocked.h>

typedef struct _PROGRAM_ITEM
{
	WINPR_SLIST_ENTRY ItemEntry;
	ULONG Signature;
} PROGRAM_ITEM, *PPROGRAM_ITEM;

int TestInterlockedSList(int argc, char* argv[])
{
	ULONG Count;
	WINPR_PSLIST_ENTRY pFirstEntry;
	WINPR_PSLIST_HEADER pListHead;

	/* Initialize the list header to a MEMORY_ALLOCATION_ALIGNMENT boundary. */
	pListHead = (WINPR_PSLIST_HEADER)_aligned_malloc(sizeof(WINPR_SLIST_HEADER),
	                                                 MEMORY_ALLOCATION_ALIGNMENT);

	if (!pListHead)
	{
		printf("Memory allocation failed.\n");
		return -1;
	}

	InitializeSListHead(pListHead);

	/* Insert 10 items into the list. */
	for (Count = 1; Count <= 10; Count += 1)
	{
		PPROGRAM_ITEM pProgramItem =
		    (PPROGRAM_ITEM)_aligned_malloc(sizeof(PROGRAM_ITEM), MEMORY_ALLOCATION_ALIGNMENT);

		if (!pProgramItem)
		{
			printf("Memory allocation failed.\n");
			return -1;
		}

		pProgramItem->Signature = Count;
		pFirstEntry = InterlockedPushEntrySList(pListHead, &(pProgramItem->ItemEntry));
	}

	/* Remove 10 items from the list and display the signature. */
	for (Count = 10; Count >= 1; Count -= 1)
	{
		PPROGRAM_ITEM pProgramItem;
		WINPR_PSLIST_ENTRY pListEntry = InterlockedPopEntrySList(pListHead);

		if (!pListEntry)
		{
			printf("List is empty.\n");
			return -1;
		}

		pProgramItem = (PPROGRAM_ITEM)pListEntry;
		printf("Signature is %" PRIu32 "\n", pProgramItem->Signature);

		/*
		 * This example assumes that the SLIST_ENTRY structure is the
		 * first member of the structure. If your structure does not
		 * follow this convention, you must compute the starting address
		 * of the structure before calling the free function.
		 */

		_aligned_free(pListEntry);
	}

	/* Flush the list and verify that the items are gone. */
	pFirstEntry = InterlockedPopEntrySList(pListHead);

	if (pFirstEntry)
	{
		printf("Error: List is not empty.\n");
		return -1;
	}

	_aligned_free(pListHead);

	return 0;
}

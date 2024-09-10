
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/windows.h>
#include <winpr/interlocked.h>

#define ITEM_COUNT 23

typedef struct
{
	WINPR_SLIST_ENTRY ItemEntry;
	ULONG Signature;
} PROGRAM_ITEM, *PPROGRAM_ITEM;

int TestInterlockedSList(int argc, char* argv[])
{
	int rc = -1;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	/* Initialize the list header to a MEMORY_ALLOCATION_ALIGNMENT boundary. */
	WINPR_PSLIST_HEADER pListHead = (WINPR_PSLIST_HEADER)winpr_aligned_malloc(
	    sizeof(WINPR_SLIST_HEADER), MEMORY_ALLOCATION_ALIGNMENT);

	if (!pListHead)
	{
		printf("Memory allocation failed.\n");
		return -1;
	}

	InitializeSListHead(pListHead);

	/* Insert 10 items into the list. */
	for (ULONG Count = 0; Count < ITEM_COUNT; Count++)
	{
		PPROGRAM_ITEM pProgramItem =
		    (PPROGRAM_ITEM)winpr_aligned_malloc(sizeof(PROGRAM_ITEM), MEMORY_ALLOCATION_ALIGNMENT);

		if (!pProgramItem)
		{
			printf("Memory allocation failed.\n");
			goto fail;
		}

		pProgramItem->Signature = Count + 1UL;
		WINPR_PSLIST_ENTRY pFirstEntry =
		    InterlockedPushEntrySList(pListHead, &(pProgramItem->ItemEntry));
		if (((Count == 0) && pFirstEntry) || ((Count != 0) && !pFirstEntry))
		{
			printf("Error: List is empty.\n");
			winpr_aligned_free(pProgramItem);
			goto fail;
		}
	}

	/* Remove 10 items from the list and display the signature. */
	for (ULONG Count = 0; Count < ITEM_COUNT; Count++)
	{
		WINPR_PSLIST_ENTRY pListEntry = InterlockedPopEntrySList(pListHead);

		if (!pListEntry)
		{
			printf("List is empty.\n");
			goto fail;
		}

		PPROGRAM_ITEM pProgramItem = (PPROGRAM_ITEM)pListEntry;
		printf("Signature is %" PRIu32 "\n", pProgramItem->Signature);

		/*
		 * This example assumes that the SLIST_ENTRY structure is the
		 * first member of the structure. If your structure does not
		 * follow this convention, you must compute the starting address
		 * of the structure before calling the free function.
		 */

		winpr_aligned_free(pListEntry);
	}

	/* Flush the list and verify that the items are gone. */
	WINPR_PSLIST_ENTRY pFirstEntry = InterlockedPopEntrySList(pListHead);

	if (pFirstEntry)
	{
		printf("Error: List is not empty.\n");
		goto fail;
	}

	rc = 0;
fail:
	winpr_aligned_free(pListHead);

	return rc;
}

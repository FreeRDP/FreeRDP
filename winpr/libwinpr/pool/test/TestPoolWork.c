
#include <winpr/crt.h>
#include <winpr/pool.h>
#include <winpr/interlocked.h>

static LONG count = 0;

static void CALLBACK test_WorkCallback(PTP_CALLBACK_INSTANCE instance, void* context, PTP_WORK work)
{
	int index;
	BYTE a[1024];
	BYTE b[1024];
	BYTE c[1024];
	printf("Hello %s: %03"PRId32" (thread: 0x%08"PRIX32")\n", (char*) context,
	       InterlockedIncrement(&count), GetCurrentThreadId());

	for (index = 0; index < 100; index++)
	{
		ZeroMemory(a, 1024);
		ZeroMemory(b, 1024);
		ZeroMemory(c, 1024);
		FillMemory(a, 1024, 0xAA);
		FillMemory(b, 1024, 0xBB);
		CopyMemory(c, a, 1024);
		CopyMemory(c, b, 1024);
	}
}

static BOOL test1(void)
{
	int index;
	PTP_WORK work;
	printf("Global Thread Pool\n");
	work = CreateThreadpoolWork(test_WorkCallback, "world", NULL);

	if (!work)
	{
		printf("CreateThreadpoolWork failure\n");
		return FALSE;
	}

	/**
	 * You can post a work object one or more times (up to MAXULONG) without waiting for prior callbacks to complete.
	 * The callbacks will execute in parallel. To improve efficiency, the thread pool may throttle the threads.
	 */

	for (index = 0; index < 10; index++)
		SubmitThreadpoolWork(work);

	WaitForThreadpoolWorkCallbacks(work, FALSE);
	CloseThreadpoolWork(work);
	return TRUE;
}

static BOOL test2(void)
{
	BOOL rc = FALSE;
	int index;
	PTP_POOL pool;
	PTP_WORK work;
	PTP_CLEANUP_GROUP cleanupGroup = NULL;
	TP_CALLBACK_ENVIRON environment;
	printf("Private Thread Pool\n");

	if (!(pool = CreateThreadpool(NULL)))
	{
		printf("CreateThreadpool failure\n");
		return FALSE;
	}

	if (!SetThreadpoolThreadMinimum(pool, 4))
	{
		printf("SetThreadpoolThreadMinimum failure\n");
		goto fail;
	}

	SetThreadpoolThreadMaximum(pool, 8);
	InitializeThreadpoolEnvironment(&environment);
	SetThreadpoolCallbackPool(&environment, pool);
	cleanupGroup = CreateThreadpoolCleanupGroup();

	if (!cleanupGroup)
	{
		printf("CreateThreadpoolCleanupGroup failure\n");
		goto fail;
	}

	SetThreadpoolCallbackCleanupGroup(&environment, cleanupGroup, NULL);
	work = CreateThreadpoolWork(test_WorkCallback, "world", &environment);

	if (!work)
	{
		printf("CreateThreadpoolWork failure\n");
		goto fail;
	}

	for (index = 0; index < 10; index++)
		SubmitThreadpoolWork(work);

	WaitForThreadpoolWorkCallbacks(work, FALSE);
	rc = TRUE;
fail:

	if (cleanupGroup)
	{
		CloseThreadpoolCleanupGroupMembers(cleanupGroup, TRUE, NULL);
		CloseThreadpoolCleanupGroup(cleanupGroup);
		DestroyThreadpoolEnvironment(&environment);
		/**
		 * See Remarks at https://msdn.microsoft.com/en-us/library/windows/desktop/ms682043(v=vs.85).aspx
		 * If there is a cleanup group associated with the work object,
		 * it is not necessary to call CloseThreadpoolWork !
		 * calling the CloseThreadpoolCleanupGroupMembers function releases the work, wait,
		 * and timer objects associated with the cleanup group.
		 */
#if 0
		CloseThreadpoolWork(work); // this would segfault, see comment above. */
#endif
	}

	CloseThreadpool(pool);
	return rc;
}

int TestPoolWork(int argc, char* argv[])
{
	if (!test1())
		return -1;

	if (!test2())
		return -1;

	return 0;
}

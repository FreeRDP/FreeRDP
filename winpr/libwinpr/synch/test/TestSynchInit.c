#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/interlocked.h>

#define TEST_NUM_THREADS	100
#define TEST_NUM_FAILURES	10

static INIT_ONCE initOnceTest = INIT_ONCE_STATIC_INIT;

static HANDLE hStartEvent = NULL;
static LONG *pErrors = NULL;
static LONG *pTestThreadFunctionCalls = NULL;
static LONG *pTestOnceFunctionCalls = NULL;
static LONG *pInitOnceExecuteOnceCalls = NULL;


static BOOL CALLBACK TestOnceFunction(PINIT_ONCE once, PVOID param, PVOID *context)
{
	LONG calls = InterlockedIncrement(pTestOnceFunctionCalls) - 1;

	/* simulate execution time */
	Sleep(100 + rand() % 400);

	if (calls < TEST_NUM_FAILURES)
	{
		/* simulated error */
		return FALSE;
	}
	if (calls == TEST_NUM_FAILURES)
	{
		return TRUE;
	}
	fprintf(stderr, "%s: error: called again after success\n", __FUNCTION__);
	InterlockedIncrement(pErrors);
	return FALSE;
}

static DWORD WINAPI TestThreadFunction(LPVOID lpParam)
{
	LONG calls;
	BOOL ok;
	InterlockedIncrement(pTestThreadFunctionCalls);
	if (WaitForSingleObject(hStartEvent, INFINITE) != WAIT_OBJECT_0)
	{
		fprintf(stderr, "%s: error: failed to wait for start event\n", __FUNCTION__);
		InterlockedIncrement(pErrors);
		return 0;
	}

	ok = InitOnceExecuteOnce(&initOnceTest, TestOnceFunction, NULL, NULL);
	calls = InterlockedIncrement(pInitOnceExecuteOnceCalls);
	if (!ok && calls > TEST_NUM_FAILURES)
	{
		fprintf(stderr, "%s: InitOnceExecuteOnce failed unexpectedly\n", __FUNCTION__);
		InterlockedIncrement(pErrors);
	}
	return 0;
}

int TestSynchInit(int argc, char* argv[])
{
	HANDLE hThreads[TEST_NUM_THREADS];
	DWORD dwCreatedThreads = 0;
	DWORD i;
	BOOL result = FALSE;

	pErrors = _aligned_malloc(sizeof(LONG), sizeof(LONG));
	pTestThreadFunctionCalls = _aligned_malloc(sizeof(LONG), sizeof(LONG));
	pTestOnceFunctionCalls = _aligned_malloc(sizeof(LONG), sizeof(LONG));
	pInitOnceExecuteOnceCalls = _aligned_malloc(sizeof(LONG), sizeof(LONG));

	if (!pErrors || !pTestThreadFunctionCalls || !pTestOnceFunctionCalls || !pInitOnceExecuteOnceCalls)
	{
		fprintf(stderr, "error: _aligned_malloc failed\n");
		goto out;
	}

	*pErrors = 0;
	*pTestThreadFunctionCalls = 0;
	*pTestOnceFunctionCalls = 0;
	*pInitOnceExecuteOnceCalls = 0;

	if (!(hStartEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		fprintf(stderr, "error creating start event\n");
		InterlockedIncrement(pErrors);
		goto out;
	}

	for (i = 0; i < TEST_NUM_THREADS; i++)
	{
		if (!(hThreads[i] = CreateThread(NULL, 0, TestThreadFunction, NULL, 0, NULL)))
		{
			fprintf(stderr, "error creating thread #%"PRIu32"\n", i);
			InterlockedIncrement(pErrors);
			goto out;
		}
		dwCreatedThreads++;
	}

	Sleep(100);
	SetEvent(hStartEvent);

	for (i = 0; i < dwCreatedThreads; i++)
	{
		if (WaitForSingleObject(hThreads[i], INFINITE) != WAIT_OBJECT_0)
		{
			fprintf(stderr, "error: error waiting for thread #%"PRIu32"\n", i);
			InterlockedIncrement(pErrors);
			goto out;
		}
	}

	if (*pErrors == 0 &&
		*pTestThreadFunctionCalls == TEST_NUM_THREADS &&
		*pInitOnceExecuteOnceCalls == TEST_NUM_THREADS &&
		*pTestOnceFunctionCalls == TEST_NUM_FAILURES + 1)
	{
		result = TRUE;
	}

out:
	fprintf(stderr, "Test result:              %s\n", result ? "OK" : "ERROR");
	fprintf(stderr, "Error count:              %"PRId32"\n", pErrors ? *pErrors : -1);
	fprintf(stderr, "Threads created:          %"PRIu32"\n", dwCreatedThreads);
	fprintf(stderr, "TestThreadFunctionCalls:  %"PRId32"\n", pTestThreadFunctionCalls ? *pTestThreadFunctionCalls : -1);
	fprintf(stderr, "InitOnceExecuteOnceCalls: %"PRId32"\n", pInitOnceExecuteOnceCalls ? *pInitOnceExecuteOnceCalls : -1);
	fprintf(stderr, "TestOnceFunctionCalls:    %"PRId32"\n", pTestOnceFunctionCalls ? *pTestOnceFunctionCalls : -1);

	_aligned_free(pErrors);
	_aligned_free(pTestThreadFunctionCalls);
	_aligned_free(pTestOnceFunctionCalls);
	_aligned_free(pInitOnceExecuteOnceCalls);

	CloseHandle(hStartEvent);


	for (i = 0; i < dwCreatedThreads; i++)
	{
		CloseHandle(hThreads[i]);
	}

	return (result ? 0 : 1);
}

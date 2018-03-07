// Copyright © 2015 Hewlett-Packard Development Company, L.P.

#include <winpr/file.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

static DWORD WINAPI thread_func(LPVOID arg)
{
	/* exists of the thread the quickest as possible */
	ExitThread(0);
	return 0;
}

int TestThreadExitThread(int argc, char* argv[])
{
	HANDLE thread; 
	DWORD waitResult;
	int i;

	/* FIXME: create some noise to better guaranty the test validity and
         * decrease the number of loops */
	for (i=0; i<50000; i++)
	{
		thread = CreateThread(NULL,
				0,
				thread_func,
				NULL,
				0,
				NULL);

		if (thread == INVALID_HANDLE_VALUE)
		{
			fprintf(stderr, "Got an invalid thread!\n");
			return -1;
		}

		waitResult = WaitForSingleObject(thread, 1000);
		if (waitResult != WAIT_OBJECT_0)
		{
			/* When the thread exits before the internal thread_list
			 * was updated, ExitThread() is not able to retrieve the
			 * related WINPR_THREAD object and is not able to signal
			 * the end of the thread. Therefore WaitForSingleObject
			 * never get the signal.
			 */
			fprintf(stderr, "1 second should have been enough for the thread to be in a signaled state\n");
			return -1;
		}

		CloseHandle(thread);
	}
	return 0;
}

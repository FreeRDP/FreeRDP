
#include <winpr/crt.h>
#include <winpr/thread.h>
#include <winpr/collections.h>

static DWORD WINAPI message_queue_consumer_thread(LPVOID arg)
{
	wMessage message;
	wMessageQueue* queue;

	queue = (wMessageQueue*) arg;

	while (MessageQueue_Wait(queue))
	{
		if (MessageQueue_Peek(queue, &message, TRUE))
		{
			if (message.id == WMQ_QUIT)
				break;

			printf("Message.Type: %"PRIu32"\n", message.id);
		}
	}

	return 0;
}

int TestMessageQueue(int argc, char* argv[])
{
	HANDLE thread;
	wMessageQueue* queue;

	if (!(queue = MessageQueue_New(NULL)))
	{
		printf("failed to create message queue\n");
		return 1;
	}

	if (!(thread = CreateThread(NULL, 0, message_queue_consumer_thread, (void*) queue, 0, NULL)))
	{
		printf("failed to create thread\n");
		MessageQueue_Free(queue);
		return 1;
	}

	if (!MessageQueue_Post(queue, NULL, 123, NULL, NULL) ||
			!MessageQueue_Post(queue, NULL, 456, NULL, NULL) ||
			!MessageQueue_Post(queue, NULL, 789, NULL, NULL) ||
			!MessageQueue_PostQuit(queue, 0) ||
			WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0)
		return -1;

	MessageQueue_Free(queue);
	CloseHandle(thread);

	return 0;
}


#include <winpr/crt.h>
#include <winpr/thread.h>
#include <winpr/collections.h>

static void* message_queue_consumer_thread(void* arg)
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

			printf("Message.Type: %d\n", message.id);
		}
	}

	return NULL;
}

int TestMessageQueue(int argc, char* argv[])
{
	HANDLE thread;
	wMessageQueue* queue;

	queue = MessageQueue_New();

	thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) message_queue_consumer_thread, (void*) queue, 0, NULL);

	MessageQueue_Post(queue, NULL, 123, NULL, NULL);
	MessageQueue_Post(queue, NULL, 456, NULL, NULL);
	MessageQueue_Post(queue, NULL, 789, NULL, NULL);
	MessageQueue_PostQuit(queue, 0);

	WaitForSingleObject(thread, INFINITE);

	MessageQueue_Free(queue);

	return 0;
}


#include <winpr/crt.h>
#include <winpr/thread.h>
#include <winpr/collections.h>

static void* message_echo_pipe_client_thread(void* arg)
{
	int index;
	int count;
	wMessage message;
	wMessagePipe* pipe;

	count = index = 0;
	pipe = (wMessagePipe*) arg;

	while (index < 100)
	{
		MessageQueue_Post(pipe->In, NULL, 0, (void*) (size_t) index, NULL);

		if (!MessageQueue_Wait(pipe->Out))
			break;

		if (!MessageQueue_Peek(pipe->Out, &message, TRUE))
			break;

		if (message.id == WMQ_QUIT)
			break;

		count = (int) (size_t) message.wParam;

		if (count != index)
			printf("Echo count mismatch: Actual: %d, Expected: %d\n", count, index);

		index++;
	}

	MessageQueue_PostQuit(pipe->In, 0);

	return NULL;
}

static void* message_echo_pipe_server_thread(void* arg)
{
	int count;
	wMessage message;
	wMessagePipe* pipe;

	pipe = (wMessagePipe*) arg;

	while (MessageQueue_Wait(pipe->In))
	{
		if (MessageQueue_Peek(pipe->In, &message, TRUE))
		{
			if (message.id == WMQ_QUIT)
				break;

			count = (int) (size_t) message.wParam;

			MessageQueue_Dispatch(pipe->Out, &message);
		}
	}

	return NULL;
}

int TestMessagePipe(int argc, char* argv[])
{
	HANDLE ClientThread;
	HANDLE ServerThread;
	wMessagePipe* EchoPipe;

	EchoPipe = MessagePipe_New();

	ClientThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) message_echo_pipe_client_thread, (void*) EchoPipe, 0, NULL);
	ServerThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) message_echo_pipe_server_thread, (void*) EchoPipe, 0, NULL);

	WaitForSingleObject(ClientThread, INFINITE);
	WaitForSingleObject(ServerThread, INFINITE);

	return 0;
}

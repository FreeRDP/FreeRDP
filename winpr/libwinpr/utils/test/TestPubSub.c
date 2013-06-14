
#include <winpr/crt.h>
#include <winpr/thread.h>
#include <winpr/collections.h>

typedef struct _MouseMotionEventArgs
{
	wEventArgs e;

	int x;
	int y;

} MouseMotionEventArgs;

typedef void (*pMouseMotionEventHandler)(void* context, MouseMotionEventArgs* e);

void MouseMotionEventHandler(void* context, MouseMotionEventArgs* e)
{
	printf("MouseMotionEvent: x: %d y: %d\n", e->x, e->y);
}

struct _wEvent
{
	const char* EventName;
	pEventHandler EventHandler;
	wEventArgs EventArgs;
};
typedef struct _wEvent wEvent;

static wEvent Events[] =
{
	{ "MouseMotion", NULL, { sizeof(MouseMotionEventArgs) } },
	{ NULL, NULL, { 0 } },
};

void ListExportedEvents(wEvent* events)
{
	int i;

	for (i = 0; events[i].EventName; i++)
	{
		printf("Event[%d]: %s\n", i, events[i].EventName);
	}
}

void RegisterEventHandler(wEvent* events, const char* EventName, pEventHandler EventHandler)
{
	int i;

	for (i = 0; events[i].EventName; i++)
	{
		if (strcmp(events[i].EventName, EventName) == 0)
		{
			printf("Registering Event Handler for %s\n", EventName);
			events[i].EventHandler = EventHandler;
		}
	}
}

void TriggerEvent(wEvent* events, const char* EventName, void* context, wEventArgs* e)
{
	int i;

	for (i = 0; events[i].EventName; i++)
	{
		if (strcmp(events[i].EventName, EventName) == 0)
		{
			printf("Triggering Event %s\n", EventName);

			if (events[i].EventHandler)
				events[i].EventHandler(context, e);
		}
	}
}

int TestPubSub(int argc, char* argv[])
{
	wPubSub* nodeA;
	wPubSub* nodeB;

	nodeA = PubSub_New(TRUE);
	nodeB = PubSub_New(TRUE);

	ListExportedEvents(Events);

	/* Register Event Handler */
	RegisterEventHandler(Events, "MouseMotion", (pEventHandler) MouseMotionEventHandler);

	/* Call Event Handler */
	{
		MouseMotionEventArgs e;

		e.x = 64;
		e.y = 128;

		TriggerEvent(Events, "MouseMotion", NULL, (wEventArgs*) &e);
	}

	PubSub_Free(nodeA);
	PubSub_Free(nodeB);

	return 0;
}


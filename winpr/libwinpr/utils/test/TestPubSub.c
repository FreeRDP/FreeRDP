
#include <winpr/crt.h>
#include <winpr/thread.h>
#include <winpr/collections.h>

DEFINE_EVENT_BEGIN(MouseMotion)
	int x;
	int y;
DEFINE_EVENT_END(MouseMotion)

DEFINE_EVENT_BEGIN(MouseButton)
	int x;
	int y;
	int flags;
	int button;
DEFINE_EVENT_END(MouseButton)

void MouseMotionEventHandler(void* context, MouseMotionEventArgs* e)
{
	printf("MouseMotionEvent: x: %d y: %d\n", e->x, e->y);
}

void MouseButtonEventHandler(void* context, MouseButtonEventArgs* e)
{
	printf("MouseButtonEvent: x: %d y: %d flags: %d button: %d\n", e->x, e->y, e->flags, e->button);
}

static wEventType Node_Events[] =
{
	DEFINE_EVENT_ENTRY(MouseMotion)
	DEFINE_EVENT_ENTRY(MouseButton)
};

#define NODE_EVENT_COUNT (sizeof(Node_Events) / sizeof(wEventType))

int TestPubSub(int argc, char* argv[])
{
	wPubSub* node;

	node = PubSub_New(TRUE);

	PubSub_AddEventTypes(node, Node_Events, NODE_EVENT_COUNT);

	PubSub_SubscribeMouseMotion(node, MouseMotionEventHandler);
	PubSub_SubscribeMouseButton(node, MouseButtonEventHandler);

	/* Call Event Handler */
	{
		MouseMotionEventArgs e;

		e.x = 64;
		e.y = 128;

		PubSub_OnMouseMotion(node, NULL, &e);
	}

	{
		MouseButtonEventArgs e;

		e.x = 23;
		e.y = 56;
		e.flags = 7;
		e.button = 1;

		PubSub_OnMouseButton(node, NULL, &e);
	}

	PubSub_Free(node);

	return 0;
}


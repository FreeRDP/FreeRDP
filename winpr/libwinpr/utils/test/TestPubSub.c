
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

static void MouseMotionEventHandler(void* context, const MouseMotionEventArgs* e)
{
	printf("MouseMotionEvent: x: %d y: %d\n", e->x, e->y);
}

static void MouseButtonEventHandler(void* context, const MouseButtonEventArgs* e)
{
	printf("MouseButtonEvent: x: %d y: %d flags: %d button: %d\n", e->x, e->y, e->flags, e->button);
}

static wEventType Node_Events[] = { DEFINE_EVENT_ENTRY(MouseMotion),
	                                DEFINE_EVENT_ENTRY(MouseButton) };

#define NODE_EVENT_COUNT (sizeof(Node_Events) / sizeof(wEventType))

int TestPubSub(int argc, char* argv[])
{
	wPubSub* node = nullptr;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	node = PubSub_New(TRUE);
	if (!node)
		return -1;

	PubSub_AddEventTypes(node, Node_Events, NODE_EVENT_COUNT);

	if (PubSub_SubscribeMouseMotion(node, MouseMotionEventHandler) < 0)
		return -1;
	if (PubSub_SubscribeMouseButton(node, MouseButtonEventHandler) < 0)
		return -1;

	/* Call Event Handler */
	{
		MouseMotionEventArgs e;

		e.x = 64;
		e.y = 128;

		PubSub_OnMouseMotion(node, nullptr, &e);
	}

	{
		MouseButtonEventArgs e;

		e.x = 23;
		e.y = 56;
		e.flags = 7;
		e.button = 1;

		PubSub_OnMouseButton(node, nullptr, &e);
	}

	PubSub_Free(node);

	return 0;
}

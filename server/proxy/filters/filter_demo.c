#include "filters_api.h"

static PF_FILTER_RESULT demo_filter_keyboard_event(connectionInfo* info, void* param)
{
	proxyKeyboardEventInfo* event_data = (proxyKeyboardEventInfo*) param;
	return FILTER_PASS;
}

static PF_FILTER_RESULT demo_filter_mouse_event(connectionInfo* info, void* param)
{
	proxyMouseEventInfo* event_data = (proxyMouseEventInfo*) param;

    if (event_data->x % 100 == 0)
    {
        return FILTER_DROP;
    }

	return FILTER_PASS;
}

bool filter_init(proxyEvents* events)
{
	events->KeyboardEvent = demo_filter_keyboard_event;
	events->MouseEvent = demo_filter_mouse_event;
}

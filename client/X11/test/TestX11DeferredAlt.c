/* FreeRDP: Licensed under the Apache License, Version 2.0. */
#include <freerdp/config.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xutil.h>
#include <freerdp/input.h>
#include <freerdp/client.h>
#include <freerdp/locale/locale.h>
#include "../xf_keyboard.h"
#include "../xf_event.h"

static const Display* cdisplay_stub = (const Display*)0x123456789abcdef;
static Display* display_stub = (Display*)0x123456789abcdef;

/* Run the real X11 keyboard/focus/button handlers with a deterministic X server
 * and input sink. No display, window manager, RDP server or sleeps are needed. */
static char trace[1024];
static size_t used;
static BOOL failAlt;
static BOOL minimized;
static unsigned ungrabs;
static BOOL altGr;
static char physical[32];
static XkbDescRec keyboard_map;
static XkbNamesRec keyboard_names;
static XkbKeyNameRec keyboard_key_names[256];
static XModifierKeymap modifier_map;
static KeySym keyboard_keysym;

struct keys_t
{
	KeySym sym;
	KeyCode code;
	DWORD scan;
	char name;
};
static const struct keys_t keys[] = { { XK_Alt_L, 64, RDP_SCANCODE_LMENU, 'A' },
	                                  { XK_Alt_R, 108, RDP_SCANCODE_RMENU, 'R' },
	                                  { XK_Shift_L, 50, RDP_SCANCODE_LSHIFT, 'S' },
	                                  { XK_Shift_R, 62, RDP_SCANCODE_RSHIFT, 'H' },
	                                  { XK_Control_L, 37, RDP_SCANCODE_LCONTROL, 'C' },
	                                  { XK_Control_R, 105, RDP_SCANCODE_RCONTROL, 'D' },
	                                  { XK_Super_L, 133, RDP_SCANCODE_LWIN, 'L' },
	                                  { XK_Super_R, 134, RDP_SCANCODE_RWIN, 'G' },
	                                  { XK_f, 41, RDP_SCANCODE_KEY_F, 'F' },
	                                  { XK_Tab, 23, RDP_SCANCODE_TAB, 'T' },
	                                  { XK_m, 58, RDP_SCANCODE_KEY_M, 'N' } };

static void record(char name, int direction)
{
	WINPR_ASSERT(used + 2 < sizeof(trace));
	trace[used++] = name;
	trace[used++] = WINPR_CXX_COMPAT_CAST(char, (direction & 0xff));
	trace[used] = '\0';
}

Bool XGetEventData(Display* display, XGenericEventCookie* cookie)
{
	WINPR_UNUSED(display);
	WINPR_ASSERT(display == cdisplay_stub);
	memset(cookie, 0, sizeof(XGenericEventCookie));
	return True;
}

void XFreeEventData(Display* display, WINPR_ATTR_UNUSED XGenericEventCookie* cookie)
{
	WINPR_UNUSED(display);
	WINPR_ASSERT(display == cdisplay_stub);
}

KeyCode XKeysymToKeycode(Display* display, KeySym sym)
{
	WINPR_UNUSED(display);
	WINPR_ASSERT(display == cdisplay_stub);
	if (sym == XK_ISO_Level3_Shift || sym == XK_Mode_switch)
		return altGr ? 108 : 0;
	for (size_t i = 0; i < ARRAYSIZE(keys); i++)
	{
		if (keys[i].sym == sym)
			return keys[i].code;
	}
	return 0;
}

XkbDescPtr XkbGetMap(Display* display, unsigned int which, unsigned int device_spec)
{
	WINPR_UNUSED(display);
	WINPR_ASSERT(display == cdisplay_stub);
	WINPR_UNUSED(which);
	WINPR_UNUSED(device_spec);
	return &keyboard_map;
}

Status XkbGetNames(Display* display, unsigned int which, XkbDescPtr desc)
{
	WINPR_UNUSED(display);
	WINPR_ASSERT(display == cdisplay_stub);
	WINPR_UNUSED(which);
	WINPR_UNUSED(desc);
	return Success;
}

void XkbFreeKeyboard(XkbDescPtr xkb, unsigned int which, Bool free_desc)
{
	WINPR_UNUSED(xkb);
	WINPR_UNUSED(which);
	WINPR_UNUSED(free_desc);
}

KeySym XkbKeycodeToKeysym(Display* display,
#if NeedWidePrototypes
                          unsigned int
#else
                          KeyCode
#endif
                              keycode,
                          int group, int level)
{
	WINPR_UNUSED(display);
	WINPR_ASSERT(display == cdisplay_stub);
	WINPR_UNUSED(keycode);
	WINPR_UNUSED(group);
	WINPR_UNUSED(level);
	return keyboard_keysym;
}

int XDisplayKeycodes(Display* display, int* min_keycodes_return, int* max_keycodes_return)
{
	WINPR_UNUSED(display);
	WINPR_ASSERT(display == cdisplay_stub);
	*min_keycodes_return = keyboard_map.min_key_code;
	*max_keycodes_return = keyboard_map.max_key_code;
	return 1;
}

XModifierKeymap* XGetModifierMapping(Display* display)
{
	WINPR_UNUSED(display);
	WINPR_ASSERT(display == cdisplay_stub);
	return &modifier_map;
}

int XFreeModifiermap(XModifierKeymap* modmap)
{
	WINPR_UNUSED(modmap);
	return 1;
}

int XQueryKeymap(Display* display, char map[32])
{
	WINPR_UNUSED(display);
	WINPR_ASSERT(display == cdisplay_stub);
	memcpy(map, physical, sizeof(physical));
	return 1;
}

Bool XQueryPointer(Display* display, Window window, Window* root, Window* child, int* rx, int* ry,
                   int* x, int* y, unsigned int* mask)
{
	WINPR_UNUSED(display);
	WINPR_ASSERT(display == cdisplay_stub);
	WINPR_UNUSED(window);
	*root = *child = None;
	*rx = *ry = *x = *y = 0;
	*mask = 0;
	return False;
}

int XSync(Display* display, Bool discard)
{
	WINPR_UNUSED(display);
	WINPR_ASSERT(display == cdisplay_stub);
	WINPR_UNUSED(discard);
	return 1;
}

int XIconifyWindow(Display* display, Window window, int screen)
{
	WINPR_UNUSED(display);
	WINPR_ASSERT(display == cdisplay_stub);
	WINPR_UNUSED(window);
	WINPR_UNUSED(screen);
	minimized = TRUE;
	return 1;
}

int XUngrabKeyboard(Display* display, Time time)
{
	WINPR_UNUSED(display);
	WINPR_ASSERT(display == cdisplay_stub);
	WINPR_UNUSED(time);
	ungrabs++;
	return 1;
}

int XUngrabPointer(Display* display, Time time)
{
	WINPR_UNUSED(display);
	WINPR_ASSERT(display == cdisplay_stub);
	WINPR_UNUSED(time);
	return 1;
}

BOOL freerdp_input_send_keyboard_event_ex(rdpInput* input, BOOL down, BOOL repeat,
                                          UINT32 rdp_scancode)
{
	WINPR_UNUSED(input);
	if (failAlt && down &&
	    ((rdp_scancode == RDP_SCANCODE_LMENU) || (rdp_scancode == RDP_SCANCODE_RMENU)))
		return FALSE;
	for (size_t i = 0; i < ARRAYSIZE(keys); i++)
	{
		const struct keys_t* cur = &keys[i];
		if (cur->scan == rdp_scancode)
		{

			record(cur->name, down ? (repeat ? '*' : '+') : '-');
			return TRUE;
		}
	}
	record('?', down ? '+' : '-');
	return TRUE;
}

BOOL freerdp_input_send_synchronize_event(rdpInput* input, UINT32 flags)
{
	WINPR_UNUSED(input);
	WINPR_UNUSED(flags);
	return TRUE;
}

BOOL freerdp_input_send_focus_in_event(rdpInput* input, UINT16 toggleStates)
{
	WINPR_UNUSED(input);
	WINPR_UNUSED(toggleStates);
	return TRUE;
}

BOOL freerdp_client_send_button_event(rdpClientContext* cctx, BOOL relative, UINT16 mflags, INT32 x,
                                      INT32 y)
{
	WINPR_UNUSED(cctx);
	WINPR_UNUSED(relative);
	WINPR_UNUSED(x);
	WINPR_UNUSED(y);
	record((mflags & PTR_FLAGS_MOVE) ? 'M' : 'B', (mflags & PTR_FLAGS_DOWN) ? '+' : '-');
	return TRUE;
}

BOOL freerdp_client_send_extended_button_event(rdpClientContext* cctx, BOOL relative, UINT16 mflags,
                                               INT32 x, INT32 y)
{
	WINPR_UNUSED(cctx);
	WINPR_UNUSED(relative);
	WINPR_UNUSED(x);
	WINPR_UNUSED(y);
	record('E', (mflags & PTR_XFLAGS_DOWN) ? '+' : '-');
	return TRUE;
}

BOOL freerdp_client_send_wheel_event(rdpClientContext* cctx, UINT16 flags)
{
	WINPR_UNUSED(cctx);
	WINPR_UNUSED(flags);
	record('W', '+');
	return TRUE;
}

static void key(xfContext* xfc, KeySym sym, BOOL down)
{
	XKeyEvent event = WINPR_C_ARRAY_INIT;
	event.keycode = XKeysymToKeycode(xfc->display, sym);
	if (down)
	{
		const int val = physical[event.keycode / 8] | (1 << (event.keycode % 8));
		physical[event.keycode / 8] = WINPR_CXX_COMPAT_CAST(char, (val & 0xff));
		xf_keyboard_key_press(xfc, &event, sym);
	}
	else
	{
		const int val = physical[event.keycode / 8] & ~(1 << (event.keycode % 8));
		physical[event.keycode / 8] = WINPR_CXX_COMPAT_CAST(char, (val & 0xff));
		xf_keyboard_key_release(xfc, &event, sym);
	}
}

WINPR_ATTR_NODISCARD
static BOOL focus(xfContext* xfc, int type, int mode)
{
	XEvent event = WINPR_C_ARRAY_INIT;
	event.xfocus.type = type;
	event.xfocus.mode = mode;
	return xf_event_process(xfc->common.context.instance, &event);
}

typedef enum
{
	END,
	ALT_DOWN,
	ALT_UP,
	SHIFT_DOWN,
	SHIFT_UP,
	CTRL_DOWN,
	CTRL_UP,
	RIGHT_ALT_DOWN,
	RIGHT_ALT_UP,
	F_DOWN,
	F_UP,
	TAB_UP,
	RIGHT_SHIFT_DOWN,
	RIGHT_SHIFT_UP,
	M_DOWN,
	M_UP,
	RIGHT_CTRL_DOWN,
	RIGHT_CTRL_UP,
	TAB_DOWN,
	GRAB_OUT,
	GRAB_IN,
	FOCUS_OUT,
	FOCUS_IN,
	WHILE_GRABBED_OUT,
	MOTION,
	BUTTON_DOWN,
	BUTTON_UP,
	WHEEL_DOWN,
	WHEEL_UP,
	RAW_BUTTON_DOWN,
	LOCAL_CANCEL
} Event;

static const struct
{
	KeySym sym;
	BOOL down;
} keyEvents[] = {
	{ 0, FALSE },           { XK_Alt_L, TRUE },      { XK_Alt_L, FALSE },     { XK_Shift_L, TRUE },
	{ XK_Shift_L, FALSE },  { XK_Control_L, TRUE },  { XK_Control_L, FALSE }, { XK_Alt_R, TRUE },
	{ XK_Alt_R, FALSE },    { XK_f, TRUE },          { XK_f, FALSE },         { XK_Tab, FALSE },
	{ XK_Shift_R, TRUE },   { XK_Shift_R, FALSE },   { XK_m, TRUE },          { XK_m, FALSE },
	{ XK_Control_R, TRUE }, { XK_Control_R, FALSE }, { XK_Tab, TRUE }
};

enum
{
	DEFAULT_MODE = 1,
	UNICODE_MODE = 2,
	FAIL_ALT = 4,
	ALTGR_MODE = 8,
	EXPECT_MINIMIZED = 32,
	GRAB_KEYBOARD = 64,
	EXPECT_UNGRAB = 128
};
static const struct
{
	const char* name;
	Event events[16];
	const char* expected;
	unsigned flags;
} cases[] = {
	{ "right Ctrl ungrab forwards release and subsequent input",
	  { RIGHT_CTRL_DOWN, RIGHT_CTRL_UP, F_DOWN, F_UP },
	  "D+D-F+F-",
	  EXPECT_UNGRAB },
	{ "right Ctrl ungrab with keyboard grabbing enabled",
	  { RIGHT_CTRL_DOWN, RIGHT_CTRL_UP, F_DOWN, F_UP },
	  "D+D-F+F-",
	  EXPECT_UNGRAB | GRAB_KEYBOARD },
	{ "default right Ctrl ungrab",
	  { RIGHT_CTRL_DOWN, RIGHT_CTRL_UP, F_DOWN, F_UP },
	  "D+D-F+F-",
	  EXPECT_UNGRAB | DEFAULT_MODE },
	{ "remote Alt+Tab", { ALT_DOWN, TAB_DOWN, TAB_UP, ALT_UP }, "A+T+T-A-", GRAB_KEYBOARD },
	{ "default sends immediately", { ALT_DOWN }, "A+", DEFAULT_MODE },
	{ "default Alt tap", { ALT_DOWN, ALT_UP }, "A+A-", DEFAULT_MODE },
	{ "pending repeats", { ALT_DOWN, ALT_DOWN }, "", 0 },
	{ "bare tap", { ALT_DOWN, ALT_DOWN, ALT_UP }, "A+A-", 0 },
	{ "Unicode Alt tap uses scancodes", { ALT_DOWN, ALT_UP }, "A+A-", UNICODE_MODE },
	{ "Alt+F ordering and repeat", { ALT_DOWN, F_DOWN, F_DOWN, F_UP, ALT_UP }, "A+F+F*F-A-", 0 },
	{ "Shift alone remains immediate", { SHIFT_DOWN, SHIFT_UP }, "S+S-", 0 },
	{ "Shift before Alt",
	  { SHIFT_DOWN, ALT_DOWN, F_DOWN, F_UP, ALT_UP, SHIFT_UP },
	  "S+A+F+F-A-S-",
	  0 },
	{ "Shift does not flush Alt", { ALT_DOWN, SHIFT_DOWN }, "S+", 0 },
	{ "Shift repeats do not flush Alt", { ALT_DOWN, SHIFT_DOWN, SHIFT_DOWN }, "S+S*", 0 },
	{ "Alt before Shift",
	  { ALT_DOWN, SHIFT_DOWN, F_DOWN, F_UP, SHIFT_UP, ALT_UP },
	  "S+A+F+F-S-A-",
	  0 },
	{ "Alt first, Shift release commits chord",
	  { ALT_DOWN, SHIFT_DOWN, SHIFT_UP, ALT_UP },
	  "S+A+S-A-",
	  0 },
	{ "Shift first, Alt release commits tap",
	  { SHIFT_DOWN, ALT_DOWN, ALT_UP, SHIFT_UP },
	  "S+A+A-S-",
	  0 },
	{ "Shift first, Shift release commits chord",
	  { SHIFT_DOWN, ALT_DOWN, SHIFT_UP, ALT_UP },
	  "S+A+S-A-",
	  0 },
	{ "right Shift release commits chord",
	  { RIGHT_ALT_DOWN, RIGHT_SHIFT_DOWN, RIGHT_SHIFT_UP, RIGHT_ALT_UP },
	  "H+R+H-R-",
	  0 },
	{ "other modifier flushes Alt", { ALT_DOWN, CTRL_DOWN, CTRL_UP, ALT_UP }, "A+C+C-A-", 0 },
	{ "right Alt default immediate", { RIGHT_ALT_DOWN }, "R+", DEFAULT_MODE },
	{ "right Alt pending repeats", { RIGHT_ALT_DOWN, RIGHT_ALT_DOWN }, "", 0 },
	{ "right Alt tap", { RIGHT_ALT_DOWN, RIGHT_ALT_UP }, "R+R-", 0 },
	{ "right Alt Unicode tap", { RIGHT_ALT_DOWN, RIGHT_ALT_UP }, "R+R-", UNICODE_MODE },
	{ "AltGr immediate", { RIGHT_ALT_DOWN }, "R+", ALTGR_MODE },
	{ "AltGr chord", { RIGHT_ALT_DOWN, F_DOWN, F_UP, RIGHT_ALT_UP }, "R+F+F-R-", ALTGR_MODE },
	{ "both Alt keys remain pending", { ALT_DOWN, RIGHT_ALT_DOWN }, "", 0 },
	{ "both Alt keys flush left then right",
	  { RIGHT_ALT_DOWN, ALT_DOWN, ALT_UP, RIGHT_ALT_UP },
	  "A+R+A-R-",
	  0 },
	{ "both Alt keys canceled",
	  { ALT_DOWN, RIGHT_ALT_DOWN, GRAB_OUT, GRAB_IN, ALT_UP, RIGHT_ALT_UP },
	  "",
	  0 },
	{ "host consumes Tab without changing selection",
	  { ALT_DOWN, GRAB_OUT, ALT_UP, GRAB_IN },
	  "",
	  0 },
	{ "host Alt+Shift+Tab",
	  { ALT_DOWN, SHIFT_DOWN, GRAB_OUT, FOCUS_OUT, SHIFT_UP, ALT_UP, FOCUS_IN, TAB_UP },
	  "S+S-",
	  0 },
	{ "host Shift+Alt+Tab",
	  { SHIFT_DOWN, ALT_DOWN, GRAB_OUT, SHIFT_UP, ALT_UP, GRAB_IN },
	  "S+S-",
	  0 },
	{ "right Alt+right Shift host switch",
	  { RIGHT_ALT_DOWN, RIGHT_SHIFT_DOWN, GRAB_OUT, RIGHT_SHIFT_UP, RIGHT_ALT_UP, GRAB_IN },
	  "H+H-",
	  0 },
	{ "pending release after return is ignored", { ALT_DOWN, FOCUS_OUT, FOCUS_IN, ALT_UP }, "", 0 },
	{ "held Alt is not restored for guest input",
	  { ALT_DOWN, GRAB_OUT, GRAB_IN, F_DOWN, F_UP, ALT_UP },
	  "F+F-",
	  0 },
	{ "cleanup releases sent keys", { ALT_DOWN, F_DOWN, WHILE_GRABBED_OUT }, "A+F+F-A-", 0 },
	{ "cleanup does not repeat releases",
	  { ALT_DOWN, F_DOWN, WHILE_GRABBED_OUT, F_UP, ALT_UP, FOCUS_IN },
	  "A+F+F-A-",
	  0 },
	{ "pointer motion leaves Alt pending", { ALT_DOWN, MOTION }, "M-", 0 },
	{ "Alt+button", { ALT_DOWN, MOTION, BUTTON_DOWN, BUTTON_UP, ALT_UP }, "M-A+B+B-A-", 0 },
	{ "right Alt+button", { RIGHT_ALT_DOWN, BUTTON_DOWN, BUTTON_UP, RIGHT_ALT_UP }, "R+B+B-R-", 0 },
	{ "wheel release does not flush", { ALT_DOWN, WHEEL_UP }, "", 0 },
	{ "Alt+wheel", { ALT_DOWN, WHEEL_UP, WHEEL_DOWN, ALT_UP }, "A+W+A-", 0 },
	{ "Alt+raw extended button", { ALT_DOWN, RAW_BUTTON_DOWN, ALT_UP }, "A+E+A-", 0 },
	{ "local action cancels unused Alt", { ALT_DOWN, LOCAL_CANCEL, ALT_UP }, "", 0 },
	{ "local action retains sent Alt release",
	  { ALT_DOWN, F_DOWN, F_UP, LOCAL_CANCEL, ALT_UP },
	  "A+F+F-A-",
	  0 },
	{ "failed Alt-down produces no Alt-up", { ALT_DOWN, ALT_UP }, "", FAIL_ALT },
	{ "failed right Alt-down produces no Alt-up", { RIGHT_ALT_DOWN, RIGHT_ALT_UP }, "", FAIL_ALT },
	{ "failed Alt-down blocks guest key", { ALT_DOWN, F_DOWN, F_UP, ALT_UP }, "", FAIL_ALT },
	{ "canceled Alt release leaves other Alt pending",
	  { ALT_DOWN, GRAB_OUT, GRAB_IN, RIGHT_ALT_DOWN, ALT_UP, RIGHT_ALT_UP },
	  "R+R-",
	  0 },
	{ "local shortcut sees pending physical Alt",
	  { CTRL_DOWN, ALT_DOWN, M_DOWN, M_UP, ALT_UP, CTRL_UP },
	  "C+C-",
	  EXPECT_MINIMIZED }
};

WINPR_ATTR_NODISCARD
static BOOL dispatch(xfContext* xfc, Event event)
{
	if (event > END && event < ARRAYSIZE(keyEvents))
	{
		key(xfc, keyEvents[event].sym, keyEvents[event].down);
		return TRUE;
	}
	switch (event)
	{
		case GRAB_OUT:
			return focus(xfc, FocusOut, NotifyGrab);
		case GRAB_IN:
			return focus(xfc, FocusIn, NotifyUngrab);
		case FOCUS_OUT:
			return focus(xfc, FocusOut, NotifyNormal);
		case FOCUS_IN:
			return focus(xfc, FocusIn, NotifyNormal);
		case WHILE_GRABBED_OUT:
			return focus(xfc, FocusOut, NotifyWhileGrabbed);
		case MOTION:
			return xf_generic_MotionNotify(xfc, 10, 20, 10, 20, None, FALSE);
		case BUTTON_DOWN:
		case BUTTON_UP:
			return xf_generic_ButtonEvent(xfc, 10, 20, 10, 20, 1, None, FALSE,
			                              event == BUTTON_DOWN);
		case WHEEL_DOWN:
		case WHEEL_UP:
			return xf_generic_ButtonEvent(xfc, 10, 20, 10, 20, 4, None, FALSE, event == WHEEL_DOWN);
		case RAW_BUTTON_DOWN:
			return xf_generic_RawButtonEvent(xfc, 8, FALSE, TRUE);
		case LOCAL_CANCEL:
			xfc->deferredAlt.pending = 0;
			return TRUE;
		default:
			return FALSE;
	}
}

WINPR_ATTR_NODISCARD
static BOOL test_keyboard_mapping_reload(void)
{
	BOOL rc = FALSE;
	xfContext xfc = WINPR_C_ARRAY_INIT;
	rdpSettings* settings = freerdp_settings_new(0);
	if (!settings)
		return FALSE;

	keyboard_map = (XkbDescRec)WINPR_C_ARRAY_INIT;
	keyboard_names = (XkbNamesRec)WINPR_C_ARRAY_INIT;
	memset(keyboard_key_names, 0, sizeof(keyboard_key_names));
	keyboard_map.min_key_code = 38;
	keyboard_map.max_key_code = 39;
	keyboard_map.names = &keyboard_names;
	keyboard_names.keys = keyboard_key_names;
	keyboard_keysym = XK_a;
	xfc.common.context.settings = settings;
	xfc.display = display_stub;
	xfc.log = WLog_Get("com.freerdp.client.x11.test.keyboard");
	if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardLayout, ENGLISH_UNITED_STATES))
	{
		(void)fprintf(stderr, "%s: 1: KeyboardLayout could not be set\n", __func__);
		goto out;
	}

	memcpy(keyboard_key_names[38].name, "AC01", XkbKeyNameLength);
	if (!xf_keyboard_init(&xfc) || (xfc.X11_KEYCODE_TO_VIRTUAL_SCANCODE[38] != RDP_SCANCODE_KEY_A))
	{
		(void)fprintf(stderr, "%s: 2: xf_keyboard_init failed or %u != %u\n", __func__,
		              xfc.X11_KEYCODE_TO_VIRTUAL_SCANCODE[38], RDP_SCANCODE_KEY_A);
		goto out;
	}

#if defined(__APPLE__)
	const DWORD expect = RDP_SCANCODE_OEM_6;
#else
	const DWORD expect = RDP_SCANCODE_KEY_A;
#endif

	/* A new map with non-standard key names must be recovered through keysyms. */
	memset(keyboard_key_names[38].name, 0, XkbKeyNameLength);
	memcpy(keyboard_key_names[38].name, "L001", XkbKeyNameLength);
	if (!xf_keyboard_init(&xfc) || (xfc.X11_KEYCODE_TO_VIRTUAL_SCANCODE[38] != expect))
	{
		(void)fprintf(stderr, "%s: 3: xf_keyboard_init failed or %u != %u\n", __func__,
		              xfc.X11_KEYCODE_TO_VIRTUAL_SCANCODE[38], expect);
		goto out;
	}

	/* If the replacement map is unusable, retain the last working mapping. */
	keyboard_keysym = NoSymbol;
	if (!xf_keyboard_init(&xfc) || (xfc.X11_KEYCODE_TO_VIRTUAL_SCANCODE[38] != expect))
	{
		(void)fprintf(stderr, "%s: 4: xf_keyboard_init failed or %u != %u\n", __func__,
		              xfc.X11_KEYCODE_TO_VIRTUAL_SCANCODE[38], expect);
		goto out;
	}

	rc = TRUE;
out:
	xf_keyboard_free(&xfc);
	freerdp_settings_free(settings);
	return rc;
}

int main(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	if (!test_keyboard_mapping_reload())
		return 1;

	int rc = 1;
	xfContext xfc = WINPR_C_ARRAY_INIT;
	xfWindow window = WINPR_C_ARRAY_INIT;
	XModifierKeymap modifiers = WINPR_C_ARRAY_INIT;
	freerdp instance = WINPR_C_ARRAY_INIT;
	rdpInput input = WINPR_C_ARRAY_INIT;
	rdpSettings* settings = freerdp_settings_new(0);
	/* xf_minimize() publishes a window state change */
	wPubSub* pubSub = PubSub_New(TRUE);
	if (!settings || !pubSub)
	{
		(void)fprintf(stderr, "settings=%p, pubSub=%p\n", (const void*)settings,
		              (const void*)pubSub);
		goto out;
	}
	xfc.common.context.settings = settings;
	xfc.common.context.pubSub = pubSub;
	xfc.common.context.input = &input;
	xfc.common.context.instance = &instance;
	instance.context = &xfc.common.context;
	xfc.window = &window;
	xfc.display = display_stub; /* All X calls used in this test are mocked. */
	xfc.modifierMap = &modifiers;
	xfc.log = WLog_Get("com.freerdp.client.x11.test.deferred-alt");
	for (size_t i = 0; i < ARRAYSIZE(keys); i++)
		xfc.X11_KEYCODE_TO_VIRTUAL_SCANCODE[keys[i].code] = keys[i].scan;
	xfc.button_map[0].button = 1;
	xfc.button_map[0].flags = PTR_FLAGS_BUTTON1;
	xfc.button_map[1].button = 4;
	xfc.button_map[1].flags = PTR_FLAGS_WHEEL | 120;
	xfc.button_map[2].button = 8;
	xfc.button_map[2].flags = PTR_XFLAGS_BUTTON1;
	for (size_t i = 0; i < ARRAYSIZE(cases); i++)
	{
		memset(xfc.KeyboardState, 0, sizeof(xfc.KeyboardState));
		memset(&xfc.deferredAlt, 0, sizeof(xfc.deferredAlt));
		memset(physical, 0, sizeof(physical));
		used = 0;
		trace[0] = '\0';
		failAlt = (cases[i].flags & FAIL_ALT) != 0;
		minimized = FALSE;
		ungrabs = 0;
		xfc.wasRightCtrlAlreadyPressed = FALSE;
		xfc.ungrabKeyboardWithRightCtrl = FALSE;
		xfc.grab_keyboard = (cases[i].flags & GRAB_KEYBOARD) != 0;
		altGr = (cases[i].flags & ALTGR_MODE) != 0;
		xfc.deferAlt = !(cases[i].flags & DEFAULT_MODE);
		if (!freerdp_settings_set_bool(settings, FreeRDP_GrabKeyboard, xfc.grab_keyboard) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_UnicodeInput,
		                               (cases[i].flags & UNICODE_MODE) != 0))
		{
			(void)fprintf(stderr, "%s: GrabKeyboard or UnicodeInput or %" PRIu32 " != 0\n",
			              cases[i].name, cases[i].flags & UNICODE_MODE);
			goto out;
		}
		for (size_t j = 0; j < ARRAYSIZE(cases[i].events) && cases[i].events[j] != END; j++)
		{
			if (!dispatch(&xfc, cases[i].events[j]))
			{
				(void)fprintf(stderr, "%s: event %zu failed\n", cases[i].name, j);
				goto out;
			}
		}
		if ((cases[i].flags & EXPECT_UNGRAB) && (ungrabs != 1))
		{
			(void)fprintf(stderr, "%s: expected one keyboard ungrab, got %u\n", cases[i].name,
			              ungrabs);
			goto out;
		}
		if ((cases[i].flags & EXPECT_MINIMIZED) && !minimized)
		{
			(void)fprintf(stderr, "%s: local shortcut did not minimize\n", cases[i].name);
			goto out;
		}
		if (strcmp(trace, cases[i].expected) != 0)
		{
			(void)fprintf(stderr, "%s: expected '%s', got '%s'\n", cases[i].name, cases[i].expected,
			              trace);
			goto out;
		}
	}
	rc = 0;
out:
	PubSub_Free(pubSub);
	freerdp_settings_free(settings);
	return rc;
}

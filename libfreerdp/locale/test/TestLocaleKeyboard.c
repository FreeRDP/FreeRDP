#include <stdio.h>
#include <winpr/crypto.h>
#include <freerdp/locale/keyboard.h>

static BOOL test_scancode_name(void)
{
	const DWORD scancodes[] = { RDP_SCANCODE_ESCAPE,
		                        RDP_SCANCODE_KEY_1,
		                        RDP_SCANCODE_KEY_2,
		                        RDP_SCANCODE_KEY_3,
		                        RDP_SCANCODE_KEY_4,
		                        RDP_SCANCODE_KEY_5,
		                        RDP_SCANCODE_KEY_6,
		                        RDP_SCANCODE_KEY_7,
		                        RDP_SCANCODE_KEY_8,
		                        RDP_SCANCODE_KEY_9,
		                        RDP_SCANCODE_KEY_0,
		                        RDP_SCANCODE_OEM_MINUS,
		                        RDP_SCANCODE_OEM_PLUS,
		                        RDP_SCANCODE_BACKSPACE,
		                        RDP_SCANCODE_TAB,
		                        RDP_SCANCODE_KEY_Q,
		                        RDP_SCANCODE_KEY_W,
		                        RDP_SCANCODE_KEY_E,
		                        RDP_SCANCODE_KEY_R,
		                        RDP_SCANCODE_KEY_T,
		                        RDP_SCANCODE_KEY_Y,
		                        RDP_SCANCODE_KEY_U,
		                        RDP_SCANCODE_KEY_I,
		                        RDP_SCANCODE_KEY_O,
		                        RDP_SCANCODE_KEY_P,
		                        RDP_SCANCODE_OEM_4,
		                        RDP_SCANCODE_OEM_6,
		                        RDP_SCANCODE_RETURN,
		                        RDP_SCANCODE_LCONTROL,
		                        RDP_SCANCODE_KEY_A,
		                        RDP_SCANCODE_KEY_S,
		                        RDP_SCANCODE_KEY_D,
		                        RDP_SCANCODE_KEY_F,
		                        RDP_SCANCODE_KEY_G,
		                        RDP_SCANCODE_KEY_H,
		                        RDP_SCANCODE_KEY_J,
		                        RDP_SCANCODE_KEY_K,
		                        RDP_SCANCODE_KEY_L,
		                        RDP_SCANCODE_OEM_1,
		                        RDP_SCANCODE_OEM_7,
		                        RDP_SCANCODE_OEM_3,
		                        RDP_SCANCODE_LSHIFT,
		                        RDP_SCANCODE_OEM_5,
		                        RDP_SCANCODE_KEY_Z,
		                        RDP_SCANCODE_KEY_X,
		                        RDP_SCANCODE_KEY_C,
		                        RDP_SCANCODE_KEY_V,
		                        RDP_SCANCODE_KEY_B,
		                        RDP_SCANCODE_KEY_N,
		                        RDP_SCANCODE_KEY_M,
		                        RDP_SCANCODE_OEM_COMMA,
		                        RDP_SCANCODE_OEM_PERIOD,
		                        RDP_SCANCODE_OEM_2,
		                        RDP_SCANCODE_RSHIFT,
		                        RDP_SCANCODE_MULTIPLY,
		                        RDP_SCANCODE_LMENU,
		                        RDP_SCANCODE_SPACE,
		                        RDP_SCANCODE_CAPSLOCK,
		                        RDP_SCANCODE_F1,
		                        RDP_SCANCODE_F2,
		                        RDP_SCANCODE_F3,
		                        RDP_SCANCODE_F4,
		                        RDP_SCANCODE_F5,
		                        RDP_SCANCODE_F6,
		                        RDP_SCANCODE_F7,
		                        RDP_SCANCODE_F8,
		                        RDP_SCANCODE_F9,
		                        RDP_SCANCODE_F10,
		                        RDP_SCANCODE_NUMLOCK,
		                        RDP_SCANCODE_SCROLLLOCK,
		                        RDP_SCANCODE_NUMPAD7,
		                        RDP_SCANCODE_NUMPAD8,
		                        RDP_SCANCODE_NUMPAD9,
		                        RDP_SCANCODE_SUBTRACT,
		                        RDP_SCANCODE_NUMPAD4,
		                        RDP_SCANCODE_NUMPAD5,
		                        RDP_SCANCODE_NUMPAD6,
		                        RDP_SCANCODE_ADD,
		                        RDP_SCANCODE_NUMPAD1,
		                        RDP_SCANCODE_NUMPAD2,
		                        RDP_SCANCODE_NUMPAD3,
		                        RDP_SCANCODE_NUMPAD0,
		                        RDP_SCANCODE_DECIMAL,
		                        RDP_SCANCODE_SYSREQ,
		                        RDP_SCANCODE_OEM_102,
		                        RDP_SCANCODE_F11,
		                        RDP_SCANCODE_F12,
		                        RDP_SCANCODE_SLEEP,
		                        RDP_SCANCODE_ZOOM,
		                        RDP_SCANCODE_HELP,
		                        RDP_SCANCODE_F13,
		                        RDP_SCANCODE_F14,
		                        RDP_SCANCODE_F15,
		                        RDP_SCANCODE_F16,
		                        RDP_SCANCODE_F17,
		                        RDP_SCANCODE_F18,
		                        RDP_SCANCODE_F19,
		                        RDP_SCANCODE_F20,
		                        RDP_SCANCODE_F21,
		                        RDP_SCANCODE_F22,
		                        RDP_SCANCODE_F23,
		                        RDP_SCANCODE_F24,
		                        RDP_SCANCODE_HIRAGANA,
		                        RDP_SCANCODE_HANJA_KANJI,
		                        RDP_SCANCODE_KANA_HANGUL,
		                        RDP_SCANCODE_ABNT_C1,
		                        RDP_SCANCODE_F24_JP,
		                        RDP_SCANCODE_CONVERT_JP,
		                        RDP_SCANCODE_NONCONVERT_JP,
		                        RDP_SCANCODE_TAB_JP,
		                        RDP_SCANCODE_BACKSLASH_JP,
		                        RDP_SCANCODE_ABNT_C2,
		                        RDP_SCANCODE_HANJA,
		                        RDP_SCANCODE_HANGUL,
		                        RDP_SCANCODE_RETURN_KP,
		                        RDP_SCANCODE_RCONTROL,
		                        RDP_SCANCODE_DIVIDE,
		                        RDP_SCANCODE_PRINTSCREEN,
		                        RDP_SCANCODE_RMENU,
		                        RDP_SCANCODE_PAUSE,
		                        RDP_SCANCODE_HOME,
		                        RDP_SCANCODE_UP,
		                        RDP_SCANCODE_PRIOR,
		                        RDP_SCANCODE_LEFT,
		                        RDP_SCANCODE_RIGHT,
		                        RDP_SCANCODE_END,
		                        RDP_SCANCODE_DOWN,
		                        RDP_SCANCODE_NEXT,
		                        RDP_SCANCODE_INSERT,
		                        RDP_SCANCODE_DELETE,
		                        RDP_SCANCODE_NULL,
		                        RDP_SCANCODE_HELP2,
		                        RDP_SCANCODE_LWIN,
		                        RDP_SCANCODE_RWIN,
		                        RDP_SCANCODE_APPS,
		                        RDP_SCANCODE_POWER_JP,
		                        RDP_SCANCODE_SLEEP_JP,
		                        RDP_SCANCODE_NUMLOCK_EXTENDED,
		                        RDP_SCANCODE_RSHIFT_EXTENDED,
		                        RDP_SCANCODE_VOLUME_MUTE,
		                        RDP_SCANCODE_VOLUME_DOWN,
		                        RDP_SCANCODE_VOLUME_UP,
		                        RDP_SCANCODE_MEDIA_NEXT_TRACK,
		                        RDP_SCANCODE_MEDIA_PREV_TRACK,
		                        RDP_SCANCODE_MEDIA_STOP,
		                        RDP_SCANCODE_MEDIA_PLAY_PAUSE,
		                        RDP_SCANCODE_BROWSER_BACK,
		                        RDP_SCANCODE_BROWSER_FORWARD,
		                        RDP_SCANCODE_BROWSER_REFRESH,
		                        RDP_SCANCODE_BROWSER_STOP,
		                        RDP_SCANCODE_BROWSER_SEARCH,
		                        RDP_SCANCODE_BROWSER_FAVORITES,
		                        RDP_SCANCODE_BROWSER_HOME,
		                        RDP_SCANCODE_LAUNCH_MAIL,
		                        RDP_SCANCODE_LAUNCH_MEDIA_SELECT,
		                        RDP_SCANCODE_LAUNCH_APP1,
		                        RDP_SCANCODE_LAUNCH_APP2 };
	for (size_t x = 0; x < ARRAYSIZE(scancodes); x++)
	{
		const DWORD code = scancodes[x];
		const char* sc = freerdp_keyboard_scancode_name(code);
		if (!sc)
		{
			(void)fprintf(stderr, "Failed to run freerdp_keyboard_scancode_name(%" PRIu32 ")\n",
			              code);
			return FALSE;
		}
	}

	return TRUE;
}

static BOOL test_layouts(DWORD types)
{
	BOOL rc = FALSE;
	size_t count = 0;
	RDP_KEYBOARD_LAYOUT* layouts = freerdp_keyboard_get_layouts(types, &count);
	if (!layouts || (count == 0))
	{
		(void)fprintf(stderr,
		              "freerdp_keyboard_get_layouts(type: %" PRIu32 ") -> %" PRIuz
		              " elements, layouts: %p:\n",
		              types, count, layouts);
		goto fail;
	}

	for (size_t x = 0; x < count; x++)
	{
		const RDP_KEYBOARD_LAYOUT* cur = &layouts[x];
		if ((cur->code == 0) || (!cur->name) || (strnlen(cur->name, 2) == 0))
		{
			(void)fprintf(stderr,
			              "freerdp_keyboard_get_layouts(type: %" PRIu32 ") -> %" PRIuz
			              " elements, failed:\n",
			              types, count);
			(void)fprintf(stderr, "[%" PRIuz "]: code= %" PRIu32 ", name = %s\n", x, cur->code,
			              cur->name);
			goto fail;
		}

		const char* name = freerdp_keyboard_get_layout_name_from_id(cur->code);
		if (!name)
		{
			(void)fprintf(stderr,
			              "freerdp_keyboard_get_layouts(type: %" PRIu32 ") -> %" PRIuz
			              " elements, failed:\n",
			              types, count);
			(void)fprintf(stderr,
			              "[%" PRIuz "]: freerdp_keyboard_get_layouts(%" PRIu32 ") -> NULL\n", x,
			              cur->code);
			goto fail;
		}
#if 0 // TODO: Should these always match?
       if (strcmp(name, cur->name) != 0) {
           (void)fprintf(stderr, "freerdp_keyboard_get_layouts(type: %" PRIu32 ") -> %" PRIuz " elements, failed:\n", types, count);
           (void)fprintf(stderr, "[%" PRIuz "]: freerdp_keyboard_get_layouts(%"  PRIu32 ") -> %s != %s\n", x, cur->code, name, cur->name);
           goto fail;
       }
#endif

		const DWORD id = freerdp_keyboard_get_layout_id_from_name(cur->name);
		// if (id != cur->code) {
		if (id == 0)
		{
			(void)fprintf(stderr,
			              "freerdp_keyboard_get_layouts(type: %" PRIu32 ") -> %" PRIuz
			              " elements, failed:\n",
			              types, count);
			(void)fprintf(stderr,
			              "[%" PRIuz "]: freerdp_keyboard_get_layout_id_from_name(%s) -> %" PRIu32
			              " != %" PRIu32 "\n",
			              x, cur->name, id, cur->code);
			goto fail;
		}
	}

	rc = TRUE;
fail:
	freerdp_keyboard_layouts_free(layouts, count);
	return rc;
}

static DWORD get_random(DWORD offset)
{
	DWORD x = 0;
	winpr_RAND(&x, sizeof(x));
	x = x % UINT32_MAX - offset;
	x += offset;
	return x;
}

static BOOL test_scancode_cnv(void)
{
	for (DWORD x = 0; x < UINT8_MAX; x++)
	{
		const DWORD sc = freerdp_keyboard_get_rdp_scancode_from_x11_keycode(x);
		const BOOL ex = RDP_SCANCODE_EXTENDED(sc);
		const DWORD kk = freerdp_keyboard_get_x11_keycode_from_rdp_scancode(sc, ex);
		if (sc != kk)
		{
			(void)fprintf(stderr,
			              "[%" PRIu32 "]: keycode->scancode->keycode failed: %" PRIu32
			              " -> %" PRIu32 " -> %" PRIu32 "\n",
			              x, sc, kk);
			return FALSE;
		}
	}

	for (DWORD x = 0; x < 23; x++)
	{
		DWORD x = get_random(UINT8_MAX);

		const DWORD sc = freerdp_keyboard_get_rdp_scancode_from_x11_keycode(x);
		const DWORD kk = freerdp_keyboard_get_x11_keycode_from_rdp_scancode(sc, FALSE);
		const DWORD kkex = freerdp_keyboard_get_x11_keycode_from_rdp_scancode(sc, TRUE);
		if ((sc != 0) || (kk != 0) || (kkex != 0))
		{
			(void)fprintf(stderr,
			              "[%" PRIu32 "]: invalid scancode %" PRIu32 ", keycode %" PRIu32
			              " or keycode extended %" PRIu32 " has a value != 0\n",
			              x, sc, kk, kkex);
			return FALSE;
		}
	}
	return TRUE;
}

static BOOL test_codepages(void)
{

	for (DWORD column = 0; column < 4; column++)
	{
		size_t count = 0;
		RDP_CODEPAGE* cp = freerdp_keyboard_get_matching_codepages(column, NULL, &count);
		if (!cp || (count == 0))
		{
			(void)fprintf(stderr,
			              "freerdp_keyboard_get_matching_codepages(%" PRIu32 ", NULL) failed!\n",
			              column);
			return FALSE;
		}
		freerdp_codepages_free(cp);
	}

	for (DWORD x = 0; x < 23; x++)
	{
		DWORD column = get_random(4);
		size_t count = 0;
		RDP_CODEPAGE* cp = freerdp_keyboard_get_matching_codepages(column, NULL, &count);
		freerdp_codepages_free(cp);
		if (cp || (count != 0))
		{
			(void)fprintf(stderr,
			              "freerdp_keyboard_get_matching_codepages(%" PRIu32
			              ", NULL) returned not NULL!\n",
			              column);
			return FALSE;
		}
	}

	// TODO: Test with filters set
	// TODO: Test with invalid filters set

	return TRUE;
}

static BOOL test_init(void)
{
	const DWORD kbd = freerdp_keyboard_init(0);
	if (kbd == 0)
	{
		(void)fprintf(stderr, "freerdp_keyboard_init(0) returned invalid layout 0\n");
		return FALSE;
	}

	const DWORD kbdex = freerdp_keyboard_init_ex(0, NULL);
	if (kbd == 0)
	{
		(void)fprintf(stderr, "freerdp_keyboard_init_ex(0, NULL) returned invalid layout 0\n");
		return FALSE;
	}

	if (kbd != kbdex)
	{
		(void)fprintf(
		    stderr,
		    "freerdp_keyboard_init(0) != freerdp_keyboard_init_ex(0, NULL): returned %" PRIu32
		    " vs %" PRIu32 "\n",
		    kbd, kbdex);
		return FALSE;
	}

	// TODO: Test with valid remap list
	// TODO: Test with invalid remap list
	// TODO: Test with defaults != 0
	return TRUE;
}

int TestLocaleKeyboard(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!test_scancode_name())
		return -1;

	if (!test_layouts(RDP_KEYBOARD_LAYOUT_TYPE_STANDARD))
		return -1;
	if (!test_layouts(RDP_KEYBOARD_LAYOUT_TYPE_VARIANT))
		return -1;
	if (!test_layouts(RDP_KEYBOARD_LAYOUT_TYPE_IME))
		return -1;
	if (!test_layouts(RDP_KEYBOARD_LAYOUT_TYPE_STANDARD | RDP_KEYBOARD_LAYOUT_TYPE_VARIANT))
		return -1;
	if (!test_layouts(RDP_KEYBOARD_LAYOUT_TYPE_STANDARD | RDP_KEYBOARD_LAYOUT_TYPE_IME))
		return -1;
	if (!test_layouts(RDP_KEYBOARD_LAYOUT_TYPE_VARIANT | RDP_KEYBOARD_LAYOUT_TYPE_IME))
		return -1;
	if (!test_layouts(RDP_KEYBOARD_LAYOUT_TYPE_STANDARD | RDP_KEYBOARD_LAYOUT_TYPE_VARIANT |
	                  RDP_KEYBOARD_LAYOUT_TYPE_IME))
		return -1;
	if (test_layouts(UINT32_MAX &
	                 ~(RDP_KEYBOARD_LAYOUT_TYPE_STANDARD | RDP_KEYBOARD_LAYOUT_TYPE_VARIANT |
	                   RDP_KEYBOARD_LAYOUT_TYPE_IME)))
		return -1;
	if (!test_scancode_cnv())
		return -1;
	if (!test_codepages())
		return -1;
	if (!test_init())
		return -1;

	return 0;
}

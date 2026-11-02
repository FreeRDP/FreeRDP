#include <winpr/input.h>

static bool testKeyboardType(enum WINPR_KBD_TYPE type, DWORD flag)
{
	for (DWORD x = 0; x < 0x100; x++)
	{
		const DWORD vk = GetVirtualKeyCodeFromVirtualScanCode(x | flag, type);

		/* > 127 is out of range */
		if (x > 127)
		{
			if (vk != VK_NONE)
				return false;
		}

		const DWORD sc = GetVirtualScanCodeFromVirtualKeyCode(vk | flag, type);
		if (sc != x)
		{
			if ((sc != 0) && (vk != VK_NONE))
			{
				DWORD buffer[256] = WINPR_C_ARRAY_INIT;
				const DWORD count = GetVirtualScanCodesFromVirtualKeyCode(vk | flag, type, buffer,
				                                                          ARRAYSIZE(buffer));

				bool isOK = false;
				for (DWORD i = 0; i < count; i++)
				{
					const DWORD cur = buffer[i];
					if (cur == sc)
					{
						isOK = true;
						break;
					}
				}
				if (!isOK)
				{
					printf("[%" PRIu32 "]: got %" PRIu32 ", expected %" PRIu32, type, sc, x);
					return false;
				}
			}
			else if ((sc == 0) && (vk == VK_NONE))
			{
			}
			else
			{
				printf("[%" PRIu32 "]: got %" PRIu32 ", expected %" PRIu32, type, sc, x);
				return false;
			}
		}
	}
	return true;
}

static bool testKeyboardTypes(enum WINPR_KBD_TYPE type)
{
	if (!testKeyboardType(type, 0))
		return false;
	if (!testKeyboardType(type, KBDMULTIVK))
		return false;
	return testKeyboardType(type, KBDEXT);
}

/* the modifier keys of a Mac keyboard, both ways (right Control was listed as the right
 * Windows key, so a Windows right Control found no Mac key at all) */
static bool testAppleModifiers(void)
{
	const struct
	{
		DWORD keycode;
		DWORD vkcode;
	} modifiers[] = { { 0x36, VK_RWIN | KBDEXT },  { 0x37, VK_LWIN | KBDEXT },
		              { 0x38, VK_LSHIFT },         { 0x3A, VK_LMENU },
		              { 0x3B, VK_LCONTROL },       { 0x3C, VK_RSHIFT },
		              { 0x3D, VK_RMENU | KBDEXT }, { 0x3E, VK_RCONTROL | KBDEXT } };

	for (size_t x = 0; x < ARRAYSIZE(modifiers); x++)
	{
		const DWORD keycode = modifiers[x].keycode;
		const DWORD vkcode = modifiers[x].vkcode;
		const DWORD vk = GetVirtualKeyCodeFromKeycode(keycode, WINPR_KEYCODE_TYPE_APPLE);
		const DWORD kc = GetKeycodeFromVirtualKeyCode(vkcode, WINPR_KEYCODE_TYPE_APPLE);
		if ((vk != vkcode) || (kc != keycode))
		{
			printf("Apple keycode 0x%02" PRIx32 ": got VK 0x%04" PRIx32 " and keycode 0x%02" PRIx32
			       ", expected VK 0x%04" PRIx32 "\n",
			       keycode, vk, kc, vkcode);
			return false;
		}
	}
	return true;
}

static bool testVKToKeycode(WINPR_KEYCODE_TYPE type)
{

	for (DWORD x = 0; x <= 0x100; x++)
	{
		const DWORD vkc = GetVirtualKeyCodeFromKeycode(x, type);
		const DWORD kc = GetKeycodeFromVirtualKeyCode(vkc, type);
		if (x != kc)
		{
			if (!((kc == 0) && (vkc == VK_NONE)))
			{
				DWORD keycodes[0x100] = WINPR_C_ARRAY_INIT;
				const DWORD count =
				    GetKeycodesFromVirtualKeyCode(vkc, type, keycodes, ARRAYSIZE(keycodes));
				bool rc = false;
				for (DWORD y = 0; y < count; y++)
				{
					if (keycodes[y] == x)
						rc = true;
				}
				if (!rc)
					return false;
			}
		}
	}
	return true;
}

static bool testVKToKeycodes(void)
{
	const WINPR_KEYCODE_TYPE types[] = { WINPR_KEYCODE_TYPE_NONE, WINPR_KEYCODE_TYPE_APPLE,
		                                 WINPR_KEYCODE_TYPE_EVDEV, WINPR_KEYCODE_TYPE_XKB };

	for (size_t x = 0; x < ARRAYSIZE(types); x++)
	{
		const WINPR_KEYCODE_TYPE type = types[x];
		if (!testVKToKeycode(type))
			return false;
	}
	return true;
}

int TestScancode(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	if (!testVKToKeycodes())
		return -23;

	if (!testAppleModifiers())
		return -24;

	const enum WINPR_KBD_TYPE types[] = { WINPR_KBD_TYPE_IBM_PC_XT,  WINPR_KBD_TYPE_OLIVETTI_ICO,
		                                  WINPR_KBD_TYPE_IBM_PC_AT,  WINPR_KBD_TYPE_IBM_ENHANCED,
		                                  WINPR_KBD_TYPE_NOKIA_1050, WINPR_KBD_TYPE_NOKIA_9140,
		                                  WINPR_KBD_TYPE_JAPANESE,   WINPR_KBD_TYPE_KOREAN };

	int rc = 0;
	for (size_t x = 0; x < ARRAYSIZE(types); x++)
	{
		const enum WINPR_KBD_TYPE type = types[x];
		if (!testKeyboardTypes(type))
			rc--;
	}
	return rc;
}

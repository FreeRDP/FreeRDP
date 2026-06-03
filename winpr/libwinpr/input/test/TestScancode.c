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

int TestScancode(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
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

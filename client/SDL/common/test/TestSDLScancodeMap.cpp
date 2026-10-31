#include <set>

#include "../../SDL3/sdl_scancode_map.hpp"

/* Windows scan codes of HID keyboard usages (SDL scancodes are HID usages), from the table in
 * https://learn.microsoft.com/windows/win32/inputdev/about-keyboard-input#scan-codes */
static const struct
{
	SDL_Scancode sdl;
	UINT32 rdp;
} expected[] = {
	{ SDL_SCANCODE_A, MAKE_RDP_SCANCODE(0x1E, FALSE) },
	{ SDL_SCANCODE_ESCAPE, MAKE_RDP_SCANCODE(0x01, FALSE) },
	{ SDL_SCANCODE_RETURN, MAKE_RDP_SCANCODE(0x1C, FALSE) },
	{ SDL_SCANCODE_KP_ENTER, MAKE_RDP_SCANCODE(0x1C, TRUE) },
	{ SDL_SCANCODE_RCTRL, MAKE_RDP_SCANCODE(0x1D, TRUE) },
	{ SDL_SCANCODE_LGUI, MAKE_RDP_SCANCODE(0x5B, TRUE) },
	{ SDL_SCANCODE_APPLICATION, MAKE_RDP_SCANCODE(0x5D, TRUE) },
	{ SDL_SCANCODE_MUTE, MAKE_RDP_SCANCODE(0x20, TRUE) },
	{ SDL_SCANCODE_INTERNATIONAL1, MAKE_RDP_SCANCODE(0x73, FALSE) },
	{ SDL_SCANCODE_INTERNATIONAL2, MAKE_RDP_SCANCODE(0x70, FALSE) },
	{ SDL_SCANCODE_INTERNATIONAL3, MAKE_RDP_SCANCODE(0x7D, FALSE) },
	{ SDL_SCANCODE_INTERNATIONAL4, MAKE_RDP_SCANCODE(0x79, FALSE) },
	{ SDL_SCANCODE_INTERNATIONAL5, MAKE_RDP_SCANCODE(0x7B, FALSE) },
	{ SDL_SCANCODE_LANG1, MAKE_RDP_SCANCODE(0x72, FALSE) },
	{ SDL_SCANCODE_LANG2, MAKE_RDP_SCANCODE(0x71, FALSE) },
};

int TestSDLScancodeMap(int argc, char* argv[])
{
	(void)argc;
	(void)argv;

	/* Each SDL scancode maps to one RDP scancode. */
	std::set<Uint32> seen;
	for (const auto& cur : sdl_scancode_map)
	{
		if (!seen.insert(cur.sdl).second)
			return 1;
	}

	for (const auto& exp : expected)
	{
		bool found = false;
		for (const auto& cur : sdl_scancode_map)
		{
			if (cur.sdl != exp.sdl)
				continue;
			if (cur.rdp != exp.rdp)
				return 2;
			found = true;
		}
		if (!found)
			return 3;
	}
	return 0;
}

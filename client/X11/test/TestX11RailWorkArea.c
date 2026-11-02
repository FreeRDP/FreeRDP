/* FreeRDP: Licensed under the Apache License, Version 2.0. */
#include "../xf_rail_workarea.h"

#include <stdio.h>

int main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;
	const struct
	{
		INT32 x, y;
		UINT32 width, height, desktopWidth, desktopHeight;
		BOOL valid;
		RECTANGLE_16 expected;
	} cases[] = {
		{ 0, 0, 2558, 1244, 1280, 800, TRUE, { 0, 0, 1280, 800 } },
		{ 0, 32, 1280, 768, 1280, 800, TRUE, { 0, 32, 1280, 800 } },
		{ 40, 32, 2558, 1244, 1280, 800, TRUE, { 40, 32, 1280, 800 } },
		{ -100, -20, 1000, 700, 1280, 800, TRUE, { 0, 0, 900, 680 } },
		{ 1280, 0, 200, 200, 1280, 800, FALSE, { 0 } },
		{ -200, 0, 100, 200, 1280, 800, FALSE, { 0 } },
		{ 0, 0, 0, 800, 1280, 800, FALSE, { 0 } },
		{ 0, 0, 1280, 800, 0, 800, FALSE, { 0 } },
		{ 0, 0, 1280, 800, 65536, 800, FALSE, { 0 } },
		{ 1, 1, UINT32_MAX, UINT32_MAX, 1280, 800, TRUE, { 1, 1, 1280, 800 } },
		{ INT32_MAX, 0, UINT32_MAX, 800, 1280, 800, FALSE, { 0 } },
		{ INT32_MIN, 0, UINT32_MAX, 800, 1280, 800, TRUE, { 0, 0, 1280, 800 } },
		{ 0, 0, 65535, 65535, 65535, 65535, TRUE, { 0, 0, 65535, 65535 } },
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		RECTANGLE_16 area = { 11, 12, 13, 14 };
		const BOOL valid =
		    xf_rail_workarea_clip(cases[i].x, cases[i].y, cases[i].width, cases[i].height,
		                          cases[i].desktopWidth, cases[i].desktopHeight, &area);
		if (valid != cases[i].valid ||
		    (valid &&
		     (area.left != cases[i].expected.left || area.top != cases[i].expected.top ||
		      area.right != cases[i].expected.right || area.bottom != cases[i].expected.bottom)) ||
		    (!valid &&
		     (area.left != 11 || area.top != 12 || area.right != 13 || area.bottom != 14)))
		{
			fprintf(stderr, "Work area case %zu failed\n", i);
			return 1;
		}
	}
	return xf_rail_workarea_clip(0, 0, 1280, 800, 1280, 800, NULL) ? 1 : 0;
}


#include <stdio.h>
#include <stdint.h>
#include <rdtk/rdtk.h>
#include <winpr/error.h>

int TestRdTkNinePatch(int argc, char* argv[])
{
	rdtkEngine* engine = NULL;
	rdtkSurface* surface = NULL;
	uint32_t scanline;
	uint32_t width;
	uint32_t height;
	uint8_t* data = NULL;
	int ret = -1;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!(engine = rdtk_engine_new()))
	{
		printf("%s: error creating rdtk engine (%" PRIu32 ")\n", __FUNCTION__, GetLastError());
		goto out;
	}

	width = 1024;
	height = 768;
	scanline = width * 4;

	/* let rdtk allocate the surface buffer */
	if (!(surface = rdtk_surface_new(engine, NULL, width, height, scanline)))
	{
		printf("%s: error creating auto-allocated surface (%" PRIu32 ")\n", __FUNCTION__,
		       GetLastError());
		goto out;
	}
	rdtk_surface_free(surface);
	surface = NULL;

	/* test self-allocated buffer */
	if (!(data = calloc(height, scanline)))
	{
		printf("%s: error allocating surface buffer (%" PRIu32 ")\n", __FUNCTION__, GetLastError());
		goto out;
	}

	if (!(surface = rdtk_surface_new(engine, data, width, height, scanline)))
	{
		printf("%s: error creating self-allocated surface (%" PRIu32 ")\n", __FUNCTION__,
		       GetLastError());
		goto out;
	}

	ret = 0;

out:
	rdtk_surface_free(surface);
	rdtk_engine_free(engine);
	free(data);

	return ret;
}

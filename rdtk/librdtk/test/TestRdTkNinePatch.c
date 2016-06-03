
#include <rdtk/rdtk.h>

int TestRdTkNinePatch(int argc, char* argv[])
{
	rdtkEngine* engine = NULL;
	rdtkSurface* surface = NULL;
	DWORD scanline;
	DWORD width;
	DWORD height;
	BYTE* data = NULL;
	int ret = -1;

	if (!(engine = rdtk_engine_new()))
	{
		printf("%s: error creating rdtk engine (%u)\n", __FUNCTION__, GetLastError());
		goto out;
	}

	width = 1024;
	height = 768;
	scanline = width * 4;

	/* let rdtk allocate the surface buffer */
	if (!(surface = rdtk_surface_new(engine, NULL, width, height, scanline)))
	{
		printf("%s: error creating auto-allocated surface (%u)\n", __FUNCTION__, GetLastError());
		goto out;
	}
	rdtk_surface_free(surface);
	surface = NULL;


	/* test self-allocated buffer */
	if (!(data = calloc(height, scanline)))
	{
		printf("%s: error allocating surface buffer (%u)\n", __FUNCTION__, GetLastError());
		goto out;
	}

	if (!(surface = rdtk_surface_new(engine, data, width, height, scanline)))
	{
		printf("%s: error creating self-allocated surface (%u)\n", __FUNCTION__, GetLastError());
		goto out;
	}

	ret = 0;

out:
	rdtk_surface_free(surface);
	rdtk_engine_free(engine);
	free(data);

	return ret;
}

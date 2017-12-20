
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/winpr.h>

int TestPathShell(int argc, char* argv[])
{
	const int paths[] =
	{
		KNOWN_PATH_HOME,
		KNOWN_PATH_TEMP,
		KNOWN_PATH_XDG_DATA_HOME,
		KNOWN_PATH_XDG_CONFIG_HOME,
		KNOWN_PATH_XDG_CACHE_HOME,
		KNOWN_PATH_XDG_RUNTIME_DIR,
		KNOWN_PATH_XDG_CONFIG_HOME
	};
	const char* names[] =
	{
		"KNOWN_PATH_HOME",
		"KNOWN_PATH_TEMP",
		"KNOWN_PATH_XDG_DATA_HOME",
		"KNOWN_PATH_XDG_CONFIG_HOME",
		"KNOWN_PATH_XDG_CACHE_HOME",
		"KNOWN_PATH_XDG_RUNTIME_DIR",
		"KNOWN_PATH_XDG_CONFIG_HOME"
	};
	int rc = 0;
	size_t x;

	for (x = 0; x < sizeof(paths) / sizeof(paths[0]); x++)
	{
		const int id = paths[x];
		const char* name = names[x];
		{
			char* path = GetKnownPath(id);

			if (!path)
				rc = -1;

			printf("%s Path: %s\n", name, path);
			free(path);
		}
		{
			char* path = GetKnownSubPath(id, "freerdp");

			if (!path)
				rc = -1;

			printf("%s SubPath: %s\n", name, path);
			free(path);
		}
	}

	return rc;
}


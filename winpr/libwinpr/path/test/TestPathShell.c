
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/winpr.h>

int TestPathShell(int argc, char* argv[])
{
	char* path;

	path = GetKnownPath(KNOWN_PATH_HOME);
	printf("KNOWN_PATH_HOME: %s\n", path);

	path = GetKnownPath(KNOWN_PATH_TEMP);
	printf("KNOWN_PATH_TEMP: %s\n", path);

	path = GetKnownPath(KNOWN_PATH_XDG_DATA_HOME);
	printf("KNOWN_PATH_DATA: %s\n", path);

	path = GetKnownPath(KNOWN_PATH_XDG_CONFIG_HOME);
	printf("KNOWN_PATH_CONFIG: %s\n", path);

	path = GetKnownPath(KNOWN_PATH_XDG_CACHE_HOME);
	printf("KNOWN_PATH_CACHE: %s\n", path);

	path = GetKnownPath(KNOWN_PATH_XDG_RUNTIME_DIR);
	printf("KNOWN_PATH_RUNTIME: %s\n", path);

	path = GetKnownSubPath(KNOWN_PATH_XDG_CONFIG_HOME, "freerdp");
	printf("KNOWN_PATH_CONFIG SubPath: %s\n", path);

	return 0;
}


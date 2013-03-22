
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

	path = GetKnownPath(KNOWN_PATH_DATA);
	printf("KNOWN_PATH_DATA: %s\n", path);

	path = GetKnownPath(KNOWN_PATH_CONFIG);
	printf("KNOWN_PATH_CONFIG: %s\n", path);

	path = GetKnownPath(KNOWN_PATH_CACHE);
	printf("KNOWN_PATH_CACHE: %s\n", path);

	path = GetKnownPath(KNOWN_PATH_RUNTIME);
	printf("KNOWN_PATH_RUNTIME: %s\n", path);

	return 0;
}


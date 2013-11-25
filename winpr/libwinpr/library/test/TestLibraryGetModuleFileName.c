
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/windows.h>
#include <winpr/library.h>

int TestLibraryGetModuleFileName(int argc, char* argv[])
{
	char ModuleFileName[4096];

	GetModuleFileNameA(NULL, ModuleFileName, sizeof(ModuleFileName));

	printf("GetModuleFileNameA: %s\n", ModuleFileName);

	return 0;
}


#include <stdio.h>

#include <winpr/nt.h>

int TestNtCurrentTeb(int argc, char* argv[])
{
#ifndef _WIN32
	PTEB teb;

	teb = NtCurrentTeb();

	if (!teb)
	{
		printf("NtCurrentTeb() returned NULL\n");
		return -1;
	}
#endif
	return 0;
}

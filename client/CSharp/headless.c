#include "devolutionsrdp.h"

#ifndef WIN32
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <sys/stat.h>
#include <fcntl.h>

#ifndef WIN32
bool csharp_create_shared_buffer(char* name, int size)
{
	BOOL result = FALSE;
	int desc = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
	
	if(desc < 0)
		return NULL;
	
	if (ftruncate(desc, size) == 0)
		result = TRUE;
		//handle = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, desc, 0);
	
	close(desc);
	
	return result;
}

void csharp_destroy_shared_buffer(char* name)
{
	//munmap(buffer, size);
	shm_unlink(name);
	
}
#else
void* csharp_create_shared_buffer(char* name, int size)
{
	HANDLE hMapFile;

	hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, name);
	if (!hMapFile)
		return NULL;

	return hMapFile;
}

void csharp_destroy_shared_buffer(void* hMapFile)
{
	if (hMapFile)
		CloseHandle(hMapFile);
}
#endif
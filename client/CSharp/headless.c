#include "devolutionsrdp.h"

<<<<<<< HEAD
#ifndef WIN32
=======
#ifndef _WIN32
>>>>>>> c85522c7b67efd11585eef4ce1797f6e92d99595
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <sys/stat.h>
#include <fcntl.h>

<<<<<<< HEAD
#ifndef WIN32
bool csharp_create_shared_buffer(char* name, int size)
{
	BOOL result = FALSE;
=======
#ifdef _WIN32

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

#else

bool csharp_create_shared_buffer(char* name, int size)
{
	BOOL result = FALSE;

#if !defined(ANDROID) && !defined(IOS)
>>>>>>> c85522c7b67efd11585eef4ce1797f6e92d99595
	int desc = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
	
	if(desc < 0)
		return NULL;
	
	if (ftruncate(desc, size) == 0)
		result = TRUE;
		//handle = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, desc, 0);
	
	close(desc);
<<<<<<< HEAD
=======
#endif
>>>>>>> c85522c7b67efd11585eef4ce1797f6e92d99595
	
	return result;
}

void csharp_destroy_shared_buffer(char* name)
{
<<<<<<< HEAD
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
=======
#if !defined(ANDROID) && !defined(IOS)
	//munmap(buffer, size);
	shm_unlink(name);
#endif
}

#endif

>>>>>>> c85522c7b67efd11585eef4ce1797f6e92d99595

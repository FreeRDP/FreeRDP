/*
 * FreeRDP: A Remote Desktop Protocol Client
 * Mutex Utils
 *
 * Copyright 2011 Vic Lee
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <winpr/windows.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/mutex.h>

#ifdef _WIN32
#define freerdp_mutex_t HANDLE
#else
#include <pthread.h>
#define freerdp_mutex_t pthread_mutex_t
#endif

/**
 * Mutex are used to prevent concurrent accesses to specific portions of code.
 * This funciton and the other freerdp_mutex_*() function are defining a portable API
 * to get mutex for both Windows and Linux, where the lower level implementation is very different.
 *
 * This function creates a new freerdp_mutex object.
 * Use freerdp_mutex_lock() to get exclusive ownership of the mutex, and lock a given (protected) portion of code.
 * Use freerdp_mutex_unlock() to release your ownership on a mutex.
 * Use freerdp_mutex_free() to release the resources associated with the mutex when it is no longer needed.
 *
 * @return a freerdp_mutex pointer. This should be considered opaque, as the implementation is platform dependent.
 * NULL is returned if an allocation or initialization error occurred.
 *
 * @see pthread documentation for Linux implementation of mutexes
 * @see MSDN http://msdn.microsoft.com/en-us/library/windows/desktop/ms682411%28v=vs.85%29.aspx for Windows implementation
 */
freerdp_mutex freerdp_mutex_new(void)
{
#ifdef _WIN32
	freerdp_mutex_t mutex;
	mutex = CreateMutex(NULL, FALSE, NULL);
	return (freerdp_mutex) mutex;
#else
	freerdp_mutex_t* mutex;
	mutex = xnew(freerdp_mutex_t);
	if (mutex)
		pthread_mutex_init(mutex, 0);
	return mutex;
#endif
}

/**
 * This function is used to deallocate all resources associated with a freerdp_mutex object.
 *
 * @param mutex [in]	- Pointer to the mutex that needs to be deallocated.
 * 						  On return, this object is not valid anymore.
 */
void freerdp_mutex_free(freerdp_mutex mutex)
{
#ifdef _WIN32
	CloseHandle((freerdp_mutex_t) mutex);
#else
	pthread_mutex_destroy((freerdp_mutex_t*) mutex);
	xfree(mutex);
#endif
}

/**
 * Use this function to get exclusive ownership of the mutex.
 * This should be called before entering a portion of code that needs to be protected against concurrent accesses.
 * Use the freerdp_mutex_unlock() call to release ownership when you leave this protected code.
 *
 * @param mutex [in]	- An initialized freerdp_mutex object, as returned from a call to freerdp_mutex_new().
 *						  This function will suspend the running thread when called, until the mutex is available.
 *						  Only one thread at a time will be allowed to continue execution.
 */
void freerdp_mutex_lock(freerdp_mutex mutex)
{
#ifdef _WIN32
	WaitForSingleObject((freerdp_mutex_t) mutex, INFINITE);
#else
	pthread_mutex_lock(mutex);
#endif
}

/**
 * Use this function to release your ownership on a mutex.
 * This should be called when leaving a portion of code that needs to be protected against concurrent accesses.
 * DO NOT use this call on a mutex that you do not own. See freerdp_mutex_lock() for getting ownership on a mutex.
 *
 * @param mutex [in]	- Pointer to a mutex that was locked by a call to freerdp_mutex_lock().
 */
void freerdp_mutex_unlock(freerdp_mutex mutex)
{
#ifdef _WIN32
	ReleaseMutex((freerdp_mutex_t) mutex);
#else
	pthread_mutex_unlock(mutex);
#endif
}

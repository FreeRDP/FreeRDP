/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Sleep Utils
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <freerdp/utils/sleep.h>

#include <time.h>

#ifndef _WIN32
#define _XOPEN_SOURCE 500
#include <unistd.h>
#else
#include <windows.h>
#endif

void freerdp_sleep(uint32 seconds)
{
#ifndef _WIN32
	sleep(seconds);
#else
	Sleep(seconds * 1000);
#endif
}

void freerdp_usleep(uint32 useconds)
{
#ifndef _WIN32
	usleep(useconds);
#else
	uint64 t1;
	uint64 t2;
	uint64 freq;

	QueryPerformanceCounter((LARGE_INTEGER*) &t1);
	QueryPerformanceCounter((LARGE_INTEGER*) &freq);

	do
	{
		QueryPerformanceCounter((LARGE_INTEGER*) &t2);
	}
	while ((t2 - t1) < useconds);
#endif
}

/**
 * WinPR: Windows Portable Runtime
 * Message Pipe
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/config.h>

#include <winpr/crt.h>
#include <winpr/sysinfo.h>

#include <winpr/collections.h>

/**
 * Properties
 */

/**
 * Methods
 */

void MessagePipe_PostQuit(wMessagePipe* pipe, int nExitCode)
{
	MessageQueue_PostQuit(pipe->In, nExitCode);
	MessageQueue_PostQuit(pipe->Out, nExitCode);
}

/**
 * Construction, Destruction
 */

wMessagePipe* MessagePipe_New()
{
	wMessagePipe* pipe = NULL;

	pipe = (wMessagePipe*)malloc(sizeof(wMessagePipe));

	if (!pipe)
		return NULL;

	pipe->In = MessageQueue_New(NULL);
	if (!pipe->In)
		goto error_in;

	pipe->Out = MessageQueue_New(NULL);
	if (!pipe->Out)
		goto error_out;

	return pipe;

error_out:
	MessageQueue_Free(pipe->In);
error_in:
	free(pipe);
	return NULL;
}

void MessagePipe_Free(wMessagePipe* pipe)
{
	if (pipe)
	{
		MessageQueue_Free(pipe->In);
		MessageQueue_Free(pipe->Out);

		free(pipe);
	}
}

/**
 * WinPR: Windows Portable Runtime
 * Publisher/Subscriber Pattern
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/collections.h>

/**
 * Properties
 */

int PubSub_Property(wPubSub* pubSub)
{
	return 0;
}

/**
 * Methods
 */

void PubSub_Method(wPubSub* pubSub)
{

}

/**
 * Construction, Destruction
 */

wPubSub* PubSub_New(BOOL synchronized)
{
	wPubSub* pubSub = NULL;

	pubSub = (wPubSub*) malloc(sizeof(wPubSub));

	if (pubSub)
	{
		pubSub->synchronized = synchronized;

		if (pubSub->synchronized)
			pubSub->mutex = CreateMutex(NULL, FALSE, NULL);
	}

	return pubSub;
}

void PubSub_Free(wPubSub* pubSub)
{
	if (pubSub)
	{
		if (pubSub->synchronized)
			CloseHandle(pubSub->mutex);

		free(pubSub);
	}
}

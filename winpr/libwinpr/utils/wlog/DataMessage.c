/**
 * WinPR: Windows Portable Runtime
 * WinPR Logger
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "wlog.h"

#include "wlog/DataMessage.h"

#include "../../log.h"
#define TAG WINPR_TAG("utils.wlog")

BOOL WLog_DataMessage_Write(char* filename, void* data, int length)
{
	FILE* fp;
	BOOL ret = TRUE;

	fp = fopen(filename, "w+b");

	if (!fp)
	{
		//WLog_ERR(TAG, "failed to open file %s", filename);
		return FALSE;
	}

	if (fwrite(data, length, 1, fp) != 1)
		ret = FALSE;
	fclose(fp);
	return ret;
}

/**
 * WinPR: Windows Portable Runtime
 * Windows Native System Services
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2013 Thincast Technologies GmbH
 * Copyright 2013 Norbert Federa <norbert.federa@thincast.com>
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

#include <winpr/assert.h>
#include <winpr/config.h>

#include <winpr/crt.h>
#include <winpr/library.h>
#include <winpr/nt.h>
#include <winpr/endian.h>

#ifndef _WIN32

#include <pthread.h>

#include "../handle/handle.h"

static pthread_once_t sTebOnceControl = PTHREAD_ONCE_INIT;
static pthread_key_t sTebKey;

static void sTebDestruct(void* teb)
{
	free(teb);
}

static void sTebInitOnce(void)
{
	pthread_key_create(&sTebKey, sTebDestruct);
}

PTEB NtCurrentTeb(void)
{
	PTEB teb = NULL;

	if (pthread_once(&sTebOnceControl, sTebInitOnce) == 0)
	{
		if ((teb = pthread_getspecific(sTebKey)) == NULL)
		{
			teb = calloc(1, sizeof(TEB));
			if (teb)
				pthread_setspecific(sTebKey, teb);
		}
	}
	return teb;
}
#endif

const char* FSInformationClass2Tag(UINT32 value)
{
	switch (value)
	{
		case FileDirectoryInformation:
			return "FileDirectoryInformation";
		case FileFullDirectoryInformation:
			return "FileFullDirectoryInformation";
		case FileBothDirectoryInformation:
			return "FileBothDirectoryInformation";
		case FileBasicInformation:
			return "FileBasicInformation";
		case FileStandardInformation:
			return "FileStandardInformation";
		case FileInternalInformation:
			return "FileInternalInformation";
		case FileEaInformation:
			return "FileEaInformation";
		case FileAccessInformation:
			return "FileAccessInformation";
		case FileNameInformation:
			return "FileNameInformation";
		case FileRenameInformation:
			return "FileRenameInformation";
		case FileLinkInformation:
			return "FileLinkInformation";
		case FileNamesInformation:
			return "FileNamesInformation";
		case FileDispositionInformation:
			return "FileDispositionInformation";
		case FilePositionInformation:
			return "FilePositionInformation";
		case FileFullEaInformation:
			return "FileFullEaInformation";
		case FileModeInformation:
			return "FileModeInformation";
		case FileAlignmentInformation:
			return "FileAlignmentInformation";
		case FileAllInformation:
			return "FileAllInformation";
		case FileAllocationInformation:
			return "FileAllocationInformation";
		case FileEndOfFileInformation:
			return "FileEndOfFileInformation";
		case FileAlternateNameInformation:
			return "FileAlternateNameInformation";
		case FileStreamInformation:
			return "FileStreamInformation";
		case FilePipeInformation:
			return "FilePipeInformation";
		case FilePipeLocalInformation:
			return "FilePipeLocalInformation";
		case FilePipeRemoteInformation:
			return "FilePipeRemoteInformation";
		case FileMailslotQueryInformation:
			return "FileMailslotQueryInformation";
		case FileMailslotSetInformation:
			return "FileMailslotSetInformation";
		case FileCompressionInformation:
			return "FileCompressionInformation";
		case FileObjectIdInformation:
			return "FileObjectIdInformation";
		case FileCompletionInformation:
			return "FileCompletionInformation";
		case FileMoveClusterInformation:
			return "FileMoveClusterInformation";
		case FileQuotaInformation:
			return "FileQuotaInformation";
		case FileReparsePointInformation:
			return "FileReparsePointInformation";
		case FileNetworkOpenInformation:
			return "FileNetworkOpenInformation";
		case FileAttributeTagInformation:
			return "FileAttributeTagInformation";
		case FileTrackingInformation:
			return "FileTrackingInformation";
		case FileIdBothDirectoryInformation:
			return "FileIdBothDirectoryInformation";
		case FileIdFullDirectoryInformation:
			return "FileIdFullDirectoryInformation";
		case FileValidDataLengthInformation:
			return "FileValidDataLengthInformation";
		case FileShortNameInformation:
			return "FileShortNameInformation";
		default:
			return "UNKNOWN";
	}
}

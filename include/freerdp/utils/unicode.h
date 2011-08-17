/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Unicode Utils
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

#ifndef __UNICODE_UTILS_H
#define __UNICODE_UTILS_H

#include <stdio.h>
#include <string.h>
#include <freerdp/api.h>

#define DEFAULT_CODEPAGE	"UTF-8"
#define WINDOWS_CODEPAGE	"UTF-16LE"

#ifdef HAVE_ICONV
#include <iconv.h>
#endif

#ifndef ICONV_CONST
#define ICONV_CONST ""
#endif

struct _UNICONV
{
	int iconv;
#ifdef HAVE_ICONV
	iconv_t* in_iconv_h;
	iconv_t* out_iconv_h;
#endif
};
typedef struct _UNICONV UNICONV;

FREERDP_API UNICONV* freerdp_uniconv_new();
FREERDP_API void freerdp_uniconv_free(UNICONV *uniconv);
FREERDP_API char* freerdp_uniconv_in(UNICONV *uniconv, unsigned char* pin, size_t in_len);
FREERDP_API char* freerdp_uniconv_out(UNICONV *uniconv, char *str, size_t *pout_len);
FREERDP_API void freerdp_uniconv_uppercase(UNICONV *uniconv, char *wstr, int length);

#endif /* __UNICODE_UTILS_H */

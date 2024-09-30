/*
 * Copyright © 2014 David FORT <contact@hardening-consulting.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#ifndef UWAC_UTILS_H_
#define UWAC_UTILS_H_

#include <stdlib.h>
#include <string.h>

#include <uwac/config.h>

#define min(a, b) (a) < (b) ? (a) : (b)

#define container_of(ptr, type, member) (type*)((char*)(ptr)-offsetof(type, member))

#define ARRAY_LENGTH(a) (sizeof(a) / sizeof(a)[0])

void* xmalloc(size_t s);

static inline void* zalloc(size_t size)
{
	return calloc(1, size);
}

void* xzalloc(size_t s);

char* xstrdup(const char* s);

void* xrealloc(void* p, size_t s);

static inline char* uwac_strerror(int dw, char* dmsg, size_t size)
{
#ifdef __STDC_LIB_EXT1__
	(void)strerror_s(dw, dmsg, size);
#elif defined(UWAC_HAVE_STRERROR_R)
	(void)strerror_r(dw, dmsg, size);
#else
	(void)_snprintf(dmsg, size, "%s", strerror(dw));
#endif
	return dmsg;
}

#endif /* UWAC_UTILS_H_ */

/*************************************************************************
 *
 * $Id: strio.h,v 1.11 2001/12/27 17:29:20 breese Exp $
 *
 * Copyright (C) 1998 Bjorn Reese and Daniel Stenberg.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE AUTHORS AND
 * CONTRIBUTORS ACCEPT NO RESPONSIBILITY IN ANY CONCEIVABLE MANNER.
 *
 ************************************************************************
 *
 * This maintains backwards compatibility with the strio functions.
 *
 ************************************************************************/

#ifndef TRIO_STRIO_H
#define TRIO_STRIO_H

#if !(defined(DEBUG) || defined(NDEBUG))
# define NDEBUG
#endif
#include "triostr.h"

enum {
  STRIO_HASH_NONE = TRIO_HASH_NONE,
  STRIO_HASH_PLAIN = TRIO_HASH_PLAIN,
  STRIO_HASH_TWOSIGNED = TRIO_HASH_TWOSIGNED
};

#define StrAlloc(n) trio_create(n)
#define StrAppend(x,y) ((void)trio_append((x),(y)),(x))
#define StrAppendMax(x,n,y) ((void)trio_append_max((x),(n),(y)),(x))
#define StrContains(x,y) trio_contains((x),(y))
#define StrCopy(x,y) ((void)trio_copy((x),(y)),(x))
#define StrCopyMax(x,n,y) ((void)trio_copy_max((x),(n),(y)),(x))
#define StrDuplicate(x) trio_duplicate(x)
#define StrDuplicateMax(x,n) trio_duplicate((x),(n))
#define StrEqual(x,y) trio_equal((x),(y))
#define StrEqualCase(x,y) trio_equal_case((x),(y))
#define StrEqualCaseMax(x,n,y) trio_equal_case_max((x),(n),(y))
#define StrEqualLocale(x,y) trio_equal_locale((x),(y))
#define StrEqualMax(x,n,y) trio_equal_max((x),(n),(y))
#define StrError(n) trio_error(n)
#define StrFree(x) trio_destroy(x)
#define StrFormat trio_sprintf
#define StrFormatAlloc trio_aprintf
#define StrFormatAppendMax trio_snprintfcat
#define StrFormatDateMax(x,n,y,t) trio_format_date_max((x),(n),(y),(t))
#define StrFormatMax trio_snprintf
#define StrHash(x,n) trio_hash((x),(n))
#define StrIndex(x,y) trio_index((x),(y))
#define StrIndexLast(x,y) trio_index_last((x),(y))
#define StrLength(x) trio_length((x))
#define StrMatch(x,y) trio_match((x),(y))
#define StrMatchCase(x,y) trio_match_case((x),(y))
#define StrScan trio_sscanf
#define StrSpanFunction(x,f) trio_span_function((x),(f))
#define StrSubstring(x,y) trio_substring((x),(y))
#define StrSubstringMax(x,n,y) trio_substring_max((x),(n),(y))
#define StrToDouble(x,y) trio_to_double((x),(y))
#define StrToFloat(x,y) trio_to_float((x),(y))
#define StrTokenize(x,y) trio_tokenize((x),(y))
#define StrToLong(x,y,n) trio_to_long((x),(y),(n))
#define StrToUnsignedLong(x,y,n) trio_to_unsigned_long((x),(n),(y))
#define StrToUpper(x) trio_upper(x)

#endif /* TRIO_STRIO_H */

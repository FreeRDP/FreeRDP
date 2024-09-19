/*
 * Copyright 2001-2004 Unicode, Inc.
 *
 * Disclaimer
 *
 * This source code is provided as is by Unicode, Inc. No claims are
 * made as to fitness for any particular purpose. No warranties of any
 * kind are expressed or implied. The recipient agrees to determine
 * applicability of information provided. If this file has been
 * purchased on magnetic or optical media from Unicode, Inc., the
 * sole remedy for any claim will be exchange of defective media
 * within 90 days of receipt.
 *
 * Limitations on Rights to Redistribute This Code
 *
 * Unicode, Inc. hereby grants the right to freely use the information
 * supplied in this file in the creation of products supporting the
 * Unicode Standard, and to make copies of this file in any form
 * for internal or external distribution as long as this notice
 * remains attached.
 */

/* ---------------------------------------------------------------------

Conversions between UTF32, UTF-16, and UTF-8. Source code file.
Author: Mark E. Davis, 1994.
Rev History: Rick McGowan, fixes & updates May 2001.
Sept 2001: fixed const & error conditions per
mods suggested by S. Parent & A. Lillich.
June 2002: Tim Dodd added detection and handling of incomplete
source sequences, enhanced error detection, added casts
to eliminate compiler warnings.
July 2003: slight mods to back out aggressive FFFE detection.
Jan 2004: updated switches in from-UTF8 conversions.
Oct 2004: updated to use UNI_MAX_LEGAL_UTF32 in UTF-32 conversions.

See the header file "utf.h" for complete documentation.

------------------------------------------------------------------------ */

#include <winpr/wtypes.h>
#include <winpr/string.h>
#include <winpr/assert.h>

#include "unicode.h"

#include "../log.h"
#define TAG WINPR_TAG("unicode")

/*
 * Character Types:
 *
 * UTF8:		uint8_t		8 bits
 * UTF16:	uint16_t	16 bits
 * UTF32:	uint32_t	32 bits
 */

/* Some fundamental constants */
#define UNI_REPLACEMENT_CHAR (uint32_t)0x0000FFFD
#define UNI_MAX_BMP (uint32_t)0x0000FFFF
#define UNI_MAX_UTF16 (uint32_t)0x0010FFFF
#define UNI_MAX_UTF32 (uint32_t)0x7FFFFFFF
#define UNI_MAX_LEGAL_UTF32 (uint32_t)0x0010FFFF

typedef enum
{
	conversionOK,    /* conversion successful */
	sourceExhausted, /* partial character in source, but hit end */
	targetExhausted, /* insuff. room in target for conversion */
	sourceIllegal    /* source sequence is illegal/malformed */
} ConversionResult;

typedef enum
{
	strictConversion = 0,
	lenientConversion
} ConversionFlags;

static const int halfShift = 10; /* used for shifting by 10 bits */

static const uint32_t halfBase = 0x0010000UL;
static const uint32_t halfMask = 0x3FFUL;

#define UNI_SUR_HIGH_START (uint32_t)0xD800
#define UNI_SUR_HIGH_END (uint32_t)0xDBFF
#define UNI_SUR_LOW_START (uint32_t)0xDC00
#define UNI_SUR_LOW_END (uint32_t)0xDFFF

/* --------------------------------------------------------------------- */

/*
 * Index into the table below with the first byte of a UTF-8 sequence to
 * get the number of trailing bytes that are supposed to follow it.
 * Note that *legal* UTF-8 values can't have 4 or 5-bytes. The table is
 * left as-is for anyone who may want to do such conversion, which was
 * allowed in earlier algorithms.
 */
static const char trailingBytesForUTF8[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5
};

/*
 * Magic values subtracted from a buffer value during UTF8 conversion.
 * This table contains as many values as there might be trailing bytes
 * in a UTF-8 sequence.
 */
static const uint32_t offsetsFromUTF8[6] = { 0x00000000UL, 0x00003080UL, 0x000E2080UL,
	                                         0x03C82080UL, 0xFA082080UL, 0x82082080UL };

/*
 * Once the bits are split out into bytes of UTF-8, this is a mask OR-ed
 * into the first byte, depending on how many bytes follow.  There are
 * as many entries in this table as there are UTF-8 sequence types.
 * (I.e., one byte sequence, two byte... etc.). Remember that sequencs
 * for *legal* UTF-8 will be 4 or fewer bytes total.
 */
static const uint8_t firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

/* --------------------------------------------------------------------- */

/* The interface converts a whole buffer to avoid function-call overhead.
 * Constants have been gathered. Loops & conditionals have been removed as
 * much as possible for efficiency, in favor of drop-through switches.
 * (See "Note A" at the bottom of the file for equivalent code.)
 * If your compiler supports it, the "isLegalUTF8" call can be turned
 * into an inline function.
 */

/* --------------------------------------------------------------------- */

static ConversionResult winpr_ConvertUTF16toUTF8_Internal(const uint16_t** sourceStart,
                                                          const uint16_t* sourceEnd,
                                                          uint8_t** targetStart, uint8_t* targetEnd,
                                                          ConversionFlags flags)
{
	bool computeLength = (!targetEnd) ? true : false;
	const uint16_t* source = *sourceStart;
	uint8_t* target = *targetStart;
	ConversionResult result = conversionOK;

	while (source < sourceEnd)
	{
		uint32_t ch = 0;
		unsigned short bytesToWrite = 0;
		const uint32_t byteMask = 0xBF;
		const uint32_t byteMark = 0x80;
		const uint16_t* oldSource =
		    source; /* In case we have to back up because of target overflow. */

		ch = *source++;

		/* If we have a surrogate pair, convert to UTF32 first. */
		if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END)
		{
			/* If the 16 bits following the high surrogate are in the source buffer... */
			if (source < sourceEnd)
			{
				uint32_t ch2 = *source;

				/* If it's a low surrogate, convert to UTF32. */
				if (ch2 >= UNI_SUR_LOW_START && ch2 <= UNI_SUR_LOW_END)
				{
					ch = ((ch - UNI_SUR_HIGH_START) << halfShift) + (ch2 - UNI_SUR_LOW_START) +
					     halfBase;
					++source;
				}
				else if (flags == strictConversion)
				{
					/* it's an unpaired high surrogate */
					--source; /* return to the illegal value itself */
					result = sourceIllegal;
					break;
				}
			}
			else
			{
				/* We don't have the 16 bits following the high surrogate. */
				--source; /* return to the high surrogate */
				result = sourceExhausted;
				break;
			}
		}
		else if (flags == strictConversion)
		{
			/* UTF-16 surrogate values are illegal in UTF-32 */
			if (ch >= UNI_SUR_LOW_START && ch <= UNI_SUR_LOW_END)
			{
				--source; /* return to the illegal value itself */
				result = sourceIllegal;
				break;
			}
		}

		/* Figure out how many bytes the result will require */
		if (ch < (uint32_t)0x80)
		{
			bytesToWrite = 1;
		}
		else if (ch < (uint32_t)0x800)
		{
			bytesToWrite = 2;
		}
		else if (ch < (uint32_t)0x10000)
		{
			bytesToWrite = 3;
		}
		else if (ch < (uint32_t)0x110000)
		{
			bytesToWrite = 4;
		}
		else
		{
			bytesToWrite = 3;
			ch = UNI_REPLACEMENT_CHAR;
		}

		target += bytesToWrite;

		if ((target > targetEnd) && (!computeLength))
		{
			source = oldSource; /* Back up source pointer! */
			target -= bytesToWrite;
			result = targetExhausted;
			break;
		}

		if (!computeLength)
		{
			switch (bytesToWrite)
			{
					/* note: everything falls through. */
				case 4:
					*--target = (uint8_t)((ch | byteMark) & byteMask);
					ch >>= 6;
					/* fallthrough */
					WINPR_FALLTHROUGH
				case 3:
					*--target = (uint8_t)((ch | byteMark) & byteMask);
					ch >>= 6;
					/* fallthrough */
					WINPR_FALLTHROUGH

				case 2:
					*--target = (uint8_t)((ch | byteMark) & byteMask);
					ch >>= 6;
					/* fallthrough */
					WINPR_FALLTHROUGH

				case 1:
					*--target = (uint8_t)(ch | firstByteMark[bytesToWrite]);
			}
		}
		else
		{
			switch (bytesToWrite)
			{
					/* note: everything falls through. */
				case 4:
					--target;
					/* fallthrough */
					WINPR_FALLTHROUGH

				case 3:
					--target;
					/* fallthrough */
					WINPR_FALLTHROUGH

				case 2:
					--target;
					/* fallthrough */
					WINPR_FALLTHROUGH

				case 1:
					--target;
			}
		}

		target += bytesToWrite;
	}

	*sourceStart = source;
	*targetStart = target;
	return result;
}

/* --------------------------------------------------------------------- */

/*
 * Utility routine to tell whether a sequence of bytes is legal UTF-8.
 * This must be called with the length pre-determined by the first byte.
 * If not calling this from ConvertUTF8to*, then the length can be set by:
 *  length = trailingBytesForUTF8[*source]+1;
 * and the sequence is illegal right away if there aren't that many bytes
 * available.
 * If presented with a length > 4, this returns false.  The Unicode
 * definition of UTF-8 goes up to 4-byte sequences.
 */

static bool isLegalUTF8(const uint8_t* source, int length)
{
	uint8_t a = 0;
	const uint8_t* srcptr = source + length;

	switch (length)
	{
		default:
			return false;

			/* Everything else falls through when "true"... */
		case 4:
			if ((a = (*--srcptr)) < 0x80 || a > 0xBF)
				return false;
			/* fallthrough */
			WINPR_FALLTHROUGH

		case 3:
			if ((a = (*--srcptr)) < 0x80 || a > 0xBF)
				return false;
			/* fallthrough */
			WINPR_FALLTHROUGH

		case 2:
			if ((a = (*--srcptr)) > 0xBF)
				return false;

			switch (*source)
			{
					/* no fall-through in this inner switch */
				case 0xE0:
					if (a < 0xA0)
						return false;

					break;

				case 0xED:
					if (a > 0x9F)
						return false;

					break;

				case 0xF0:
					if (a < 0x90)
						return false;

					break;

				case 0xF4:
					if (a > 0x8F)
						return false;

					break;

				default:
					if (a < 0x80)
						return false;
					break;
			}
			/* fallthrough */
			WINPR_FALLTHROUGH

		case 1:
			if (*source >= 0x80 && *source < 0xC2)
				return false;
	}

	if (*source > 0xF4)
		return false;

	return true;
}

/* --------------------------------------------------------------------- */

static ConversionResult winpr_ConvertUTF8toUTF16_Internal(const uint8_t** sourceStart,
                                                          const uint8_t* sourceEnd,
                                                          uint16_t** targetStart,
                                                          uint16_t* targetEnd,
                                                          ConversionFlags flags)
{
	bool computeLength = (!targetEnd) ? true : false;
	ConversionResult result = conversionOK;
	const uint8_t* source = *sourceStart;
	uint16_t* target = *targetStart;

	while (source < sourceEnd)
	{
		uint32_t ch = 0;
		unsigned short extraBytesToRead = trailingBytesForUTF8[*source];

		if ((source + extraBytesToRead) >= sourceEnd)
		{
			result = sourceExhausted;
			break;
		}

		/* Do this check whether lenient or strict */
		if (!isLegalUTF8(source, extraBytesToRead + 1))
		{
			result = sourceIllegal;
			break;
		}

		/*
		 * The cases all fall through. See "Note A" below.
		 */
		switch (extraBytesToRead)
		{
			case 5:
				ch += *source++;
				ch <<= 6; /* remember, illegal UTF-8 */
				          /* fallthrough */
				WINPR_FALLTHROUGH

			case 4:
				ch += *source++;
				ch <<= 6; /* remember, illegal UTF-8 */
				          /* fallthrough */
				WINPR_FALLTHROUGH

			case 3:
				ch += *source++;
				ch <<= 6;
				/* fallthrough */
				WINPR_FALLTHROUGH

			case 2:
				ch += *source++;
				ch <<= 6;
				/* fallthrough */
				WINPR_FALLTHROUGH

			case 1:
				ch += *source++;
				ch <<= 6;
				/* fallthrough */
				WINPR_FALLTHROUGH

			case 0:
				ch += *source++;
		}

		ch -= offsetsFromUTF8[extraBytesToRead];

		if ((target >= targetEnd) && (!computeLength))
		{
			source -= (extraBytesToRead + 1); /* Back up source pointer! */
			result = targetExhausted;
			break;
		}

		if (ch <= UNI_MAX_BMP)
		{
			/* Target is a character <= 0xFFFF */
			/* UTF-16 surrogate values are illegal in UTF-32 */
			if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END)
			{
				if (flags == strictConversion)
				{
					source -= (extraBytesToRead + 1); /* return to the illegal value itself */
					result = sourceIllegal;
					break;
				}
				else
				{
					if (!computeLength)
						*target++ = UNI_REPLACEMENT_CHAR;
					else
						target++;
				}
			}
			else
			{
				if (!computeLength)
					*target++ = (uint16_t)ch; /* normal case */
				else
					target++;
			}
		}
		else if (ch > UNI_MAX_UTF16)
		{
			if (flags == strictConversion)
			{
				result = sourceIllegal;
				source -= (extraBytesToRead + 1); /* return to the start */
				break;                            /* Bail out; shouldn't continue */
			}
			else
			{
				if (!computeLength)
					*target++ = UNI_REPLACEMENT_CHAR;
				else
					target++;
			}
		}
		else
		{
			/* target is a character in range 0xFFFF - 0x10FFFF. */
			if ((target + 1 >= targetEnd) && (!computeLength))
			{
				source -= (extraBytesToRead + 1); /* Back up source pointer! */
				result = targetExhausted;
				break;
			}

			ch -= halfBase;

			if (!computeLength)
			{
				*target++ = (uint16_t)((ch >> halfShift) + UNI_SUR_HIGH_START);
				*target++ = (uint16_t)((ch & halfMask) + UNI_SUR_LOW_START);
			}
			else
			{
				target++;
				target++;
			}
		}
	}

	*sourceStart = source;
	*targetStart = target;
	return result;
}

/**
 * WinPR built-in Unicode API
 */

static int winpr_ConvertUTF8toUTF16(const uint8_t* src, int cchSrc, uint16_t* dst, int cchDst)
{
	size_t length = 0;
	uint16_t* dstBeg = NULL;
	uint16_t* dstEnd = NULL;
	const uint8_t* srcBeg = NULL;
	const uint8_t* srcEnd = NULL;
	ConversionResult result = sourceIllegal;

	if (cchSrc == -1)
		cchSrc = (int)strnlen((const char*)src, INT32_MAX - 1) + 1;

	srcBeg = src;
	srcEnd = &src[cchSrc];

	if (cchDst == 0)
	{
		result =
		    winpr_ConvertUTF8toUTF16_Internal(&srcBeg, srcEnd, &dstBeg, dstEnd, strictConversion);

		length = dstBeg - (uint16_t*)NULL;
	}
	else
	{
		dstBeg = dst;
		dstEnd = &dst[cchDst];

		result =
		    winpr_ConvertUTF8toUTF16_Internal(&srcBeg, srcEnd, &dstBeg, dstEnd, strictConversion);

		length = dstBeg - dst;
	}

	if (result == targetExhausted)
	{
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return 0;
	}

	return (result == conversionOK) ? length : 0;
}

static int winpr_ConvertUTF16toUTF8(const uint16_t* src, int cchSrc, uint8_t* dst, int cchDst)
{
	size_t length = 0;
	uint8_t* dstBeg = NULL;
	uint8_t* dstEnd = NULL;
	const uint16_t* srcBeg = NULL;
	const uint16_t* srcEnd = NULL;
	ConversionResult result = sourceIllegal;

	if (cchSrc == -1)
		cchSrc = (int)_wcsnlen((const WCHAR*)src, INT32_MAX - 1) + 1;

	srcBeg = src;
	srcEnd = &src[cchSrc];

	if (cchDst == 0)
	{
		result =
		    winpr_ConvertUTF16toUTF8_Internal(&srcBeg, srcEnd, &dstBeg, dstEnd, strictConversion);

		length = dstBeg - ((uint8_t*)NULL);
	}
	else
	{
		dstBeg = dst;
		dstEnd = &dst[cchDst];

		result =
		    winpr_ConvertUTF16toUTF8_Internal(&srcBeg, srcEnd, &dstBeg, dstEnd, strictConversion);

		length = dstBeg - dst;
	}

	if (result == targetExhausted)
	{
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return 0;
	}

	return (result == conversionOK) ? length : 0;
}

/* --------------------------------------------------------------------- */

int int_MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte,
                            LPWSTR lpWideCharStr, int cchWideChar)
{
	size_t cbCharLen = (size_t)cbMultiByte;

	WINPR_UNUSED(dwFlags);

	/* If cbMultiByte is 0, the function fails */
	if ((cbMultiByte == 0) || (cbMultiByte < -1))
		return 0;

	if (cchWideChar < 0)
		return -1;

	if (cbMultiByte < 0)
	{
		const size_t len = strlen(lpMultiByteStr);
		if (len >= INT32_MAX)
			return 0;
		cbCharLen = (int)len + 1;
	}
	else
		cbCharLen = cbMultiByte;

	WINPR_ASSERT(lpMultiByteStr);
	switch (CodePage)
	{
		case CP_ACP:
		case CP_UTF8:
			break;

		default:
			WLog_ERR(TAG, "Unsupported encoding %u", CodePage);
			return 0;
	}

	return winpr_ConvertUTF8toUTF16((const uint8_t*)lpMultiByteStr, cbCharLen,
	                                (uint16_t*)lpWideCharStr, cchWideChar);
}

int int_WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar,
                            LPSTR lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar,
                            LPBOOL lpUsedDefaultChar)
{
	size_t cbCharLen = (size_t)cchWideChar;

	WINPR_UNUSED(dwFlags);
	/* If cchWideChar is 0, the function fails */
	if ((cchWideChar == 0) || (cchWideChar < -1))
		return 0;

	if (cbMultiByte < 0)
		return -1;

	WINPR_ASSERT(lpWideCharStr);
	/* If cchWideChar is -1, the string is null-terminated */
	if (cchWideChar == -1)
	{
		const size_t len = _wcslen(lpWideCharStr);
		if (len >= INT32_MAX)
			return 0;
		cbCharLen = (int)len + 1;
	}
	else
		cbCharLen = cchWideChar;

	/*
	 * if cbMultiByte is 0, the function returns the required buffer size
	 * in bytes for lpMultiByteStr and makes no use of the output parameter itself.
	 */

	return winpr_ConvertUTF16toUTF8((const uint16_t*)lpWideCharStr, cbCharLen,
	                                (uint8_t*)lpMultiByteStr, cbMultiByte);
}

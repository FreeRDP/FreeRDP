/**
 * WinPR: Windows Portable Runtime
 * Print Utils
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

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <winpr/crt.h>
#include <winpr/print.h>

void winpr_HexDump(BYTE* data, int length)
{
	BYTE* p = data;
	int i, line, offset = 0;

	while (offset < length)
	{
		printf("%04x ", offset);

		line = length - offset;

		if (line > WINPR_HEXDUMP_LINE_LENGTH)
			line = WINPR_HEXDUMP_LINE_LENGTH;

		for (i = 0; i < line; i++)
			printf("%02x ", p[i]);

		for (; i < WINPR_HEXDUMP_LINE_LENGTH; i++)
			printf("   ");

		for (i = 0; i < line; i++)
			printf("%c", (p[i] >= 0x20 && p[i] < 0x7F) ? p[i] : '.');

		printf("\n");

		offset += line;
		p += line;
	}
}

/*----------------------------------------------------------------------------
Stripped-down printf()
Chris Giese	<geezer@execpc.com>	http://my.execpc.com/~geezer
Release date: Feb 3, 2008

This code is public domain (no copyright).
You can do whatever you want with it.

%[flag][width][.prec][mod][conv]
flag:	-	left justify, pad right w/ blanks	DONE
	0	pad left w/ 0 for numerics		DONE
	+	always print sign, + or -		no
	' '	(blank)					no
	#	(???)					no

width:		(field width)				DONE

prec:		(precision)				no

conv:	d,i	decimal int				DONE
	u	decimal unsigned			DONE
	o	octal					DONE
	x,X	hex					DONE
	f,e,g,E,G float					no
	c	char					DONE
	s	string					DONE
	p	ptr					DONE

mod:	N	near ptr				DONE
	F	far ptr					no
	h	short (16-bit) int			DONE
	l	long (32-bit) int			DONE
	L	long long (64-bit) int			no
----------------------------------------------------------------------------*/

/* flags used in processing format string */
#define		PR_LJ	0x01	/* left justify */
#define		PR_CA	0x02	/* use A-F instead of a-f for hex */
#define		PR_SG	0x04	/* signed numeric conversion (%d vs. %u) */
#define		PR_32	0x08	/* long (32-bit) numeric conversion */
#define		PR_16	0x10	/* short (16-bit) numeric conversion */
#define		PR_WS	0x20	/* PR_SG set and num was < 0 */
#define		PR_LZ	0x40	/* pad left with '0' instead of ' ' */
#define		PR_FP	0x80	/* pointers are far */

/* largest number handled is 2^32-1, lowest radix handled is 8.
2^32-1 in base 8 has 11 digits (add 5 for trailing NUL and for slop) */
#define		PR_BUFLEN	16

typedef int (*fnptr_t)(unsigned c, void **helper);

int do_printf(const char *fmt, va_list args, fnptr_t fn, void *ptr)
{
	long num;
	unsigned char state, radix;
	unsigned char *where, buf[PR_BUFLEN];
	unsigned flags, actual_wd, count, given_wd;

	state = flags = count = given_wd = 0;

	/* begin scanning format specifier list */

	for (; *fmt; fmt++)
	{
		switch (state)
		{
/* STATE 0: AWAITING % */
		case 0:
			if (*fmt != '%')	/* not %... */
			{
				fn(*fmt, &ptr);	/* ...just echo it */
				count++;
				break;
			}

			/* found %, get next char and advance state to check if next char is a flag */

			state++;
			fmt++;
			/* FALL THROUGH */
/* STATE 1: AWAITING FLAGS (%-0) */
		case 1:
			if (*fmt == '%')	/* %% */
			{
				fn(*fmt, &ptr);
				count++;
				state = flags = given_wd = 0;
				break;
			}
			if (*fmt == '-')
			{
				if (flags & PR_LJ) /* %-- is illegal */
					state = flags = given_wd = 0;
				else
					flags |= PR_LJ;
				break;
			}

			/* not a flag char: advance state to check if it's field width */

			state++;

			/* check now for '%0...' */

			if (*fmt == '0')
			{
				flags |= PR_LZ;
				fmt++;
			}
			/* FALL THROUGH */
/* STATE 2: AWAITING (NUMERIC) FIELD WIDTH */
		case 2:
			if (*fmt >= '0' && *fmt <= '9')
			{
				given_wd = 10 * given_wd + (*fmt - '0');
				break;
			}

			/* not field width: advance state to check if it's a modifier */

			state++;
			/* FALL THROUGH */
/* STATE 3: AWAITING MODIFIER CHARS (FNlh) */
		case 3:
			if (*fmt == 'F')
			{
				flags |= PR_FP;
				break;
			}
			if (*fmt == 'N')
			{
				break;
			}
			if (*fmt == 'l')
			{
				flags |= PR_32;
				break;
			}
			if (*fmt == 'h')
			{
				flags |= PR_16;
				break;
			}

			/* not modifier: advance state to check if it's a conversion char */

			state++;
			/* FALL THROUGH */
/* STATE 4: AWAITING CONVERSION CHARS (Xxpndiuocs) */
		case 4:
			where = buf + PR_BUFLEN - 1;
			*where = '\0';

			switch (*fmt)
			{
			case 'X':
				flags |= PR_CA;
				/* FALL THROUGH */
				/* xxx - far pointers (%Fp, %Fn) not yet supported */
			case 'x':
			case 'p':
			case 'n':
				radix = 16;
				goto DO_NUM;

			case 'd':
			case 'i':
				flags |= PR_SG;
				/* FALL THROUGH */
			case 'u':
				radix = 10;
				goto DO_NUM;

			case 'o':
				radix = 8;
DO_NUM:				if (flags & PR_32)
				{
					/* load the value to be printed. l=long=32 bits: */
					num = va_arg(args, unsigned long);
				}
				else if (flags & PR_16)
				{
					/* h=short=16 bits (signed or unsigned) */

					if (flags & PR_SG)
						num = (short) va_arg(args, int);
					else
						num = (unsigned short) va_arg(args, int);
				}
				else
				{
					/* no h nor l: sizeof(int) bits (signed or unsigned) */

					if (flags & PR_SG)
						num = va_arg(args, int);
					else
						num = va_arg(args, unsigned int);
				}

				if (flags & PR_SG)
				{
					/* take care of sign */

					if (num < 0)
					{
						flags |= PR_WS;
						num = -num;
					}
				}

				/*
				 * convert binary to octal/decimal/hex ASCII
				 * OK, I found my mistake. The math here is _always_ unsigned
				 */

				do
				{
					unsigned long temp;

					temp = (unsigned long) num % radix;
					where--;

					if (temp < 10)
						*where = temp + '0';
					else if (flags & PR_CA)
						*where = temp - 10 + 'A';
					else
						*where = temp - 10 + 'a';

					num = (unsigned long) num / radix;
				}
				while (num != 0);

				goto EMIT;

			case 'c':
				/* disallow pad-left-with-zeroes for %c */
				flags &= ~PR_LZ;
				where--;
				*where = (unsigned char) va_arg(args, int);
				actual_wd = 1;
				goto EMIT2;

			case 's':
				/* disallow pad-left-with-zeroes for %s */
				flags &= ~PR_LZ;
				where = va_arg(args, unsigned char*);
EMIT:
				actual_wd = strlen((char*) where);

				if (flags & PR_WS)
					actual_wd++;

				/* if we pad left with ZEROES, do the sign now */

				if ((flags & (PR_WS | PR_LZ)) == (PR_WS | PR_LZ))
				{
					fn('-', &ptr);
					count++;
				}

				/* pad on left with spaces or zeroes (for right justify) */

EMIT2:				if ((flags & PR_LJ) == 0)
				{
					while (given_wd > actual_wd)
					{
						fn((flags & PR_LZ) ? '0' : ' ', &ptr);
						count++;
						given_wd--;
					}
				}

				/* if we pad left with SPACES, do the sign now */

				if ((flags & (PR_WS | PR_LZ)) == PR_WS)
				{
					fn('-', &ptr);
					count++;
				}

				/* emit string/char/converted number */

				while (*where != '\0')
				{
					fn(*where++, &ptr);
					count++;
				}

				/* pad on right with spaces (for left justify) */

				if (given_wd < actual_wd)
					given_wd = 0;
				else
					given_wd -= actual_wd;

				for (; given_wd; given_wd--)
				{
					fn(' ', &ptr);
					count++;
				}

				break;

			default:
				break;
			}

		default:
			state = flags = given_wd = 0;
			break;
		}
	}

	return count;
}

static int vsprintf_help(unsigned c, void **ptr)
{
	char *dst;

	dst = *ptr;
	*dst++ = (char) c;
	*ptr = dst;

	return 0;
}

int wvsprintfx(char *buf, const char *fmt, va_list args)
{
	int status;

	status = do_printf(fmt, args, vsprintf_help, (void*) buf);
	buf[status] = '\0';

	return status;
}

static int discard(unsigned c_UNUSED, void **ptr_UNUSED)
{
	return 0;
}

int wsprintfx(char *buf, const char *fmt, ...)
{
	va_list args;
	int status;

	va_start(args, fmt);

	if (!buf)
		status = do_printf(fmt, args, discard, NULL);
	else
		status = wvsprintfx(buf, fmt, args);

	va_end(args);

	return status;
}

int vprintf_help(unsigned c, void **ptr_UNUSED)
{
	putchar(c);
	return 0;
}

int wvprintfx(const char *fmt, va_list args)
{
	return do_printf(fmt, args, vprintf_help, NULL);
}

int wprintfx(const char *fmt, ...)
{
	va_list args;
	int status;

	va_start(args, fmt);
	status = wvprintfx(fmt, args);
	va_end(args);

	return status;
}

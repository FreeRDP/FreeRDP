/*
 * PAM-PKCS11 string tools
 * Copyright (C) 2005 Juan Antonio Martinez <jonsito@teleline.es>
 * pam-pkcs11 is copyright (C) 2003-2004 of Mario Strasser <mast@gmx.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * $Id$
 */

#ifndef __STRINGS_H_
#define __STRINGS_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

/**
* String management library
*/
#ifndef _STRINGS_C_
#define M_EXTERN extern
#else
#define M_EXTERN
#endif

/**
 * Check for a null or spaced string
 *@param str Tested string
 *@return nonzero on null, empty or spaced string, else zero
 */
M_EXTERN int is_empty_str(const char *str);

/**
 * Duplicate a string
 *@param str String to be cloned
 *@return Pointer to cloned string or null if error in allocating memory
 */
M_EXTERN char *clone_str(const char *str);

/**
 * Duplicate a string converting all chars to upper-case
 *@param str String to be cloned & uppercassed
 *@return Pointer to result string or null if error in allocating memory
 */
M_EXTERN char *toupper_str(const char *str);

/**
 * Duplicate a string converting all chars to lower-case
 *@param str String to be cloned & lowercased
 *@return Pointer to result string or null if error in allocating memory
 */
M_EXTERN char *tolower_str(const char *str);

/**
 * Convert a byte array into a colon-separated hexadecimal sequence
 *@param binstr ByteArray to be parsed
 *@param len Number of bytes to be converted
 *@return Pointer to result string or null if error in allocating memory
 */
M_EXTERN char *bin2hex(const unsigned char *binstr,const int len);

/**
 * Convert a colon-separated hexadecimal data into a byte array
 *@param hexstr String to be parsed
 *@return Pointer to resulting byte array, or null if no memory available
 */
M_EXTERN unsigned char *hex2bin(const char *hexstr);

/**
 * Convert a colon-separated hexadecimal data into a byte array,
 * store result into a previously allocated space
 *@param hexstr String to be parsed
 *@param res Pointer to pre-allocated user space
 *@param size Pointer to store lenght of data parsed
 *@return Pointer to resulting byte array, or null on parse error
 */
M_EXTERN unsigned char *hex2bin_static(const char *hexstr,unsigned char **res,int *size);

/**
 * Splits a string to an array of nelems by using sep as character separator.
 *
 * To free() memory used by this call, call free(res[0]); free(res);
 *@param str String to be parsed
 *@param sep Character to be used as separator
 *@param nelems Number of elements of resulting array
 *@return res: Pointer to resulting string array, or null if malloc() error
 */
M_EXTERN char **split(const char *str,char sep, int nelems);

/**
 * Splits a string to an array of nelems by using sep as character separator,
 * using dest as pre-allocated destination memory for the resulting array
 *
 * To free() memory used by this call, just call free result pointer
 *@param str String to be parsed
 *@param sep Character to be used as separator
 *@param nelems Number of elements of resulting array
 *@param dst Char array to store temporary data
 *@return Pointer to resulting string array, or null if malloc() error
 */
M_EXTERN char **split_static(const char *str,char sep, int nelems,char *dst);

/**
 * Remove all extra spaces from a string.
 * a char is considered space if trues isspace()
 *
 *@param str String to be trimmed
 *@return Pointer to cloned string with all spaces trimmed or null if error in allocating memory
 */
M_EXTERN char *trim(const char *str);

#undef M_EXTERN

#endif

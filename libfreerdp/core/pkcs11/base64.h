/*
 * BASE64 Encoding funtions
 * Copyright (C) 2001, 2002  Juha Yrj\uffffl\uffff <juha.yrjola@iki.fi>
 * Copyright (C) 2003-2004 Mario Strasser <mast@gmx.net>
 * Copyright (C) 2005 Juan Antonio Martinez <jonsito@teleline.es>
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
 * $Id$
 */

#ifndef __BASE64_H_
#define __BASE64_H_

#ifndef __BASE64_C_
#define BASE64_EXTERN extern
#else
#define BASE64_EXTERN
#endif

/**
* Encode byte array into a base64 string
*@param in Pointer to byte array
*@param len lenght of input data
*@param out Pointer to preallocated buffer space
*@param outlen Size of buffer
*@return 0 on sucess, -1 on error
*/
BASE64_EXTERN int base64_encode(const unsigned char *in, size_t len, unsigned char *out, size_t *outlen);

/**
* Decode a base64 string into a byte array
*@param in Input string data
*@param out Pointer to pre-allocated buffer space
*@param outlen Size of buffer
*@return Length of converted byte array, or -1 on error
*/
BASE64_EXTERN int base64_decode(const char *in, unsigned char *out, size_t outlen);

#undef BASE64_EXTERN

#endif /* __BASE64_H_ */



/*
 * This is an OpenSSL-compatible implementation of the RSA Data Security, Inc.
 * MD5 Message-Digest Algorithm (RFC 1321).
 *
 * Homepage:
 * http://openwall.info/wiki/people/solar/software/public-domain-source-code/md5
 *
 * Author:
 * Alexander Peslyak, better known as Solar Designer <solar at openwall.com>
 *
 * This software was written by Alexander Peslyak in 2001.  No copyright is
 * claimed, and the software is hereby placed in the public domain.
 * In case this attempt to disclaim copyright and place the software in the
 * public domain is deemed null and void, then the software is
 * Copyright (c) 2001 Alexander Peslyak and it is hereby released to the
 * general public under the following terms:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 *
 * See md5.c for more information.
 */

#if !defined(WINPR_MD5_H)
#define WINPR_MD5_H

#include <winpr/wtypes.h>

/* Any 32-bit or wider unsigned integer data type will do */
typedef UINT32 winpr_MD5_u32plus;

typedef struct
{
	winpr_MD5_u32plus lo, hi;
	winpr_MD5_u32plus a, b, c, d;
	unsigned char buffer[64];
	winpr_MD5_u32plus block[16];
} WINPR_MD5_CTX;

extern void winpr_MD5_Init(WINPR_MD5_CTX* ctx);
extern void winpr_MD5_Update(WINPR_MD5_CTX* ctx, const void* data, unsigned long size);
extern void winpr_MD5_Final(unsigned char* result, WINPR_MD5_CTX* ctx);

#endif

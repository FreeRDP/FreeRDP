/*
 * This is an OpenSSL-compatible implementation of the RSA Data Security, Inc.
 * MD4 Message-Digest Algorithm (RFC 1320).
 *
 * Homepage:
 * http://openwall.info/wiki/people/solar/software/public-domain-source-code/md4
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
 * See md4.c for more information.
 */

#if !defined(WINPR_MD4_H)
#define WINPR_MD4_H

#include <winpr/wtypes.h>

/* Any 32-bit or wider unsigned integer data type will do */
typedef UINT32 winpr_MD4_u32plus;

typedef struct
{
	winpr_MD4_u32plus lo, hi;
	winpr_MD4_u32plus a, b, c, d;
	unsigned char buffer[64];
	winpr_MD4_u32plus block[16];
} WINPR_MD4_CTX;

extern void winpr_MD4_Init(WINPR_MD4_CTX* ctx);
extern void winpr_MD4_Update(WINPR_MD4_CTX* ctx, const void* data, unsigned long size);
extern void winpr_MD4_Final(unsigned char* result, WINPR_MD4_CTX* ctx);

#endif

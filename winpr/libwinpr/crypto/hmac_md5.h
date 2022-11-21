#ifndef WINPR_HMAC_MD5
#define WINPR_HMAC_MD5

#include <stddef.h>

#include "md5.h"

#define KEY_IOPAD_SIZE 64
typedef struct
{
	WINPR_MD5_CTX icontext;
	WINPR_MD5_CTX ocontext;
} WINPR_HMAC_MD5_CTX;

void hmac_md5_init(WINPR_HMAC_MD5_CTX* ctx, const unsigned char* key, size_t key_len);
void hmac_md5_update(WINPR_HMAC_MD5_CTX* ctx, const unsigned char* text, size_t text_len);
void hmac_md5_finalize(WINPR_HMAC_MD5_CTX* ctx, unsigned char* hmac);

#endif

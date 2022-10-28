#include <assert.h>
#include <string.h>

#include "hmac_md5.h"
#include "md5.h"
#include <string.h>

void hmac_md5_init(WINPR_HMAC_MD5_CTX* ctx, const unsigned char* key, size_t key_len)
{
	const WINPR_HMAC_MD5_CTX empty = { 0 };
	unsigned char k_ipad[KEY_IOPAD_SIZE] = { 0 };
	unsigned char k_opad[KEY_IOPAD_SIZE] = { 0 };

	assert(ctx);
	*ctx = empty;

	if (key_len <= KEY_IOPAD_SIZE)
	{
		memcpy(k_ipad, key, key_len);
		memcpy(k_opad, key, key_len);
	}
	else
	{
		WINPR_MD5_CTX lctx = { 0 };

		winpr_MD5_Init(&lctx);
		winpr_MD5_Update(&lctx, key, key_len);
		winpr_MD5_Final(k_ipad, &lctx);
		memcpy(k_opad, k_ipad, KEY_IOPAD_SIZE);
	}
	for (size_t i = 0; i < KEY_IOPAD_SIZE; i++)
	{
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}

	winpr_MD5_Init(&ctx->icontext);
	winpr_MD5_Update(&ctx->icontext, k_ipad, KEY_IOPAD_SIZE);

	winpr_MD5_Init(&ctx->ocontext);
	winpr_MD5_Update(&ctx->ocontext, k_opad, KEY_IOPAD_SIZE);
}

void hmac_md5_update(WINPR_HMAC_MD5_CTX* ctx, const unsigned char* text, size_t text_len)
{
	assert(ctx);
	winpr_MD5_Update(&ctx->icontext, text, text_len);
}

void hmac_md5_finalize(WINPR_HMAC_MD5_CTX* ctx, unsigned char* hmac)
{
	assert(ctx);
	winpr_MD5_Final(hmac, &ctx->icontext);

	winpr_MD5_Update(&ctx->ocontext, hmac, 16);
	winpr_MD5_Final(hmac, &ctx->ocontext);
}

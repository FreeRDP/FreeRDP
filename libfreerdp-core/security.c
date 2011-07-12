/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RDP Security
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "security.h"

void security_salted_hash(uint8* salt, uint8* input, int length, uint8* client_random, uint8* server_random, uint8* output)
{
	CryptoMd5 md5;
	CryptoSha1 sha1;
	uint8 sha1_digest[20];

	/* SaltedHash(Salt, Input) = MD5(S + SHA1(Input + Salt + ClientRandom + ServerRandom)) */

	/* SHA1_Digest = SHA1(Input + Salt + ClientRandom + ServerRandom) */
	sha1 = crypto_sha1_init();
	crypto_sha1_update(sha1, input, length); /* Input */
	crypto_sha1_update(sha1, salt, 48); /* Salt */
	crypto_sha1_update(sha1, client_random, 32); /* ClientRandom */
	crypto_sha1_update(sha1, server_random, 32); /* ServerRandom */
	crypto_sha1_final(sha1, sha1_digest);

	/* SaltedHash(S, I) = MD5(S + SHA1_Digest) */
	md5 = crypto_md5_init();
	crypto_md5_update(md5, salt, 48); /* Salt */
	crypto_md5_update(md5, sha1_digest, 20); /* SHA1_Digest */
	crypto_md5_final(md5, output);
}

void security_premaster_hash(uint8* input, int length, uint8* premaster_secret, uint8* client_random, uint8* server_random, uint8* output)
{
	/* PremasterHash(Input) = SaltedHash(PremasterSecret, Input) */
	security_salted_hash(premaster_secret, input, length, client_random, server_random, output);
}

void security_master_secret(uint8* premaster_secret, uint8* client_random, uint8* server_random, uint8* output)
{
	/* MasterSecret = PremasterHash('A') + PremasterHash('BB') + PremasterHash('CCC') */
	security_premaster_hash("A", 1, premaster_secret, client_random, server_random, &output[0]);
	security_premaster_hash("BB", 2, premaster_secret, client_random, server_random, &output[16]);
	security_premaster_hash("CCC", 3, premaster_secret, client_random, server_random, &output[32]);
}

void security_master_hash(uint8* input, int length, uint8* master_secret, uint8* client_random, uint8* server_random, uint8* output)
{
	/* MasterHash(Input) = SaltedHash(MasterSecret, Input) */
	security_salted_hash(master_secret, input, length, client_random, server_random, output);
}

void security_session_key_blob(uint8* master_secret, uint8* client_random, uint8* server_random, uint8* output)
{
	/* MasterHash = MasterHash('A') + MasterHash('BB') + MasterHash('CCC') */
	security_master_hash("A", 1, master_secret, client_random, server_random, &output[0]);
	security_master_hash("BB", 2, master_secret, client_random, server_random, &output[16]);
	security_master_hash("CCC", 3, master_secret, client_random, server_random, &output[32]);
}

void security_mac_salt_key(uint8* session_key_blob, uint8* client_random, uint8* server_random, uint8* output)
{
	/* MacSaltKey = First128Bits(SessionKeyBlob) */
	memcpy(output, session_key_blob, 16);
}

void security_licensing_encryption_key(uint8* session_key_blob, uint8* client_random, uint8* server_random, uint8* output)
{
	CryptoMd5 md5;

	/* LicensingEncryptionKey = MD5(Second128Bits(SessionKeyBlob) + ClientRandom + ServerRandom)) */

	md5 = crypto_md5_init();
	crypto_md5_update(md5, &session_key_blob[16], 16); /* Second128Bits(SessionKeyBlob) */
	crypto_md5_update(md5, client_random, 32); /* ClientRandom */
	crypto_md5_update(md5, server_random, 32); /* ServerRandom */
	crypto_md5_final(md5, output);
}


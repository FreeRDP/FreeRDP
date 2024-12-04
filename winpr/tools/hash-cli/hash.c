/**
 * WinPR: Windows Portable Runtime
 * NTLM Hashing Tool
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <winpr/ntlm.h>
#include <winpr/ssl.h>
#include <winpr/assert.h>

/**
 * Define NTOWFv1(Password, User, Domain) as
 * 	MD4(UNICODE(Password))
 * EndDefine
 *
 * Define LMOWFv1(Password, User, Domain) as
 * 	ConcatenationOf(DES(UpperCase(Password)[0..6], "KGS!@#$%"),
 * 		DES(UpperCase(Password)[7..13], "KGS!@#$%"))
 * EndDefine
 *
 * Define NTOWFv2(Password, User, Domain) as
 * 	HMAC_MD5(MD4(UNICODE(Password)),
 * 		UNICODE(ConcatenationOf(UpperCase(User), Domain)))
 * EndDefine
 *
 * Define LMOWFv2(Password, User, Domain) as
 * 	NTOWFv2(Password, User, Domain)
 * EndDefine
 *
 */

static int usage_and_exit(void)
{
	printf("winpr-hash: NTLM hashing tool\n");
	printf("Usage: winpr-hash -u <username> -p <password> [-d <domain>] [-f <_default_,sam>] [-v "
	       "<_1_,2>]\n");
	return 1;
}

int main(int argc, char* argv[])
{
	int index = 1;
	int format = 0;
	unsigned long version = 1;
	BYTE NtHash[16];
	char* User = NULL;
	size_t UserLength = 0;
	char* Domain = NULL;
	size_t DomainLength = 0;
	char* Password = NULL;
	size_t PasswordLength = 0;
	errno = 0;

	while (index < argc)
	{
		if (strcmp("-d", argv[index]) == 0)
		{
			index++;

			if (index == argc)
			{
				printf("missing domain\n\n");
				return usage_and_exit();
			}

			Domain = argv[index];
		}
		else if (strcmp("-u", argv[index]) == 0)
		{
			index++;

			if (index == argc)
			{
				printf("missing username\n\n");
				return usage_and_exit();
			}

			User = argv[index];
		}
		else if (strcmp("-p", argv[index]) == 0)
		{
			index++;

			if (index == argc)
			{
				printf("missing password\n\n");
				return usage_and_exit();
			}

			Password = argv[index];
		}
		else if (strcmp("-v", argv[index]) == 0)
		{
			index++;

			if (index == argc)
			{
				printf("missing version parameter\n\n");
				return usage_and_exit();
			}

			version = strtoul(argv[index], NULL, 0);

			if (((version != 1) && (version != 2)) || (errno != 0))
			{
				printf("unknown version %lu \n\n", version);
				return usage_and_exit();
			}
		}
		else if (strcmp("-f", argv[index]) == 0)
		{
			index++;

			if (index == argc)
			{
				printf("missing format\n\n");
				return usage_and_exit();
			}

			if (strcmp("default", argv[index]) == 0)
				format = 0;
			else if (strcmp("sam", argv[index]) == 0)
				format = 1;
		}
		else if (strcmp("-h", argv[index]) == 0)
		{
			return usage_and_exit();
		}

		index++;
	}

	if ((!User) || (!Password))
	{
		printf("missing username or password\n\n");
		return usage_and_exit();
	}
	winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);

	UserLength = strlen(User);
	PasswordLength = strlen(Password);
	DomainLength = (Domain) ? strlen(Domain) : 0;

	WINPR_ASSERT(UserLength <= UINT32_MAX);
	WINPR_ASSERT(PasswordLength <= UINT32_MAX);
	WINPR_ASSERT(DomainLength <= UINT32_MAX);

	if (version == 2)
	{
		if (!Domain)
		{
			printf("missing domain (version 2 requires a domain to specified)\n\n");
			return usage_and_exit();
		}

		if (!NTOWFv2A(Password, (UINT32)PasswordLength, User, (UINT32)UserLength, Domain,
		              (UINT32)DomainLength, NtHash))
		{
			(void)fprintf(stderr, "Hash creation failed\n");
			return 1;
		}
	}
	else
	{
		if (!NTOWFv1A(Password, (UINT32)PasswordLength, NtHash))
		{
			(void)fprintf(stderr, "Hash creation failed\n");
			return 1;
		}
	}

	if (format == 0)
	{
		for (int idx = 0; idx < 16; idx++)
			printf("%02" PRIx8 "", NtHash[idx]);

		printf("\n");
	}
	else if (format == 1)
	{
		printf("%s:", User);

		if (DomainLength > 0)
			printf("%s:", Domain);
		else
			printf(":");

		printf(":");

		for (int idx = 0; idx < 16; idx++)
			printf("%02" PRIx8 "", NtHash[idx]);

		printf(":::");
		printf("\n");
	}

	return 0;
}

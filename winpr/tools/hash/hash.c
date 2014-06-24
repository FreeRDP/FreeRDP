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

#include <winpr/ntlm.h>

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

int main(int argc, char* argv[])
{
	int index = 1;
	int format = 0;
	int version = 1;
	BYTE NtHash[16];
	char* User = NULL;
	UINT32 UserLength;
	char* Domain = NULL;
	UINT32 DomainLength;
	char* Password = NULL;
	UINT32 PasswordLength;

	while (index < argc)
	{
		if (strcmp("-d", argv[index]) == 0)
		{
			index++;

			if (index == argc)
			{
				printf("missing domain\n");
				exit(1);
			}

			Domain = argv[index];
		}
		else if (strcmp("-u", argv[index]) == 0)
		{
			index++;

			if (index == argc)
			{
				printf("missing username\n");
				exit(1);
			}

			User = argv[index];
		}
		else if (strcmp("-p", argv[index]) == 0)
		{
			index++;

			if (index == argc)
			{
				printf("missing password\n");
				exit(1);
			}

			Password = argv[index];
		}
		else if (strcmp("-v", argv[index]) == 0)
		{
			index++;

			if (index == argc)
			{
				printf("missing version\n");
				exit(1);
			}

			version = atoi(argv[index]);

			if ((version != 1) && (version != 2))
				version = 1;
		}
		else if (strcmp("-f", argv[index]) == 0)
		{
			index++;

			if (index == argc)
			{
				printf("missing format\n");
				exit(1);
			}

			if (strcmp("default", argv[index]) == 0)
				format = 0;
			else if (strcmp("sam", argv[index]) == 0)
				format = 1;
		}
		else if (strcmp("-h", argv[index]) == 0)
		{
			printf("winpr-hash: NTLM hashing tool\n");
			printf("Usage: winpr-hash -u <username> -p <password> [-d <domain>] -f <default,sam> -v <1,2>\n");
			exit(1);
		}

		index++;
	}

	if ((!User) || (!Password))
	{
		printf("missing username or password\n");
		exit(1);
	}

	UserLength = strlen(User);
	PasswordLength = strlen(Password);
	DomainLength = (Domain) ? strlen(Domain) : 0;

	if (version == 2)
	{
		if (!Domain)
		{
			printf("missing domain\n");
			exit(1);
		}

		NTOWFv2A(Password, PasswordLength, User, UserLength, Domain, DomainLength, NtHash);
	}
	else
	{
		NTOWFv1A(Password, PasswordLength, NtHash);
	}

	if (format == 0)
	{
		for (index = 0; index < 16; index++)
			printf("%02x", NtHash[index]);
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

		for (index = 0; index < 16; index++)
			printf("%02x", NtHash[index]);

		printf(":::");
		printf("\n");
	}

	return 0;
}

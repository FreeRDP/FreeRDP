/**
 * WinPR: Windows Portable Runtime
 * Buffer Manipulation
 *
 * Copyright 2025 David Fort <contact@hardening-consulting.com>
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
#include <string.h>
#include <winpr/cred.h>

typedef struct
{
	LPCSTR marshalled;
	BYTE source[CERT_HASH_LENGTH];
} TestItem;

static const TestItem testValues[] = {
	{ "@@BQ9eNR0KWVU-CT8sPCp8z37POZHJ",
	  { 0x50, 0xef, 0x35, 0x11, 0xad, 0x58, 0x15, 0xf5, 0x0b, 0x13,
	    0xcf, 0x3e, 0x42, 0xca, 0xcf, 0xf7, 0xfe, 0x38, 0xd9, 0x91 } },
	{ "@@BKay-HwJsFZzclXAWZ#nO6Eluc7P",
	  { 0x8a, 0x26, 0xff, 0x07, 0x9c, 0xb0, 0x45, 0x36, 0x73, 0xe5,
	    0x05, 0x58, 0x99, 0x7f, 0x3a, 0x3a, 0x51, 0xba, 0xdc, 0xfe }

	}
};

static int TestUnmarshal(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char** argv)
{

	for (size_t i = 0; i < ARRAYSIZE(testValues); i++)
	{
		CRED_MARSHAL_TYPE t = BinaryBlobForSystem;
		CERT_CREDENTIAL_INFO* certInfo = NULL;
		const TestItem* const val = &testValues[i];

		if (!CredUnmarshalCredentialA(val->marshalled, &t, (void**)&certInfo) || !certInfo ||
		    (t != CertCredential))
			return -1;

		const BOOL ok =
		    memcmp(val->source, certInfo->rgbHashOfCert, sizeof(certInfo->rgbHashOfCert)) == 0;

		free(certInfo);

		if (!ok)
			return -1;
	}
	return 0;
}

static int TestMarshal(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char** argv)
{

	for (size_t i = 0; i < ARRAYSIZE(testValues); i++)
	{
		CERT_CREDENTIAL_INFO certInfo = { sizeof(certInfo), { 0 } };
		const TestItem* const val = &testValues[i];

		memcpy(certInfo.rgbHashOfCert, val->source, sizeof(certInfo.rgbHashOfCert));
		LPSTR out = NULL;

		if (!CredMarshalCredentialA(CertCredential, &certInfo, &out) || !out)
			return -1;

		BOOL ok = (strcmp(val->marshalled, out) == 0);

		free(out);

		if (!ok)
			return -1;
	}
	return 0;
}

int TestMarshalUnmarshal(int argc, char** argv)
{
	int ret = TestUnmarshal(argc, argv);
	if (ret)
		return ret;

	ret = TestMarshal(argc, argv);
	return ret;
}

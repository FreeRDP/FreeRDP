/**
 * WinPR: Windows Portable Runtime
 * makecert replacement
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

#include <errno.h>

#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/file.h>
#include <winpr/cmdline.h>
#include <winpr/sysinfo.h>
#include <winpr/crypto.h>
#include <winpr/file.h>

#ifdef WITH_OPENSSL
#include <openssl/crypto.h>
#include <openssl/conf.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/rsa.h>
#include <openssl/pkcs12.h>
#include <openssl/x509v3.h>
#endif

#include <winpr/tools/makecert.h>

struct S_MAKECERT_CONTEXT
{
	int argc;
	char** argv;

#ifdef WITH_OPENSSL
	RSA* rsa;
	X509* x509;
	EVP_PKEY* pkey;
	PKCS12* pkcs12;
#endif

	BOOL live;
	BOOL silent;

	BOOL crtFormat;
	BOOL pemFormat;
	BOOL pfxFormat;

	char* password;

	char* output_file;
	char* output_path;
	char* default_name;
	char* common_name;

	int duration_years;
	int duration_months;
};

static char* makecert_read_str(BIO* bio, size_t* pOffset)
{
	int status = -1;
	size_t offset = 0;
	size_t length = 0;
	char* x509_str = NULL;

	while (offset >= length)
	{
		size_t new_len;
		size_t readBytes = 0;
		char* new_str;
		new_len = length * 2;
		if (new_len == 0)
			new_len = 2048;

		if (new_len > INT_MAX)
		{
			status = -1;
			break;
		}

		new_str = (char*)realloc(x509_str, new_len);

		if (!new_str)
		{
			status = -1;
			break;
		}

		length = new_len;
		x509_str = new_str;
		ERR_clear_error();
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
		status = BIO_read_ex(bio, &x509_str[offset], length - offset, &readBytes);
#else
		status = BIO_read(bio, &x509_str[offset], length - offset);
		readBytes = status;
#endif
		if (status <= 0)
			break;

		offset += (size_t)readBytes;
	}

	if (status < 0)
	{
		free(x509_str);
		if (pOffset)
			*pOffset = 0;
		return NULL;
	}

	x509_str[offset] = '\0';
	if (pOffset)
		*pOffset = offset + 1;
	return x509_str;
}

static int makecert_print_command_line_help(COMMAND_LINE_ARGUMENT_A* args, int argc, char** argv)
{
	char* str;
	const COMMAND_LINE_ARGUMENT_A* arg;

	if (!argv || (argc < 1))
		return -1;

	printf("Usage: %s [options] [output file]\n", argv[0]);
	printf("\n");
	arg = args;

	do
	{
		if (arg->Flags & COMMAND_LINE_VALUE_FLAG)
		{
			printf("    %s", "-");
			printf("%-20s", arg->Name);
			printf("\t%s\n", arg->Text);
		}
		else if ((arg->Flags & COMMAND_LINE_VALUE_REQUIRED) ||
		         (arg->Flags & COMMAND_LINE_VALUE_OPTIONAL))
		{
			printf("    %s", "-");

			if (arg->Format)
			{
				size_t length = strlen(arg->Name) + strlen(arg->Format) + 2;
				str = malloc(length + 1);

				if (!str)
					return -1;

				sprintf_s(str, length + 1, "%s %s", arg->Name, arg->Format);
				printf("%-20s", str);
				free(str);
			}
			else
			{
				printf("%-20s", arg->Name);
			}

			printf("\t%s\n", arg->Text);
		}
	} while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return 1;
}

#ifdef WITH_OPENSSL
static int x509_add_ext(X509* cert, int nid, char* value)
{
	X509V3_CTX ctx;
	X509_EXTENSION* ext;

	if (!cert || !value)
		return 0;

	X509V3_set_ctx_nodb(&ctx) X509V3_set_ctx(&ctx, cert, cert, NULL, NULL, 0);
	ext = X509V3_EXT_conf_nid(NULL, &ctx, nid, value);

	if (!ext)
		return 0;

	X509_add_ext(cert, ext, -1);
	X509_EXTENSION_free(ext);
	return 1;
}
#endif

static char* x509_name_parse(char* name, char* txt, size_t* length)
{
	char* p;
	char* entry;

	if (!name || !txt || !length)
		return NULL;

	p = strstr(name, txt);

	if (!p)
		return NULL;

	entry = p + strlen(txt) + 1;
	p = strchr(entry, '=');

	if (!p)
		*length = strlen(entry);
	else
		*length = (size_t)(p - entry);

	return entry;
}

static char* x509_get_default_name(void)
{
	CHAR* computerName = NULL;
	DWORD nSize = 0;

	if (GetComputerNameExA(ComputerNamePhysicalDnsFullyQualified, NULL, &nSize) ||
	    GetLastError() != ERROR_MORE_DATA)
		goto fallback;

	computerName = (CHAR*)calloc(1, nSize);

	if (!computerName)
		goto fallback;

	if (!GetComputerNameExA(ComputerNamePhysicalDnsFullyQualified, computerName, &nSize))
		goto fallback;

	return computerName;
fallback:
	free(computerName);

	if (GetComputerNameExA(ComputerNamePhysicalNetBIOS, NULL, &nSize) ||
	    GetLastError() != ERROR_MORE_DATA)
		return NULL;

	computerName = (CHAR*)calloc(1, nSize);

	if (!computerName)
		return NULL;

	if (!GetComputerNameExA(ComputerNamePhysicalNetBIOS, computerName, &nSize))
	{
		free(computerName);
		return NULL;
	}

	return computerName;
}

static int command_line_pre_filter(MAKECERT_CONTEXT* context, int index, int argc, LPCSTR* argv)
{
	if (!context || !argv || (index < 0) || (argc < 0))
		return -1;

	if (index == (argc - 1))
	{
		if (argv[index][0] != '-')
		{
			context->output_file = _strdup(argv[index]);

			if (!context->output_file)
				return -1;

			return 1;
		}
	}

	return 0;
}

static int makecert_context_parse_arguments(MAKECERT_CONTEXT* context,
                                            COMMAND_LINE_ARGUMENT_A* args, int argc, char** argv)
{
	int status;
	DWORD flags;
	const COMMAND_LINE_ARGUMENT_A* arg;

	if (!context || !argv || (argc < 0))
		return -1;

	/**
	 * makecert -r -pe -n "CN=%COMPUTERNAME%" -eku 1.3.6.1.5.5.7.3.1 -ss my -sr LocalMachine
	 * -sky exchange -sp "Microsoft RSA SChannel Cryptographic Provider" -sy 12
	 */
	CommandLineClearArgumentsA(args);
	flags = COMMAND_LINE_SEPARATOR_SPACE | COMMAND_LINE_SIGIL_DASH;
	status =
	    CommandLineParseArgumentsA(argc, argv, args, flags, context,
	                               (COMMAND_LINE_PRE_FILTER_FN_A)command_line_pre_filter, NULL);

	if (status & COMMAND_LINE_STATUS_PRINT_HELP)
	{
		makecert_print_command_line_help(args, argc, argv);
		return 0;
	}

	arg = args;
	errno = 0;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
			continue;

		CommandLineSwitchStart(arg)
		    /* Basic Options */
		    CommandLineSwitchCase(arg, "silent")
		{
			context->silent = TRUE;
		}
		CommandLineSwitchCase(arg, "live")
		{
			context->live = TRUE;
		}
		CommandLineSwitchCase(arg, "format")
		{
			if (!(arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
				continue;

			if (strcmp(arg->Value, "crt") == 0)
			{
				context->crtFormat = TRUE;
				context->pemFormat = FALSE;
				context->pfxFormat = FALSE;
			}
			else if (strcmp(arg->Value, "pem") == 0)
			{
				context->crtFormat = FALSE;
				context->pemFormat = TRUE;
				context->pfxFormat = FALSE;
			}
			else if (strcmp(arg->Value, "pfx") == 0)
			{
				context->crtFormat = FALSE;
				context->pemFormat = FALSE;
				context->pfxFormat = TRUE;
			}
			else
				return -1;
		}
		CommandLineSwitchCase(arg, "path")
		{
			if (!(arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
				continue;

			context->output_path = _strdup(arg->Value);

			if (!context->output_path)
				return -1;
		}
		CommandLineSwitchCase(arg, "p")
		{
			if (!(arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
				continue;

			context->password = _strdup(arg->Value);

			if (!context->password)
				return -1;
		}
		CommandLineSwitchCase(arg, "n")
		{
			if (!(arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
				continue;

			context->common_name = _strdup(arg->Value);

			if (!context->common_name)
				return -1;
		}
		CommandLineSwitchCase(arg, "y")
		{
			long val;

			if (!(arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
				continue;

			val = strtol(arg->Value, NULL, 0);

			if ((errno != 0) || (val < 0) || (val > INT32_MAX))
				return -1;

			context->duration_years = (int)val;
		}
		CommandLineSwitchCase(arg, "m")
		{
			long val;

			if (!(arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
				continue;

			val = strtol(arg->Value, NULL, 0);

			if ((errno != 0) || (val < 1) || (val > 12))
				return -1;

			context->duration_months = (int)val;
		}
		CommandLineSwitchDefault(arg)
		{
		}
		CommandLineSwitchEnd(arg)
	} while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return 1;
}

int makecert_context_set_output_file_name(MAKECERT_CONTEXT* context, char* name)
{
	if (!context)
		return -1;

	free(context->output_file);
	context->output_file = NULL;

	if (name)
		context->output_file = _strdup(name);

	if (!context->output_file)
		return -1;

	return 1;
}

int makecert_context_output_certificate_file(MAKECERT_CONTEXT* context, char* path)
{
#ifdef WITH_OPENSSL
	FILE* fp = NULL;
	int status;
	size_t length;
	size_t offset;
	char* filename = NULL;
	char* fullpath = NULL;
	char* ext;
	int ret = -1;
	BIO* bio = NULL;
	char* x509_str = NULL;

	if (!context || !path)
		return -1;

	if (!context->output_file)
	{
		context->output_file = _strdup(context->default_name);

		if (!context->output_file)
			return -1;
	}

	/*
	 * Output Certificate File
	 */
	length = strlen(context->output_file);
	filename = malloc(length + 8);

	if (!filename)
		return -1;

	if (context->crtFormat)
		ext = "crt";
	else if (context->pemFormat)
		ext = "pem";
	else if (context->pfxFormat)
		ext = "pfx";
	else
		goto out_fail;

	sprintf_s(filename, length + 8, "%s.%s", context->output_file, ext);

	if (path)
		fullpath = GetCombinedPath(path, filename);
	else
		fullpath = _strdup(filename);

	if (!fullpath)
		goto out_fail;

	fp = winpr_fopen(fullpath, "w+");

	if (fp)
	{
		if (context->pfxFormat)
		{
			if (!context->password)
			{
				context->password = _strdup("password");

				if (!context->password)
					goto out_fail;

				printf("Using default export password \"password\"\n");
			}

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
			OpenSSL_add_all_algorithms();
			OpenSSL_add_all_ciphers();
			OpenSSL_add_all_digests();
#else
			OPENSSL_init_crypto(OPENSSL_INIT_ADD_ALL_CIPHERS | OPENSSL_INIT_ADD_ALL_DIGESTS |
			                        OPENSSL_INIT_LOAD_CONFIG,
			                    NULL);
#endif
			context->pkcs12 = PKCS12_create(context->password, context->default_name, context->pkey,
			                                context->x509, NULL, 0, 0, 0, 0, 0);

			if (!context->pkcs12)
				goto out_fail;

			bio = BIO_new(BIO_s_mem());

			if (!bio)
				goto out_fail;

			status = i2d_PKCS12_bio(bio, context->pkcs12);

			if (status != 1)
				goto out_fail;

			x509_str = makecert_read_str(bio, &offset);

			if (!x509_str)
				goto out_fail;

			length = offset;

			if (fwrite((void*)x509_str, length, 1, fp) != 1)
				goto out_fail;
		}
		else
		{
			bio = BIO_new(BIO_s_mem());

			if (!bio)
				goto out_fail;

			if (!PEM_write_bio_X509(bio, context->x509))
				goto out_fail;

			x509_str = makecert_read_str(bio, &offset);

			if (!x509_str)
				goto out_fail;

			length = offset;

			if (fwrite(x509_str, length, 1, fp) != 1)
				goto out_fail;

			free(x509_str);
			x509_str = NULL;
			BIO_free_all(bio);
			bio = NULL;

			if (context->pemFormat)
			{
				bio = BIO_new(BIO_s_mem());

				if (!bio)
					goto out_fail;

				status = PEM_write_bio_PrivateKey(bio, context->pkey, NULL, NULL, 0, NULL, NULL);

				if (status < 0)
					goto out_fail;

				x509_str = makecert_read_str(bio, &offset);
				if (!x509_str)
					goto out_fail;

				length = offset;

				if (fwrite(x509_str, length, 1, fp) != 1)
					goto out_fail;
			}
		}
	}

	ret = 1;
out_fail:
	BIO_free_all(bio);

	if (fp)
		fclose(fp);

	free(x509_str);
	free(filename);
	free(fullpath);
	return ret;
#else
	return 1;
#endif
}

int makecert_context_output_private_key_file(MAKECERT_CONTEXT* context, char* path)
{
#ifdef WITH_OPENSSL
	FILE* fp = NULL;
	size_t length;
	size_t offset;
	char* filename = NULL;
	char* fullpath = NULL;
	int ret = -1;
	BIO* bio = NULL;
	char* x509_str = NULL;

	if (!context->crtFormat)
		return 1;

	if (!context->output_file)
	{
		context->output_file = _strdup(context->default_name);

		if (!context->output_file)
			return -1;
	}

	/**
	 * Output Private Key File
	 */
	length = strlen(context->output_file);
	filename = malloc(length + 8);

	if (!filename)
		return -1;

	sprintf_s(filename, length + 8, "%s.key", context->output_file);

	if (path)
		fullpath = GetCombinedPath(path, filename);
	else
		fullpath = _strdup(filename);

	if (!fullpath)
		goto out_fail;

	fp = winpr_fopen(fullpath, "w+");

	if (!fp)
		goto out_fail;

	bio = BIO_new(BIO_s_mem());

	if (!bio)
		goto out_fail;

	if (!PEM_write_bio_PrivateKey(bio, context->pkey, NULL, NULL, 0, NULL, NULL))
		goto out_fail;

	x509_str = makecert_read_str(bio, &offset);

	if (!x509_str)
		goto out_fail;

	length = offset;

	if (fwrite((void*)x509_str, length, 1, fp) != 1)
		goto out_fail;

	ret = 1;
out_fail:

	if (fp)
		fclose(fp);

	BIO_free_all(bio);
	free(x509_str);
	free(filename);
	free(fullpath);
	return ret;
#else
	return 1;
#endif
}

int makecert_context_process(MAKECERT_CONTEXT* context, int argc, char** argv)
{
	COMMAND_LINE_ARGUMENT_A args[] = {
		/* Custom Options */

		{ "rdp", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL,
		  "Unsupported - Generate certificate with required options for RDP usage." },
		{ "silent", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL,
		  "Silently generate certificate without verbose output." },
		{ "live", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL,
		  "Generate certificate live in memory when used as a library." },
		{ "format", COMMAND_LINE_VALUE_REQUIRED, "<crt|pem|pfx>", NULL, NULL, -1, NULL,
		  "Specify certificate file format" },
		{ "path", COMMAND_LINE_VALUE_REQUIRED, "<path>", NULL, NULL, -1, NULL,
		  "Specify certificate file output path" },
		{ "p", COMMAND_LINE_VALUE_REQUIRED, "<password>", NULL, NULL, -1, NULL,
		  "Specify certificate export password" },

		/* Basic Options */

		{ "n", COMMAND_LINE_VALUE_REQUIRED, "<name>", NULL, NULL, -1, NULL,
		  "Specifies the subject's certificate name. This name must conform to the X.500 standard. "
		  "The simplest method is to specify the name in double quotes, preceded by CN=; for "
		  "example, "
		  "-n \"CN=myName\"." },
		{ "pe", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL,
		  "Unsupported - Marks the generated private key as exportable. This allows the private "
		  "key to "
		  "be included in the certificate." },
		{ "sk", COMMAND_LINE_VALUE_REQUIRED, "<keyname>", NULL, NULL, -1, NULL,
		  "Unsupported - Specifies the subject's key container location, which contains the "
		  "private "
		  "key. "
		  "If a key container does not exist, it will be created." },
		{ "sr", COMMAND_LINE_VALUE_REQUIRED, "<location>", NULL, NULL, -1, NULL,
		  "Unsupported - Specifies the subject's certificate store location. location can be "
		  "either "
		  "currentuser (the default) or localmachine." },
		{ "ss", COMMAND_LINE_VALUE_REQUIRED, "<store>", NULL, NULL, -1, NULL,
		  "Unsupported - Specifies the subject's certificate store name that stores the output "
		  "certificate." },
		{ "#", COMMAND_LINE_VALUE_REQUIRED, "<number>", NULL, NULL, -1, NULL,
		  "Specifies a serial number from 1 to 2,147,483,647. The default is a unique value "
		  "generated "
		  "by Makecert.exe." },
		{ "$", COMMAND_LINE_VALUE_REQUIRED, "<authority>", NULL, NULL, -1, NULL,
		  "Unsupported - Specifies the signing authority of the certificate, which must be set to "
		  "either commercial "
		  "(for certificates used by commercial software publishers) or individual (for "
		  "certificates "
		  "used by individual software publishers)." },

		/* Extended Options */

		{ "a", COMMAND_LINE_VALUE_REQUIRED, "<algorithm>", NULL, NULL, -1, NULL,
		  "Specifies the signature algorithm. algorithm must be md5, sha1, sha256 (the default), "
		  "sha384, or sha512." },
		{ "b", COMMAND_LINE_VALUE_REQUIRED, "<mm/dd/yyyy>", NULL, NULL, -1, NULL,
		  "Unsupported - Specifies the start of the validity period. Defaults to the current "
		  "date." },
		{ "crl", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL,
		  "Unsupported - Generates a certificate relocation list (CRL) instead of a certificate." },
		{ "cy", COMMAND_LINE_VALUE_REQUIRED, "<certType>", NULL, NULL, -1, NULL,
		  "Unsupported - Specifies the certificate type. Valid values are end for end-entity and "
		  "authority for certification authority." },
		{ "e", COMMAND_LINE_VALUE_REQUIRED, "<mm/dd/yyyy>", NULL, NULL, -1, NULL,
		  "Unsupported - Specifies the end of the validity period. Defaults to 12/31/2039 11:59:59 "
		  "GMT." },
		{ "eku", COMMAND_LINE_VALUE_REQUIRED, "<oid[,oid…]>", NULL, NULL, -1, NULL,
		  "Unsupported - Inserts a list of comma-separated, enhanced key usage object identifiers "
		  "(OIDs) into the certificate." },
		{ "h", COMMAND_LINE_VALUE_REQUIRED, "<number>", NULL, NULL, -1, NULL,
		  "Unsupported - Specifies the maximum height of the tree below this certificate." },
		{ "ic", COMMAND_LINE_VALUE_REQUIRED, "<file>", NULL, NULL, -1, NULL,
		  "Unsupported - Specifies the issuer's certificate file." },
		{ "ik", COMMAND_LINE_VALUE_REQUIRED, "<keyName>", NULL, NULL, -1, NULL,
		  "Unsupported - Specifies the issuer's key container name." },
		{ "iky", COMMAND_LINE_VALUE_REQUIRED, "<keyType>", NULL, NULL, -1, NULL,
		  "Unsupported - Specifies the issuer's key type, which must be one of the following: "
		  "signature (which indicates that the key is used for a digital signature), "
		  "exchange (which indicates that the key is used for key encryption and key exchange), "
		  "or an integer that represents a provider type. "
		  "By default, you can pass 1 for an exchange key or 2 for a signature key." },
		{ "in", COMMAND_LINE_VALUE_REQUIRED, "<name>", NULL, NULL, -1, NULL,
		  "Unsupported - Specifies the issuer's certificate common name." },
		{ "ip", COMMAND_LINE_VALUE_REQUIRED, "<provider>", NULL, NULL, -1, NULL,
		  "Unsupported - Specifies the issuer's CryptoAPI provider name. For information about the "
		  "CryptoAPI provider name, see the –sp option." },
		{ "ir", COMMAND_LINE_VALUE_REQUIRED, "<location>", NULL, NULL, -1, NULL,
		  "Unsupported - Specifies the location of the issuer's certificate store. location can be "
		  "either currentuser (the default) or localmachine." },
		{ "is", COMMAND_LINE_VALUE_REQUIRED, "<store>", NULL, NULL, -1, NULL,
		  "Unsupported - Specifies the issuer's certificate store name." },
		{ "iv", COMMAND_LINE_VALUE_REQUIRED, "<pvkFile>", NULL, NULL, -1, NULL,
		  "Unsupported - Specifies the issuer's .pvk private key file." },
		{ "iy", COMMAND_LINE_VALUE_REQUIRED, "<type>", NULL, NULL, -1, NULL,
		  "Unsupported - Specifies the issuer's CryptoAPI provider type. For information about the "
		  "CryptoAPI provider type, see the –sy option." },
		{ "l", COMMAND_LINE_VALUE_REQUIRED, "<link>", NULL, NULL, -1, NULL,
		  "Unsupported - Links to policy information (for example, to a URL)." },
		{ "len", COMMAND_LINE_VALUE_REQUIRED, "<number>", NULL, NULL, -1, NULL,
		  "Specifies the generated key length, in bits." },
		{ "m", COMMAND_LINE_VALUE_REQUIRED, "<number>", NULL, NULL, -1, NULL,
		  "Specifies the duration, in months, of the certificate validity period." },
		{ "y", COMMAND_LINE_VALUE_REQUIRED, "<number>", NULL, NULL, -1, NULL,
		  "Specifies the duration, in years, of the certificate validity period." },
		{ "nscp", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL,
		  "Unsupported - Includes the Netscape client-authorization extension." },
		{ "r", COMMAND_LINE_VALUE_FLAG, NULL, NULL, NULL, -1, NULL,
		  "Unsupported - Creates a self-signed certificate." },
		{ "sc", COMMAND_LINE_VALUE_REQUIRED, "<file>", NULL, NULL, -1, NULL,
		  "Unsupported - Specifies the subject's certificate file." },
		{ "sky", COMMAND_LINE_VALUE_REQUIRED, "<keyType>", NULL, NULL, -1, NULL,
		  "Unsupported - Specifies the subject's key type, which must be one of the following: "
		  "signature (which indicates that the key is used for a digital signature), "
		  "exchange (which indicates that the key is used for key encryption and key exchange), "
		  "or an integer that represents a provider type. "
		  "By default, you can pass 1 for an exchange key or 2 for a signature key." },
		{ "sp", COMMAND_LINE_VALUE_REQUIRED, "<provider>", NULL, NULL, -1, NULL,
		  "Unsupported - Specifies the subject's CryptoAPI provider name, which must be defined in "
		  "the "
		  "registry subkeys of "
		  "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography\\Defaults\\Provider. If both –sp "
		  "and "
		  "–sy are present, "
		  "the type of the CryptoAPI provider must correspond to the Type value of the provider's "
		  "subkey." },
		{ "sv", COMMAND_LINE_VALUE_REQUIRED, "<pvkFile>", NULL, NULL, -1, NULL,
		  "Unsupported - Specifies the subject's .pvk private key file. The file is created if "
		  "none "
		  "exists." },
		{ "sy", COMMAND_LINE_VALUE_REQUIRED, "<type>", NULL, NULL, -1, NULL,
		  "Unsupported - Specifies the subject's CryptoAPI provider type, which must be defined in "
		  "the "
		  "registry subkeys of "
		  "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography\\Defaults\\Provider Types. If "
		  "both "
		  "–sy and –sp are present, "
		  "the name of the CryptoAPI provider must correspond to the Name value of the provider "
		  "type "
		  "subkey." },
		{ "tbs", COMMAND_LINE_VALUE_REQUIRED, "<file>", NULL, NULL, -1, NULL,
		  "Unsupported - Specifies the certificate or CRL file to be signed." },

		/* Help */

		{ "?", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_HELP, NULL, NULL, NULL, -1, "help",
		  "print help" },
		{ "!", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_HELP, NULL, NULL, NULL, -1, "help-ext",
		  "print extended help" },
		{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
	};
#ifdef WITH_OPENSSL
	size_t length;
	char* entry;
	int key_length;
	long serial = 0;
	X509_NAME* name = NULL;
	const EVP_MD* md = NULL;
	const COMMAND_LINE_ARGUMENT_A* arg;
	int ret;
	ret = makecert_context_parse_arguments(context, args, argc, argv);

	if (ret < 1)
	{
		return ret;
	}

	if (!context->default_name && !context->common_name)
	{
		context->default_name = x509_get_default_name();

		if (!context->default_name)
			return -1;
	}
	else
	{
		context->default_name = _strdup(context->common_name);

		if (!context->default_name)
			return -1;
	}

	if (!context->common_name)
	{
		context->common_name = _strdup(context->default_name);

		if (!context->common_name)
			return -1;
	}

	if (!context->pkey)
		context->pkey = EVP_PKEY_new();

	if (!context->pkey)
		return -1;

	if (!context->x509)
		context->x509 = X509_new();

	if (!context->x509)
		return -1;

	key_length = 2048;
	arg = CommandLineFindArgumentA(args, "len");

	if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
	{
		unsigned long val = strtoul(arg->Value, NULL, 0);

		if ((errno != 0) || (val > INT_MAX))
			return -1;
		key_length = (int)val;
	}

#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
	context->rsa = RSA_generate_key(key_length, RSA_F4, NULL, NULL);
#else
	{
		BIGNUM* rsa = BN_secure_new();
		int rc;

		if (!rsa)
			return -1;

		context->rsa = RSA_new();

		if (!context->rsa)
		{
			BN_clear_free(rsa);
			return -1;
		}

		BN_set_word(rsa, RSA_F4);
		rc = RSA_generate_key_ex(context->rsa, key_length, rsa, NULL);
		BN_clear_free(rsa);

		if (rc != 1)
			return -1;
	}
#endif

	if (!EVP_PKEY_assign_RSA(context->pkey, context->rsa))
		return -1;

	context->rsa = NULL;
	X509_set_version(context->x509, 2);
	arg = CommandLineFindArgumentA(args, "#");

	if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
	{
		serial = strtol(arg->Value, NULL, 0);

		if (errno != 0)
			return -1;
	}
	else
		serial = (long)GetTickCount64();

	ASN1_INTEGER_set(X509_get_serialNumber(context->x509), serial);
	{
		ASN1_TIME* before;
		ASN1_TIME* after;
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
		before = X509_get_notBefore(context->x509);
		after = X509_get_notAfter(context->x509);
#else
		before = X509_getm_notBefore(context->x509);
		after = X509_getm_notAfter(context->x509);
#endif
		X509_gmtime_adj(before, 0);

		if (context->duration_months)
			X509_gmtime_adj(after, (long)(60 * 60 * 24 * 31 * context->duration_months));
		else if (context->duration_years)
			X509_gmtime_adj(after, (long)(60 * 60 * 24 * 365 * context->duration_years));
	}
	X509_set_pubkey(context->x509, context->pkey);
	name = X509_get_subject_name(context->x509);
	arg = CommandLineFindArgumentA(args, "n");

	if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
	{
		entry = x509_name_parse(arg->Value, "C", &length);

		if (entry)
			X509_NAME_add_entry_by_txt(name, "C", MBSTRING_UTF8, (const unsigned char*)entry,
			                           (int)length, -1, 0);

		entry = x509_name_parse(arg->Value, "ST", &length);

		if (entry)
			X509_NAME_add_entry_by_txt(name, "ST", MBSTRING_UTF8, (const unsigned char*)entry,
			                           (int)length, -1, 0);

		entry = x509_name_parse(arg->Value, "L", &length);

		if (entry)
			X509_NAME_add_entry_by_txt(name, "L", MBSTRING_UTF8, (const unsigned char*)entry,
			                           (int)length, -1, 0);

		entry = x509_name_parse(arg->Value, "O", &length);

		if (entry)
			X509_NAME_add_entry_by_txt(name, "O", MBSTRING_UTF8, (const unsigned char*)entry,
			                           (int)length, -1, 0);

		entry = x509_name_parse(arg->Value, "OU", &length);

		if (entry)
			X509_NAME_add_entry_by_txt(name, "OU", MBSTRING_UTF8, (const unsigned char*)entry,
			                           (int)length, -1, 0);

		entry = context->common_name;
		length = strlen(entry);
		X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8, (const unsigned char*)entry,
		                           (int)length, -1, 0);
	}
	else
	{
		entry = context->common_name;
		length = strlen(entry);
		X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8, (const unsigned char*)entry,
		                           (int)length, -1, 0);
	}

	X509_set_issuer_name(context->x509, name);
	x509_add_ext(context->x509, NID_ext_key_usage, "serverAuth");
	arg = CommandLineFindArgumentA(args, "a");
	md = EVP_sha256();

	if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
	{
		md = EVP_get_digestbyname(arg->Value);
		if (!md)
			return -1;
	}

	if (!X509_sign(context->x509, context->pkey, md))
		return -1;

	/**
	 * Print certificate
	 */

	if (!context->silent)
	{
		BIO* bio;
		int status;
		char* x509_str;
		bio = BIO_new(BIO_s_mem());

		if (!bio)
			return -1;

		status = X509_print(bio, context->x509);

		if (status < 0)
		{
			BIO_free_all(bio);
			return -1;
		}

		x509_str = makecert_read_str(bio, NULL);
		if (!x509_str)
		{
			BIO_free_all(bio);
			return -1;
		}

		printf("%s", x509_str);
		free(x509_str);
		BIO_free_all(bio);
	}

	/**
	 * Output certificate and private key to files
	 */

	if (!context->live)
	{
		if (!winpr_PathFileExists(context->output_path))
		{
			if (!CreateDirectoryA(context->output_path, NULL))
				return -1;
		}

		if (makecert_context_output_certificate_file(context, context->output_path) != 1)
			return -1;

		if (context->crtFormat)
		{
			if (makecert_context_output_private_key_file(context, context->output_path) < 0)
				return -1;
		}
	}

#endif
	return 0;
}

MAKECERT_CONTEXT* makecert_context_new(void)
{
	MAKECERT_CONTEXT* context = (MAKECERT_CONTEXT*)calloc(1, sizeof(MAKECERT_CONTEXT));

	if (context)
	{
		context->crtFormat = TRUE;
		context->duration_years = 1;
	}

	return context;
}

void makecert_context_free(MAKECERT_CONTEXT* context)
{
	if (context)
	{
		free(context->password);
		free(context->default_name);
		free(context->common_name);
		free(context->output_file);
		free(context->output_path);
#ifdef WITH_OPENSSL
		X509_free(context->x509);
		EVP_PKEY_free(context->pkey);
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
		CRYPTO_cleanup_all_ex_data();
#endif
#endif
		free(context);
	}
}

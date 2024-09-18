#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "../cmdline.h"

static char* resize(char** buffer, size_t* size, size_t increment)
{
	const size_t nsize = *size + increment;
	char* tmp = realloc(*buffer, nsize);
	if (!tmp)
	{
		(void)fprintf(stderr,
		              "Could not reallocate string buffer from %" PRIuz " to %" PRIuz " bytes.\n",
		              *size, nsize);
		free(*buffer);
		return NULL;
	}
	memset(&tmp[*size], '\0', increment);
	*size = nsize;
	*buffer = tmp;
	return tmp;
}

static char* append(char** buffer, size_t* size, const char* str)
{
	const size_t len = strnlen(*buffer, *size);
	const size_t add = strlen(str);
	const size_t required = len + add + 1;

	if (required > *size)
	{
		if (!resize(buffer, size, required - *size))
			return NULL;
	}
	strncpy(&(*buffer)[len], str, add);
	return *buffer;
}

static LPSTR tr_esc_str(LPCSTR arg, bool format)
{
	const char* str = NULL;
	LPSTR tmp = NULL;
	size_t ds = 0;

	if (NULL == arg)
		return NULL;

	const size_t s = strlen(arg) + 1;
	if (!resize(&tmp, &ds, s))
		exit(-2);

	for (size_t x = 0; x < s; x++)
	{
		char data[2] = { 0 };
		switch (arg[x])
		{
			case '-':
				str = "\\-";
				if (!append(&tmp, &ds, str))
					exit(-3);
				break;

			case '<':
				if (format)
					str = "\\fI";
				else
					str = "<";

				if (!append(&tmp, &ds, str))
					exit(-3);
				break;

			case '>':
				if (format)
					str = "\\fR";
				else
					str = ">";

				if (!append(&tmp, &ds, str))
					exit(-4);
				break;

			case '\'':
				str = "\\*(Aq";
				if (!append(&tmp, &ds, str))
					exit(-4);
				break;

			case '.':
				if (!append(&tmp, &ds, "\\&."))
					exit(-6);
				break;

			case '\r':
			case '\n':
				if (!append(&tmp, &ds, "\n.br\n"))
					exit(-7);
				break;

			default:
				data[0] = arg[x];
				if (!append(&tmp, &ds, data))
					exit(-8);
				break;
		}
	}

	return tmp;
}

int main(int argc, char* argv[])
{
	size_t elements = sizeof(global_cmd_args) / sizeof(global_cmd_args[0]);

	if (argc != 2)
	{
		(void)fprintf(stderr, "Usage: %s <output file name>\n", argv[0]);
		return -1;
	}

	const char* fname = argv[1];

	(void)fprintf(stdout, "Generating manpage file '%s'\n", fname);
	FILE* fp = fopen(fname, "w");
	if (NULL == fp)
	{
		(void)fprintf(stderr, "Could not open '%s' for writing.\n", fname);
		return -1;
	}

	/* The tag used as header in the manpage */
	(void)fprintf(fp, ".SH \"OPTIONS\"\n");

	if (elements < 2)
	{
		(void)fprintf(stderr, "The argument array 'args' is empty, writing an empty file.\n");
		elements = 1;
	}

	for (size_t x = 0; x < elements - 1; x++)
	{
		const COMMAND_LINE_ARGUMENT_A* arg = &global_cmd_args[x];
		char* name = tr_esc_str(arg->Name, FALSE);
		char* alias = tr_esc_str(arg->Alias, FALSE);
		char* format = tr_esc_str(arg->Format, TRUE);
		char* text = tr_esc_str(arg->Text, FALSE);

		(void)fprintf(fp, ".PP\n");
		bool first = true;
		do
		{
			(void)fprintf(fp, "%s\\fB", first ? "" : ", ");
			first = false;
			if (arg->Flags == COMMAND_LINE_VALUE_BOOL)
				(void)fprintf(fp, "%s", arg->Default ? "\\-" : "+");
			else
				(void)fprintf(fp, "/");

			(void)fprintf(fp, "%s\\fR", name);

			if (format)
			{
				if (arg->Flags == COMMAND_LINE_VALUE_OPTIONAL)
					(void)fprintf(fp, "[");

				(void)fprintf(fp, ":%s", format);

				if (arg->Flags == COMMAND_LINE_VALUE_OPTIONAL)
					(void)fprintf(fp, "]");
			}

			if (alias == name)
				break;

			free(name);
			name = alias;
		} while (alias);
		(void)fprintf(fp, "\n");

		if (text)
		{
			(void)fprintf(fp, ".RS 4\n");
			const int hasText = text && (strnlen(text, 2) > 0);
			if (hasText)
				(void)fprintf(fp, "%s", text);

			if (arg->Flags & COMMAND_LINE_VALUE_BOOL &&
			    (!arg->Default || arg->Default == BoolValueTrue))
				(void)fprintf(fp, " (default:%s)\n", arg->Default ? "on" : "off");
			else if (arg->Default)
			{
				char* value = tr_esc_str(arg->Default, FALSE);
				(void)fprintf(fp, " (default:%s)\n", value);
				free(value);
			}
			else if (hasText)
				(void)fprintf(fp, "\n");
		}

		(void)fprintf(fp, ".RE\n");

		free(name);
		free(format);
		free(text);
	}

	(void)fclose(fp);

	(void)fprintf(stdout, "successfully generated '%s'\n", fname);
	return 0;
}

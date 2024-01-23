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
		fprintf(stderr, "Could not reallocate string buffer from %" PRIuz " to %" PRIuz " bytes.\n",
		        *size, nsize);
		free(*buffer);
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
	strcat(*buffer, str);
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
			case '<':
				if (format)
					str = "<replaceable>";
				else
					str = "&lt;";

				if (!append(&tmp, &ds, str))
					exit(-3);
				break;

			case '>':
				if (format)
					str = "</replaceable>";
				else
					str = "&gt;";

				if (!append(&tmp, &ds, str))
					exit(-4);
				break;

			case '\'':
				if (!append(&tmp, &ds, "&apos;"))
					exit(-5);
				break;

			case '"':
				if (!append(&tmp, &ds, "&quot;"))
					exit(-6);
				break;

			case '&':
				if (!append(&tmp, &ds, "&amp;"))
					exit(-6);
				break;

			case '\r':
			case '\n':
				if (!append(&tmp, &ds, "<sbr/>"))
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
	const char* fname = "freerdp-argument.1.xml";

	fprintf(stdout, "Generating docbook file '%s'\n", fname);
	FILE* fp = fopen(fname, "w");
	if (NULL == fp)
	{
		fprintf(stderr, "Could not open '%s' for writing.\n", fname);
		return -1;
	}

	/* The tag used as header in the manpage */
	fprintf(fp, "<refsect1>\n");
	fprintf(fp, "\t<title>Options</title>\n");
	fprintf(fp, "\t\t<variablelist>\n");

	/* Iterate over argument struct and write data to docbook 4.5
	 * compatible XML */
	if (elements < 2)
	{
		fprintf(stderr, "The argument array 'args' is empty, writing an empty file.\n");
		elements = 1;
	}

	for (size_t x = 0; x < elements - 1; x++)
	{
		const COMMAND_LINE_ARGUMENT_A* arg = &global_cmd_args[x];
		char* name = tr_esc_str(arg->Name, FALSE);
		char* alias = tr_esc_str(arg->Alias, FALSE);
		char* format = tr_esc_str(arg->Format, TRUE);
		char* text = tr_esc_str(arg->Text, FALSE);
		fprintf(fp, "\t\t\t<varlistentry>\n");

		do
		{
			fprintf(fp, "\t\t\t\t<term><option>");

			if (arg->Flags == COMMAND_LINE_VALUE_BOOL)
				fprintf(fp, "%s", arg->Default ? "-" : "+");
			else
				fprintf(fp, "/");

			fprintf(fp, "%s</option>", name);

			if (format)
			{
				if (arg->Flags == COMMAND_LINE_VALUE_OPTIONAL)
					fprintf(fp, "[");

				fprintf(fp, ":%s", format);

				if (arg->Flags == COMMAND_LINE_VALUE_OPTIONAL)
					fprintf(fp, "]");
			}

			fprintf(fp, "</term>\n");

			if (alias == name)
				break;

			free(name);
			name = alias;
		} while (alias);

		if (text)
		{
			fprintf(fp, "\t\t\t\t<listitem>\n");
			fprintf(fp, "\t\t\t\t\t<para>");

			if (text)
				fprintf(fp, "%s", text);

			if (arg->Flags & COMMAND_LINE_VALUE_BOOL &&
			    (!arg->Default || arg->Default == BoolValueTrue))
				fprintf(fp, " (default:%s)", arg->Default ? "on" : "off");
			else if (arg->Default)
			{
				char* value = tr_esc_str(arg->Default, FALSE);
				fprintf(fp, " (default:%s)", value);
				free(value);
			}

			fprintf(fp, "</para>\n");
			fprintf(fp, "\t\t\t\t</listitem>\n");
		}

		fprintf(fp, "\t\t\t</varlistentry>\n");
		free(name);
		free(format);
		free(text);
	}

	fprintf(fp, "\t\t</variablelist>\n");
	fprintf(fp, "\t</refsect1>\n");
	fclose(fp);

	fprintf(stdout, "successfully generated '%s'\n", fname);
	return 0;
}

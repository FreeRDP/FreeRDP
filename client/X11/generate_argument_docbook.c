#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "../common/cmdline.h"

#define TAG FREERDP_TAG("generate_argument_docbook")
LPSTR tr_esc_str(LPCSTR arg, bool format)
{
	LPSTR tmp = NULL;
	size_t cs = 0, x, ds, len;
	size_t s;
	if(NULL == arg)
		return NULL;
	s = strlen(arg);
	/* Find trailing whitespaces */
	while((s > 0) && isspace(arg[s-1]))
		s--;
	/* Prepare a initial buffer with the size of the result string. */
	ds = s + 1;
	if(s)
		tmp = (LPSTR)realloc(tmp, ds * sizeof(CHAR));
	if(NULL == tmp)
	{
		fprintf(stderr,  "Could not allocate string buffer.\n");
		exit(-2);
	}
	/* Copy character for character and check, if it is necessary to escape. */
	memset(tmp, 0, ds * sizeof(CHAR));
	for(x=0; x<s; x++)
	{
		switch(arg[x])
		{
			case '<':
				len = format ? 13 : 4;
				ds += len - 1;
				tmp = (LPSTR)realloc(tmp, ds * sizeof(CHAR));
				if(NULL == tmp)
				{
					fprintf(stderr,  "Could not reallocate string buffer.\n");
					exit(-3);
				}
				if (format)
					strncpy (&tmp[cs], "<replaceable>", len);
				else				
					strncpy (&tmp[cs], "&lt;", len);
				cs += len;
				break;
			case '>':
				len = format ? 14 : 4;
				ds += len - 1;
				tmp = (LPSTR)realloc(tmp, ds * sizeof(CHAR));
				if(NULL == tmp)
				{
					fprintf(stderr,  "Could not reallocate string buffer.\n");
					exit(-4);
				}
				if (format)
					strncpy (&tmp[cs], "</replaceable>", len);
				else				
					strncpy (&tmp[cs], "&lt;", len);
				cs += len;
				break;
			case '\'':
				ds += 5;
				tmp = (LPSTR)realloc(tmp, ds * sizeof(CHAR));
				if(NULL == tmp)
				{
					fprintf(stderr,  "Could not reallocate string buffer.\n");
					exit(-5);
				}
				tmp[cs++] = '&';
				tmp[cs++] = 'a';
				tmp[cs++] = 'p';
				tmp[cs++] = 'o';
				tmp[cs++] = 's';
				tmp[cs++] = ';';
				break;
			case '"':
				ds += 5;
				tmp = (LPSTR)realloc(tmp, ds * sizeof(CHAR));
				if(NULL == tmp)
				{
					fprintf(stderr,  "Could not reallocate string buffer.\n");
					exit(-6);
				}
				tmp[cs++] = '&';
				tmp[cs++] = 'q';
				tmp[cs++] = 'u';
				tmp[cs++] = 'o';
				tmp[cs++] = 't';
				tmp[cs++] = ';';
				break;
			case '&':
				ds += 4;
				tmp = (LPSTR)realloc(tmp, ds * sizeof(CHAR));
				if(NULL == tmp)
				{
					fprintf(stderr,  "Could not reallocate string buffer.\n");
					exit(-7);
				}
				tmp[cs++] = '&';
				tmp[cs++] = 'a';
				tmp[cs++] = 'm';
				tmp[cs++] = 'p';
				tmp[cs++] = ';';
				break;
			default:
				tmp[cs++] = arg[x];
				break;
		}
		/* Assure, the string is '\0' terminated. */
		tmp[ds-1] = '\0';
	}
	return tmp;
}

int main(int argc, char *argv[])
{
	size_t elements = sizeof(args)/sizeof(args[0]);
	size_t x;
	const char *fname = "xfreerdp-argument.1.xml";
	FILE *fp = NULL;
	/* Open output file for writing, truncate if existing. */
	fp = fopen(fname, "w");
	if(NULL == fp)
	{
		fprintf(stderr,  "Could not open '%s' for writing.\n", fname);
		return -1;
	}
	/* The tag used as header in the manpage */
	fprintf(fp, "<refsect1>\n");
	fprintf(fp, "\t<title>Options</title>\n");
	fprintf(fp, "\t\t<variablelist>\n");
	/* Iterate over argument struct and write data to docbook 4.5
	 * compatible XML */
	if(elements < 2)
	{
		fprintf(stderr,  "The argument array 'args' is empty, writing an empty file.\n");
		elements = 1;
	}
	for(x=0; x<elements - 1; x++)
	{
		const COMMAND_LINE_ARGUMENT_A *arg = &args[x];
		char *name = tr_esc_str((LPSTR) arg->Name, FALSE);
		char *alias = tr_esc_str((LPSTR) arg->Alias, FALSE);
		char *format = tr_esc_str(arg->Format, TRUE);
		char *text = tr_esc_str((LPSTR) arg->Text, FALSE);

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

			free (name);
			name = alias;
		}
		while (alias);

		if (text)
		{
			fprintf(fp, "\t\t\t\t<listitem>\n");
			fprintf(fp, "\t\t\t\t\t<para>");

			if (text)
				fprintf(fp, "%s", text);

			if (arg->Flags == COMMAND_LINE_VALUE_BOOL)
				fprintf(fp, " (default:%s)", arg->Default ? "on" : "off");
			else if (arg->Default)
			{
				char *value = tr_esc_str((LPSTR) arg->Default, FALSE);
				fprintf(fp, " (default:%s)", value);
				free (value);
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
	return 0;
}


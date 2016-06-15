
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include <winpr/cmdline.h>

/* We need to include the command line c file to get access to
 * the argument struct. */
#include "../common/cmdline.c"

LPSTR tr_esc_str(LPCSTR arg)
{
	LPSTR tmp = NULL;
	size_t cs = 0, x, ds;
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
		WLog_ERR(TAG,  "Could not allocate string buffer.");
		exit(-2);
	}
	/* Copy character for character and check, if it is necessary to escape. */
	memset(tmp, 0, ds * sizeof(CHAR));
	for(x=0; x<s; x++)
	{
		switch(arg[x])
		{
			case '<':
				ds += 3;
				tmp = (LPSTR)realloc(tmp, ds * sizeof(CHAR));
				if(NULL == tmp)
				{
					WLog_ERR(TAG,  "Could not reallocate string buffer.");
					exit(-3);
				}
				tmp[cs++] = '&';
				tmp[cs++] = 'l';
				tmp[cs++] = 't';
				tmp[cs++] = ';';
				break;
			case '>':
				ds += 3;
				tmp = (LPSTR)realloc(tmp, ds * sizeof(CHAR));
				if(NULL == tmp)
				{
					WLog_ERR(TAG,  "Could not reallocate string buffer.");
					exit(-4);
				}
				tmp[cs++] = '&';
				tmp[cs++] = 'g';
				tmp[cs++] = 't';
				tmp[cs++] = ';';
				break;
			case '\'':
				ds += 5;
				tmp = (LPSTR)realloc(tmp, ds * sizeof(CHAR));
				if(NULL == tmp)
				{
					WLog_ERR(TAG,  "Could not reallocate string buffer.");
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
					WLog_ERR(TAG,  "Could not reallocate string buffer.");
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
					WLog_ERR(TAG,  "Could not reallocate string buffer.");
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
		WLog_ERR(TAG,  "Could not open '%s' for writing.", fname);
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
		WLog_ERR(TAG,  "The argument array 'args' is empty, writing an empty file.");
		elements = 1;
	}
	for(x=0; x<elements - 1; x++)
	{
		const COMMAND_LINE_ARGUMENT_A *arg = &args[x];
		const char *name = tr_esc_str((LPSTR) arg->Name);
		const char *format = tr_esc_str(arg->Format);
		const char *text = tr_esc_str((LPSTR) arg->Text);
		fprintf(fp, "\t\t\t<varlistentry>\n");

		fprintf(fp, "\t\t\t\t<term><option>/%s</option>", name);
		if ((arg->Flags == COMMAND_LINE_VALUE_REQUIRED) && format)
			fprintf(fp, " <replaceable>%s</replaceable>\n", format);
		fprintf(fp, "</term>\n");

		if (format || text)
		{
			fprintf(fp, "\t\t\t\t<listitem>\n");
			fprintf(fp, "\t\t\t\t\t<para>%s\n", format ? format : "");
			if (text)
			{
				if (format)
					fprintf(fp, " - ");
				fprintf(fp, "%s", text);
			}
			fprintf(fp, "</para>\n");
			fprintf(fp, "\t\t\t\t</listitem>\n");
		}
		fprintf(fp, "\t\t\t</varlistentry>\n");
		free((void*) name);
		free((void*) format);
		free((void*) text);
	}
	fprintf(fp, "\t\t</variablelist>\n");
	fprintf(fp, "\t</refsect1>\n");
	fclose(fp);
	return 0;
}


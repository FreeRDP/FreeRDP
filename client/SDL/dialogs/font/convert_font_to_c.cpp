#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

#include "opensans_variable_font.hpp"

#define BLOCK_SIZE 8192
#define LINEWIDTH 80

static void usage(const char* prg)
{
	fprintf(stderr, "%s <font file> <buffer file>\n", prg);
}

static int write_header(FILE* out, const char* font, size_t size)
{
	fprintf(out, "/* AUTOGENERATED file, do not edit\n");
	fprintf(out, " *\n");
	fprintf(out, " * contains the converted font %s\n", font);
	fprintf(out, " */\n");
	fprintf(out, "\n#pragma once\n");
	fprintf(out, "#include <vector>\n");
	fprintf(out, "\n");
	fprintf(out, "const std::vector<unsigned char> font_buffer ={\n");

	return 0;
}

static int read(FILE* out, const char* font)
{
	FILE* fp = fopen(font, "rb");
	if (!fp)
	{
		fprintf(stderr, "Failed to open font file '%s'\n", font);
		return -11;
	}

	fseek(fp, 0, SEEK_END);
	off_t size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	const int rc = write_header(out, font, (size_t)size);
	if (rc != 0)
	{
		fclose(fp);
		return -12;
	}

	int first = 1;
	while (!feof(fp))
	{
		char buffer[BLOCK_SIZE] = {};
		const size_t read = fread(buffer, 1, sizeof(buffer), fp);
		for (size_t x = 0; x < read; x++)
		{
			if (!first)
				fprintf(out, ",");
			first = 0;
			fprintf(out, "0x%02" PRIx8, buffer[x] & 0xFF);
			if ((x % (LINEWIDTH / 5)) == 0)
				fprintf(out, "\n");
		}
	}

	fclose(fp);
	return rc;
}

static int write_trailer(FILE* out)
{
	fprintf(out, "};\n");
	return 0;
}

int main(int argc, char* argv[])
{
	int rc = -3;
	if (argc != 3)
	{
		usage(argv[0]);
		return -1;
	}

	const char* font = argv[1];
	const char* header = argv[2];

	FILE* fp = fopen(header, "w");
	if (!fp)
	{
		fprintf(stderr, "Failed to open header file '%s'", header);
		return -2;
	}

	rc = read(fp, font);
	if (rc != 0)
		goto fail;
	rc = write_trailer(fp);

fail:
	fclose(fp);
	return rc;
}

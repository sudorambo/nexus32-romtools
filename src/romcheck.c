/*
 * romcheck — NEXUS-32 ROM validator.
 * Usage: romcheck game.nxrom [--verbose]
 * Exit 0 = valid, 1 = errors. Rejects invalid or unsupported format with clear messages.
 */

#include "rom_validate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	const char *path = NULL;
	int verbose = 0;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0)
			verbose = 1;
		else if (!path)
			path = argv[i];
		else {
			fprintf(stderr, "romcheck: usage: romcheck <file.nxrom> [--verbose]\n");
			return 1;
		}
	}
	if (!path) {
		fprintf(stderr, "romcheck: usage: romcheck <file.nxrom> [--verbose]\n");
		return 1;
	}

	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "romcheck: error: cannot open %s\n", path);
		return 1;
	}
	fseek(f, 0, SEEK_END);
	long flen = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (flen < 0 || (unsigned long)flen > (size_t)-1) {
		fprintf(stderr, "romcheck: error: invalid file size\n");
		fclose(f);
		return 1;
	}
	size_t size = (size_t)flen;
	uint8_t *buf = malloc(size);
	if (!buf) {
		fprintf(stderr, "romcheck: error: out of memory\n");
		fclose(f);
		return 1;
	}
	if (fread(buf, 1, size, f) != size) {
		fprintf(stderr, "romcheck: error: read failed\n");
		free(buf);
		fclose(f);
		return 1;
	}
	fclose(f);

	int ret = rom_validate(buf, size, "romcheck", verbose);
	free(buf);
	if (ret != 0)
		return 1;
	if (verbose)
		printf("ROM valid.\n");
	return 0;
}

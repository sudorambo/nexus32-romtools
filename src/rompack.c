/*
 * rompack — NEXUS-32 ROM packer.
 * Mode 1: rompack -o <out.nxrom> -c <pack.toml> [--no-validate] [--compress]
 *   Manifest must have code, entry_point, title, author; optional data.
 * Mode 2: rompack -o <out.nxrom> -b <file.nxbin> [ -c pack.toml ] [--no-validate] [--compress]
 *   Code/data from .nxbin; entry_point from .nxbin; optional -c for title/author/screen/cycle_budget.
 */

#include "rom_header.h"
#include "rom_validate.h"
#include "crc32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#define NXB_MAGIC             0x0042584Eu  /* .nxbin magic (little-endian "NXB\0") */
#define PACK_CODE_PATH_SIZE   256
#define PACK_DATA_PATH_SIZE   256
#define PACK_TITLE_SIZE       32
#define PACK_AUTHOR_SIZE      32

typedef struct {
	char code_path[PACK_CODE_PATH_SIZE];
	char data_path[PACK_DATA_PATH_SIZE];
	uint32_t entry_point;
	char title[PACK_TITLE_SIZE];
	char author[PACK_AUTHOR_SIZE];
	uint32_t screen_width;
	uint32_t screen_height;
	uint32_t cycle_budget;
} pack_manifest_t;

static void manifest_init(pack_manifest_t *m)
{
	memset(m, 0, sizeof(*m));
	m->entry_point = 0x400;
	m->screen_width = 0;
	m->screen_height = 0;
	m->cycle_budget = 0;
}

/* Parse "key = value" or "key=value". Value may be quoted. Returns 1 if line was key=value. */
static int parse_line(const char *line, char *key, size_t key_size, char *val, size_t val_size)
{
	while (*line && isspace((unsigned char)*line)) line++;
	while (*line && isspace((unsigned char)*line)) line++;
	const char *k = line;
	while (*line && *line != '=' && !isspace((unsigned char)*line)) line++;
	while (*line && isspace((unsigned char)*line)) line++;
	if (*line != '=')
		return 0;
	size_t klen = (size_t)(line - k);
	while (klen > 0 && isspace((unsigned char)k[klen - 1])) klen--;
	if (klen >= key_size) klen = key_size - 1;
	memcpy(key, k, klen);
	key[klen] = '\0';
	line++;
	while (*line && isspace((unsigned char)*line)) line++;
	if (*line == '"') {
		line++;
		const char *v = line;
		while (*line && *line != '"') line++;
		size_t vlen = (size_t)(line - v);
		if (vlen >= val_size) vlen = val_size - 1;
		memcpy(val, v, vlen);
		val[vlen] = '\0';
	} else {
		const char *v = line;
		while (*line && !isspace((unsigned char)*line)) line++;
		size_t vlen = (size_t)(line - v);
		if (vlen >= val_size) vlen = val_size - 1;
		memcpy(val, v, vlen);
		val[vlen] = '\0';
	}
	return 1;
}

static uint32_t parse_uint32(const char *s)
{
	unsigned long v = 0;
	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		for (s += 2; *s; s++) {
			int c = *s;
			if (c >= '0' && c <= '9') v = v * 16 + (c - '0');
			else if (c >= 'a' && c <= 'f') v = v * 16 + (c - 'a' + 10);
			else if (c >= 'A' && c <= 'F') v = v * 16 + (c - 'A' + 10);
			else break;
		}
	} else {
		for (; *s && *s >= '0' && *s <= '9'; s++)
			v = v * 10 + (unsigned long)(*s - '0');
	}
	return (uint32_t)v;
}

/* metadata_only: 1 = only read title/author/screen/cycle (for -b mode with -c); 0 = require code path */
static int load_manifest(const char *path, pack_manifest_t *m, int metadata_only)
{
	FILE *f = fopen(path, "r");
	if (!f) {
		fprintf(stderr, "rompack: cannot open manifest %s\n", path);
		return -1;
	}
	manifest_init(m);
	char line[512];
	char key[64], val[384];
	while (fgets(line, sizeof(line), f)) {
		/* Strip comment */
		char *cmt = strchr(line, '#');
		if (cmt) *cmt = '\0';
		if (!parse_line(line, key, sizeof(key), val, sizeof(val)))
			continue;
		if (strcmp(key, "code") == 0) {
			strncpy(m->code_path, val, PACK_CODE_PATH_SIZE - 1);
			m->code_path[PACK_CODE_PATH_SIZE - 1] = '\0';
		} else if (strcmp(key, "data") == 0) {
			strncpy(m->data_path, val, PACK_DATA_PATH_SIZE - 1);
			m->data_path[PACK_DATA_PATH_SIZE - 1] = '\0';
		} else if (strcmp(key, "entry_point") == 0) {
			m->entry_point = parse_uint32(val);
		} else if (strcmp(key, "title") == 0) {
			strncpy(m->title, val, PACK_TITLE_SIZE - 1);
			m->title[PACK_TITLE_SIZE - 1] = '\0';
		} else if (strcmp(key, "author") == 0) {
			strncpy(m->author, val, PACK_AUTHOR_SIZE - 1);
			m->author[PACK_AUTHOR_SIZE - 1] = '\0';
		} else if (strcmp(key, "screen_width") == 0) {
			m->screen_width = (uint32_t)parse_uint32(val);
		} else if (strcmp(key, "screen_height") == 0) {
			m->screen_height = (uint32_t)parse_uint32(val);
		} else if (strcmp(key, "cycle_budget") == 0) {
			m->cycle_budget = parse_uint32(val);
		}
	}
	fclose(f);
	if (!metadata_only && !m->code_path[0]) {
		fprintf(stderr, "rompack: manifest must specify 'code' file\n");
		return -1;
	}
	return 0;
}

/* Read .nxbin from SDK linker: 16-byte header (magic, entry_point, code_size, data_size) then code then data */
static int load_nxbin(const char *path, uint8_t **code_buf, size_t *code_size, uint8_t **data_buf, size_t *data_size, uint32_t *entry_point)
{
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "rompack: cannot open %s\n", path);
		return -1;
	}
	uint8_t hdr[16];
	if (fread(hdr, 1, 16, f) != 16) {
		fprintf(stderr, "rompack: %s too small for .nxbin header\n", path);
		fclose(f);
		return -1;
	}
	uint32_t magic = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8) | ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
	if (magic != NXB_MAGIC) {
		fprintf(stderr, "rompack: %s not a .nxbin file (bad magic 0x%08X)\n", path, (unsigned)magic);
		fclose(f);
		return -1;
	}
	*entry_point = (uint32_t)hdr[4] | ((uint32_t)hdr[5] << 8) | ((uint32_t)hdr[6] << 16) | ((uint32_t)hdr[7] << 24);
	uint32_t cs = (uint32_t)hdr[8] | ((uint32_t)hdr[9] << 8) | ((uint32_t)hdr[10] << 16) | ((uint32_t)hdr[11] << 24);
	uint32_t ds = (uint32_t)hdr[12] | ((uint32_t)hdr[13] << 8) | ((uint32_t)hdr[14] << 16) | ((uint32_t)hdr[15] << 24);
	if (cs > CODE_SIZE_MAX || ds > DATA_SIZE_MAX) {
		fprintf(stderr, "rompack: .nxbin code/data size exceeds limits\n");
		fclose(f);
		return -1;
	}
	*code_buf = NULL;
	*data_buf = NULL;
	*code_size = cs;
	*data_size = ds;
	if (cs) {
		*code_buf = malloc(cs);
		if (!*code_buf) {
			fprintf(stderr, "rompack: out of memory\n");
			fclose(f);
			return -1;
		}
		if (fread(*code_buf, 1, cs, f) != cs) {
			fprintf(stderr, "rompack: failed to read code from %s\n", path);
			free(*code_buf);
			fclose(f);
			return -1;
		}
	}
	if (ds) {
		*data_buf = malloc(ds);
		if (!*data_buf) {
			fprintf(stderr, "rompack: out of memory\n");
			if (*code_buf) free(*code_buf);
			fclose(f);
			return -1;
		}
		if (fread(*data_buf, 1, ds, f) != ds) {
			fprintf(stderr, "rompack: failed to read data from %s\n", path);
			if (*code_buf) free(*code_buf);
			free(*data_buf);
			fclose(f);
			return -1;
		}
	}
	fclose(f);
	return 0;
}

static int read_file(const char *path, uint8_t **out_buf, size_t *out_size)
{
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "rompack: cannot open %s\n", path);
		return -1;
	}
	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (len < 0 || (unsigned long)len > ROM_SIZE_MAX) {
		fprintf(stderr, "rompack: file too large %s\n", path);
		fclose(f);
		return -1;
	}
	size_t size = (size_t)len;
	uint8_t *buf = malloc(size);
	if (!buf) {
		fprintf(stderr, "rompack: out of memory\n");
		fclose(f);
		return -1;
	}
	if (fread(buf, 1, size, f) != size) {
		fprintf(stderr, "rompack: read failed %s\n", path);
		free(buf);
		fclose(f);
		return -1;
	}
	fclose(f);
	*out_buf = buf;
	*out_size = size;
	return 0;
}

static const char usage[] =
	"rompack: usage:\n"
	"  Mode 1: rompack -o <out.nxrom> -c <pack.toml> [--no-validate] [--compress]\n"
	"  Mode 2: rompack -o <out.nxrom> -b <file.nxbin> [-c pack.toml] [--no-validate] [--compress]\n";

int main(int argc, char **argv)
{
	const char *out_path = NULL;
	const char *manifest_path = NULL;
	const char *binary_path = NULL;
	int no_validate = 0;
	int use_compress = 0;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
			out_path = argv[++i];
		else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc)
			manifest_path = argv[++i];
		else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc)
			binary_path = argv[++i];
		else if (strcmp(argv[i], "--no-validate") == 0)
			no_validate = 1;
		else if (strcmp(argv[i], "--compress") == 0)
			use_compress = 1;
		else {
			fputs(usage, stderr);
			return 1;
		}
	}
	if (!out_path) {
		fputs(usage, stderr);
		return 1;
	}
	if (binary_path && manifest_path) {
		/* -b with -c: binary mode, manifest for metadata only */
	} else if (binary_path) {
		/* -b without -c: binary mode, use defaults for metadata */
	} else if (!manifest_path) {
		fprintf(stderr, "rompack: need -c <pack.toml> (manifest mode) or -b <file.nxbin> (binary mode)\n");
		fputs(usage, stderr);
		return 1;
	}

	pack_manifest_t manifest;
	manifest_init(&manifest);
	uint8_t *code_buf = NULL;
	size_t code_size = 0;
	uint8_t *data_buf = NULL;
	size_t data_size = 0;

	if (binary_path) {
		/* Binary mode: code/data and entry_point from .nxbin */
		if (load_nxbin(binary_path, &code_buf, &code_size, &data_buf, &data_size, &manifest.entry_point) != 0)
			return 1;
		if (manifest_path) {
			if (load_manifest(manifest_path, &manifest, 1) != 0) {
				free(code_buf);
				if (data_buf) free(data_buf);
				return 1;
			}
		} else {
			strncpy(manifest.title, "NEXUS-32", PACK_TITLE_SIZE - 1);
			manifest.title[PACK_TITLE_SIZE - 1] = '\0';
			manifest.author[0] = '\0';
			manifest.screen_width = 0;
			manifest.screen_height = 0;
			manifest.cycle_budget = 0;
		}
	} else {
		/* Manifest mode: code/data paths and entry_point from pack.toml */
		if (load_manifest(manifest_path, &manifest, 0) != 0)
			return 1;
		if (read_file(manifest.code_path, &code_buf, &code_size) != 0)
			return 1;
		if (code_size > CODE_SIZE_MAX) {
			fprintf(stderr, "rompack: code size %zu exceeds 4 MB\n", code_size);
			free(code_buf);
			return 1;
		}
		if (manifest.data_path[0]) {
			if (read_file(manifest.data_path, &data_buf, &data_size) != 0) {
				free(code_buf);
				return 1;
			}
			if (data_size > DATA_SIZE_MAX) {
				fprintf(stderr, "rompack: data size %zu exceeds 4 MB\n", data_size);
				free(code_buf);
				free(data_buf);
				return 1;
			}
		}
	}

	/* Layout: header(128) + code + data + asset_table(4 bytes, num_assets=0) */
	uint32_t code_offset = NXROM_HEADER_SIZE;
	uint32_t data_offset = code_offset + (uint32_t)code_size;
	uint32_t asset_table_offset = data_offset + (uint32_t)data_size;
	uint32_t asset_table_size = 4;  /* num_assets only, 0 assets */
	size_t total_size = asset_table_offset + asset_table_size;

	if (total_size > ROM_SIZE_MAX) {
		fprintf(stderr, "rompack: total ROM size exceeds 256 MB\n");
		free(code_buf);
		if (data_buf) free(data_buf);
		return 1;
	}

	uint8_t *rom = malloc(total_size);
	if (!rom) {
		fprintf(stderr, "rompack: out of memory\n");
		free(code_buf);
		if (data_buf) free(data_buf);
		return 1;
	}
	memset(rom, 0, total_size);

	nxrom_header_t h;
	memset(&h, 0, sizeof(h));
	h.magic[0] = NXROM_MAGIC_0;
	h.magic[1] = NXROM_MAGIC_1;
	h.magic[2] = NXROM_MAGIC_2;
	h.magic[3] = NXROM_MAGIC_3;
	h.format_version = FORMAT_VERSION_1_0;
	h.flags = use_compress ? 1u : 0u;  /* Bit 0: compressed assets (per spec §9.2) */
	h.entry_point = manifest.entry_point;
	h.code_offset = code_offset;
	h.code_size = (uint32_t)code_size;
	h.data_offset = data_offset;
	h.data_size = (uint32_t)data_size;
	h.asset_table_offset = asset_table_offset;
	h.asset_table_size = asset_table_size;
	h.total_rom_size = (uint32_t)total_size;
	h.cycle_budget = manifest.cycle_budget;
	h.screen_width = (uint16_t)manifest.screen_width;
	h.screen_height = (uint16_t)manifest.screen_height;
	strncpy(h.title, manifest.title, 31);
	h.title[31] = '\0';
	strncpy(h.author, manifest.author, 31);
	h.author[31] = '\0';
	/* checksum and header_checksum computed below; _reserved already 0 */

	nxrom_header_write(&h, rom);
	memcpy(rom + code_offset, code_buf, code_size);
	if (data_buf)
		memcpy(rom + data_offset, data_buf, data_size);
	/* Asset table: num_assets = 0 (little-endian) */
	rom[asset_table_offset] = 0;
	rom[asset_table_offset + 1] = 0;
	rom[asset_table_offset + 2] = 0;
	rom[asset_table_offset + 3] = 0;

	/* Compute and write checksums */
	uint32_t hc = nxrom_header_checksum_compute(rom);
	uint32_t rc = nxrom_rom_checksum_compute(rom, total_size);
	/* Write at offsets 116 and 112 (little-endian) */
	rom[116] = (uint8_t)(hc);
	rom[117] = (uint8_t)(hc >> 8);
	rom[118] = (uint8_t)(hc >> 16);
	rom[119] = (uint8_t)(hc >> 24);
	rom[112] = (uint8_t)(rc);
	rom[113] = (uint8_t)(rc >> 8);
	rom[114] = (uint8_t)(rc >> 16);
	rom[115] = (uint8_t)(rc >> 24);

	if (!no_validate) {
		if (rom_validate(rom, total_size, "rompack", 0) != 0) {
			free(rom);
			free(code_buf);
			if (data_buf) free(data_buf);
			return 1;
		}
	}

	FILE *out = fopen(out_path, "wb");
	if (!out) {
		fprintf(stderr, "rompack: cannot write %s\n", out_path);
		free(rom);
		free(code_buf);
		if (data_buf) free(data_buf);
		return 1;
	}
	if (fwrite(rom, 1, total_size, out) != total_size) {
		fprintf(stderr, "rompack: write failed\n");
		fclose(out);
		free(rom);
		free(code_buf);
		if (data_buf) free(data_buf);
		return 1;
	}
	fclose(out);
	free(rom);
	free(code_buf);
	if (data_buf) free(data_buf);
	printf("rompack: wrote %s (%zu bytes)\n", out_path, total_size);
	return 0;
}

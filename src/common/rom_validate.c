#include "rom_validate.h"
#include "asset_dir.h"
#include "rom_header.h"
#include "crc32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROM_VALIDATE_MAX_ASSETS 65536

static const char *v_prefix;
static int v_verbose;

#define ERR(...) do { \
	if (v_prefix) fprintf(stderr, "%s: error: ", v_prefix); \
	else fprintf(stderr, "error: "); \
	fprintf(stderr, __VA_ARGS__); \
	return -1; \
} while (0)

static void msg(const char *s)
{
	if (v_verbose)
		printf("  %s\n", s);
}

static int check_segment_in_file(size_t file_size, uint32_t offset, uint32_t size, const char *name)
{
	if (offset > file_size) {
		ERR("%s offset 0x%X beyond file size %zu\n", name, (unsigned)offset, file_size);
	}
	if (size > file_size - offset) {
		ERR("%s extends past end of file (offset 0x%X size %u, file %zu)\n",
			name, (unsigned)offset, (unsigned)size, file_size);
	}
	return 1;
}

static int ranges_overlap(uint32_t a_start, uint32_t a_len, uint32_t b_start, uint32_t b_len)
{
	if (a_len == 0 || b_len == 0)
		return 0;
	uint32_t a_end = a_start + a_len;
	uint32_t b_end = b_start + b_len;
	return !(a_end <= b_start || b_end <= a_start);
}

int rom_validate(const uint8_t *buf, size_t size, const char *prefix, int verbose)
{
	nxrom_header_t h;
	asset_entry_t entries[ROM_VALIDATE_MAX_ASSETS];

	v_prefix = prefix;
	v_verbose = verbose;

	if (size < NXROM_HEADER_SIZE) {
		ERR("file too small for header (%zu < %u)\n", size, (unsigned)NXROM_HEADER_SIZE);
	}

	if (nxrom_header_read(buf, &h) != 0) {
		ERR("failed to read header\n");
	}

	if (!nxrom_header_magic_ok(&h)) {
		ERR("invalid magic (expected NX32, got %02X %02X %02X %02X)\n",
			h.magic[0], h.magic[1], h.magic[2], h.magic[3]);
	}
	msg("magic OK");

	if (h.format_version != FORMAT_VERSION_1_0) {
		ERR("unsupported format_version 0x%04X (this tool supports 0x%04X)\n",
			(unsigned)h.format_version, (unsigned)FORMAT_VERSION_1_0);
	}
	msg("format_version 0x0100 OK");

	if (!nxrom_header_reserved_ok(&h)) {
		ERR("reserved bytes 120-127 must be zero\n");
	}
	msg("reserved bytes OK");

	if (h.total_rom_size != size) {
		ERR("total_rom_size in header (%u) does not match file size (%zu)\n",
			(unsigned)h.total_rom_size, size);
	}
	msg("total_rom_size matches file");

	if (h.code_size > CODE_SIZE_MAX) {
		ERR("code_size %u exceeds limit 4 MB\n", (unsigned)h.code_size);
	}
	if (h.data_size > DATA_SIZE_MAX) {
		ERR("data_size %u exceeds limit 4 MB\n", (unsigned)h.data_size);
	}
	if (h.total_rom_size > ROM_SIZE_MAX) {
		ERR("total_rom_size %u exceeds limit 256 MB\n", (unsigned)h.total_rom_size);
	}
	msg("segment limits OK");

	if (!check_segment_in_file(size, h.code_offset, h.code_size, "code segment"))
		return -1;
	if (!check_segment_in_file(size, h.data_offset, h.data_size, "data segment"))
		return -1;
	if (!check_segment_in_file(size, h.asset_table_offset, h.asset_table_size, "asset table"))
		return -1;
	msg("segment bounds OK");

	if (ranges_overlap(h.code_offset, h.code_size, h.data_offset, h.data_size)) {
		ERR("code and data segments overlap\n");
	}
	if (ranges_overlap(h.code_offset, h.code_size, h.asset_table_offset, h.asset_table_size)) {
		ERR("code and asset table overlap\n");
	}
	if (ranges_overlap(h.data_offset, h.data_size, h.asset_table_offset, h.asset_table_size)) {
		ERR("data and asset table overlap\n");
	}
	msg("no segment overlap");

	if (h.entry_point >= MAIN_RAM_SIZE) {
		ERR("entry_point 0x%X outside Main RAM (max 0x%X)\n",
			(unsigned)h.entry_point, (unsigned)(MAIN_RAM_SIZE - 1));
	}
	msg("entry_point in Main RAM");

	/* Optional: reject trap/halt as first instruction (spec §11.2). S-Type opcode 0x3F; SYSCALL func 0, BREAK func 1. */
	if (h.code_size >= 4 && h.code_offset + 4 <= size) {
		uint32_t first_word = (uint32_t)buf[h.code_offset] |
			((uint32_t)buf[h.code_offset + 1] << 8) |
			((uint32_t)buf[h.code_offset + 2] << 16) |
			((uint32_t)buf[h.code_offset + 3] << 24);
		uint32_t opcode = first_word >> 26;
		uint32_t sfunc = (first_word >> 21) & 0x1F;
		if (opcode == 0x3F && (sfunc == 0 || sfunc == 1)) {
			ERR("first instruction at entry is SYSCALL/BREAK (trap/halt)\n");
		}
	}
	msg("first instruction sanity OK");

	uint32_t header_crc = nxrom_header_checksum_verify(buf);
	if (header_crc != h.header_checksum) {
		ERR("header_checksum mismatch (computed 0x%08X, stored 0x%08X)\n",
			(unsigned)header_crc, (unsigned)h.header_checksum);
	}
	msg("header_checksum OK");

	uint32_t rom_crc = nxrom_rom_checksum_compute(buf, size);
	if (rom_crc != h.checksum) {
		ERR("checksum mismatch (computed 0x%08X, stored 0x%08X)\n",
			(unsigned)rom_crc, (unsigned)h.checksum);
	}
	msg("checksum OK");

	int n_assets = asset_dir_parse(buf, size,
		h.asset_table_offset, h.asset_table_size,
		entries, ROM_VALIDATE_MAX_ASSETS);
	if (n_assets < 0) {
		ERR("invalid asset directory (parse failed)\n");
	}
	msg("asset directory parsed");

	if (n_assets > 0) {
		const char *errmsg = NULL;
		if (asset_dir_validate(buf, size,
				h.asset_table_offset, h.asset_table_size,
				h.code_offset, h.code_size,
				h.data_offset, h.data_size,
				entries, (unsigned)n_assets, &errmsg) != 0) {
			ERR("%s\n", errmsg ? errmsg : "asset directory validation failed");
		}
		msg("asset entries valid");
	}

	return 0;
}

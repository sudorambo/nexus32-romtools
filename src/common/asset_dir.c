#include "asset_dir.h"
#include "rom_header.h"
#include <string.h>

static uint32_t read_u32_le(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int asset_dir_parse(const uint8_t *rom_buf, size_t rom_size,
	uint32_t table_offset, uint32_t table_size,
	asset_entry_t *entries_out, unsigned max_entries)
{
	if (table_size < 4)
		return -1;
	if (table_offset + table_size > rom_size)
		return -1;
	const uint8_t *p = rom_buf + table_offset;
	uint32_t num_assets = read_u32_le(p);
	uint32_t required = 4 + num_assets * ASSET_ENTRY_SIZE;
	if (required > table_size || num_assets > max_entries)
		return -1;
	p += 4;
	for (uint32_t i = 0; i < num_assets; i++) {
		memcpy(entries_out[i].name, p, 32);
		entries_out[i].asset_type = read_u32_le(p + 32);
		entries_out[i].format = read_u32_le(p + 36);
		entries_out[i].rom_offset = read_u32_le(p + 40);
		entries_out[i].compressed_size = read_u32_le(p + 44);
		entries_out[i].uncompressed_size = read_u32_le(p + 48);
		entries_out[i].target_region = read_u32_le(p + 52);
		entries_out[i].target_address = read_u32_le(p + 56);
		p += ASSET_ENTRY_SIZE;
	}
	return (int)num_assets;
}

static int ranges_overlap(uint32_t a_start, uint32_t a_len, uint32_t b_start, uint32_t b_len)
{
	if (a_len == 0 || b_len == 0)
		return 0;
	uint32_t a_end = a_start + a_len;
	uint32_t b_end = b_start + b_len;
	return !(a_end <= b_start || b_end <= a_start);
}

int asset_dir_validate(const uint8_t *rom_buf, size_t rom_size,
	uint32_t table_offset, uint32_t table_size,
	uint32_t code_offset, uint32_t code_size,
	uint32_t data_offset, uint32_t data_size,
	const asset_entry_t *entries, unsigned num_entries,
	const char **errmsg)
{
	(void)rom_buf;
	if (errmsg)
		*errmsg = NULL;
	for (unsigned i = 0; i < num_entries; i++) {
		uint32_t off = entries[i].rom_offset;
		uint32_t len = entries[i].compressed_size;
		if (off + len > rom_size) {
			if (errmsg)
				*errmsg = "asset data extends past end of ROM";
			return -1;
		}
		if (off < NXROM_HEADER_SIZE) {
			if (errmsg)
				*errmsg = "asset data overlaps header";
			return -1;
		}
		if (ranges_overlap(off, len, code_offset, code_size)) {
			if (errmsg)
				*errmsg = "asset data overlaps code segment";
			return -1;
		}
		if (ranges_overlap(off, len, data_offset, data_size)) {
			if (errmsg)
				*errmsg = "asset data overlaps data segment";
			return -1;
		}
		if (ranges_overlap(off, len, table_offset, table_size)) {
			if (errmsg)
				*errmsg = "asset data overlaps asset table";
			return -1;
		}
		for (unsigned j = i + 1; j < num_entries; j++) {
			if (ranges_overlap(off, len, entries[j].rom_offset, entries[j].compressed_size)) {
				if (errmsg)
					*errmsg = "asset data segments overlap";
				return -1;
			}
		}
	}
	return 0;
}

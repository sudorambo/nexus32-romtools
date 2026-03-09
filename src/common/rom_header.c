#include "rom_header.h"
#include "crc32.h"
#include <stddef.h>
#include <string.h>

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif

typedef struct {
	uint8_t  magic[4];
	uint16_t format_version;
	uint16_t flags;
	uint32_t entry_point;
	uint32_t code_offset;
	uint32_t code_size;
	uint32_t data_offset;
	uint32_t data_size;
	uint32_t asset_table_offset;
	uint32_t asset_table_size;
	uint32_t total_rom_size;
	uint32_t cycle_budget;
	uint16_t screen_width;
	uint16_t screen_height;
	char     title[32];
	char     author[32];
	uint32_t checksum;
	uint32_t header_checksum;
	uint8_t  _reserved[8];
}
#if defined(__GNUC__) || defined(__clang__)
__attribute__((packed))
#endif
nxrom_header_raw_t;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

int nxrom_header_read(const uint8_t *buf, nxrom_header_t *out)
{
	const nxrom_header_raw_t *raw = (const nxrom_header_raw_t *)buf;
	memcpy(out->magic, raw->magic, 4);
	out->format_version = raw->format_version;
	out->flags = raw->flags;
	out->entry_point = raw->entry_point;
	out->code_offset = raw->code_offset;
	out->code_size = raw->code_size;
	out->data_offset = raw->data_offset;
	out->data_size = raw->data_size;
	out->asset_table_offset = raw->asset_table_offset;
	out->asset_table_size = raw->asset_table_size;
	out->total_rom_size = raw->total_rom_size;
	out->cycle_budget = raw->cycle_budget;
	out->screen_width = raw->screen_width;
	out->screen_height = raw->screen_height;
	memcpy(out->title, raw->title, 32);
	memcpy(out->author, raw->author, 32);
	out->checksum = raw->checksum;
	out->header_checksum = raw->header_checksum;
	memcpy(out->_reserved, raw->_reserved, 8);
	return 0;
}

void nxrom_header_write(const nxrom_header_t *h, uint8_t *buf)
{
	nxrom_header_raw_t *raw = (nxrom_header_raw_t *)buf;
	memcpy(raw->magic, h->magic, 4);
	raw->format_version = h->format_version;
	raw->flags = h->flags;
	raw->entry_point = h->entry_point;
	raw->code_offset = h->code_offset;
	raw->code_size = h->code_size;
	raw->data_offset = h->data_offset;
	raw->data_size = h->data_size;
	raw->asset_table_offset = h->asset_table_offset;
	raw->asset_table_size = h->asset_table_size;
	raw->total_rom_size = h->total_rom_size;
	raw->cycle_budget = h->cycle_budget;
	raw->screen_width = h->screen_width;
	raw->screen_height = h->screen_height;
	memcpy(raw->title, h->title, 32);
	memcpy(raw->author, h->author, 32);
	raw->checksum = h->checksum;
	raw->header_checksum = h->header_checksum;
	memcpy(raw->_reserved, h->_reserved, 8);
}

int nxrom_header_magic_ok(const nxrom_header_t *h)
{
	return h->magic[0] == NXROM_MAGIC_0 && h->magic[1] == NXROM_MAGIC_1 &&
	       h->magic[2] == NXROM_MAGIC_2 && h->magic[3] == NXROM_MAGIC_3;
}

int nxrom_header_reserved_ok(const nxrom_header_t *h)
{
	for (int i = 0; i < 8; i++)
		if (h->_reserved[i] != 0)
			return 0;
	return 1;
}

/* Compute header_checksum (CRC32 of header bytes 0-119). */
uint32_t nxrom_header_checksum_compute(const uint8_t *header_buf)
{
	return crc32(header_buf, 120);
}

/* Verify: compute over header with bytes 112-119 treated as zero. */
uint32_t nxrom_header_checksum_verify(const uint8_t *header_buf)
{
	uint8_t tmp[120];
	memcpy(tmp, header_buf, 120);
	memset(tmp + 112, 0, 8);
	return crc32(tmp, 120);
}

/* Compute full ROM checksum (CRC32 of ROM excluding bytes 112-119). */
uint32_t nxrom_rom_checksum_compute(const uint8_t *rom, size_t rom_size)
{
	uint32_t crc = 0;
	crc = crc32_update(crc, rom, 112);
	if (rom_size > 120)
		crc = crc32_update(crc, rom + 120, (unsigned long)(rom_size - 120));
	return crc;
}

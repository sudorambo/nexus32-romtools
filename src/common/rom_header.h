#ifndef NEXUS32_ROM_HEADER_H
#define NEXUS32_ROM_HEADER_H

#include <stddef.h>
#include <stdint.h>

#define NXROM_MAGIC_0 0x4E  /* 'N' */
#define NXROM_MAGIC_1 0x58  /* 'X' */
#define NXROM_MAGIC_2 0x33  /* '3' */
#define NXROM_MAGIC_3 0x32  /* '2' */

#define NXROM_HEADER_SIZE 128u

#define FORMAT_VERSION_1_0  0x0100u

#define CODE_SIZE_MAX  (4u * 1024u * 1024u)   /* 4 MB */
#define DATA_SIZE_MAX  (4u * 1024u * 1024u)   /* 4 MB */
#define ROM_SIZE_MAX   (256u * 1024u * 1024u) /* 256 MB */

#define MAIN_RAM_SIZE  0x02000000u  /* 32 MB, valid entry_point range */

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
} nxrom_header_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Read header from buffer (at least NXROM_HEADER_SIZE bytes). Returns 0 on success. */
int nxrom_header_read(const uint8_t *buf, nxrom_header_t *out);

/* Write header to buffer (at least NXROM_HEADER_SIZE bytes). Does not compute checksums. */
void nxrom_header_write(const nxrom_header_t *h, uint8_t *buf);

/* Check magic bytes. Returns 1 if valid "NX32". */
int nxrom_header_magic_ok(const nxrom_header_t *h);

/* Check reserved bytes 120-127 are zero. Returns 1 if valid. */
int nxrom_header_reserved_ok(const nxrom_header_t *h);

/* Compute header_checksum (CRC32 of header bytes 0-119). buf must be at least 120 bytes.
 * When building, bytes 112-119 should be zero. */
uint32_t nxrom_header_checksum_compute(const uint8_t *header_buf);

/* Verify header_checksum: same as compute but treats bytes 112-119 as zero (for verification). */
uint32_t nxrom_header_checksum_verify(const uint8_t *header_buf);

/* Compute full ROM checksum (CRC32 of ROM excluding bytes 112-119). */
uint32_t nxrom_rom_checksum_compute(const uint8_t *rom, size_t rom_size);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS32_ROM_HEADER_H */

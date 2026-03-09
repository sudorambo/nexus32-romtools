#ifndef NEXUS32_ASSET_DIR_H
#define NEXUS32_ASSET_DIR_H

#include <stddef.h>
#include <stdint.h>

#define ASSET_ENTRY_SIZE 64u

/* Asset types per spec §9.5 / §9.6 */
#define ASSET_TYPE_TEXTURE 0
#define ASSET_TYPE_MESH    1
#define ASSET_TYPE_AUDIO  2
#define ASSET_TYPE_TILEMAP 3
#define ASSET_TYPE_SHADER 4
#define ASSET_TYPE_GENERIC 5

/* Target regions */
#define TARGET_MAIN_RAM  0
#define TARGET_VRAM      1
#define TARGET_AUDIO_RAM 2

typedef struct {
	char     name[32];
	uint32_t asset_type;
	uint32_t format;
	uint32_t rom_offset;
	uint32_t compressed_size;
	uint32_t uncompressed_size;
	uint32_t target_region;
	uint32_t target_address;
} asset_entry_t;

typedef struct {
	uint32_t num_assets;
	asset_entry_t *entries;  /* num_assets entries; caller allocates or use asset_dir_parse */
} asset_directory_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Parse asset directory from ROM buffer. rom_buf = full ROM, table_offset/size from header.
 * entries_out: pointer to array of asset_entry_t (caller allocates, or use asset_dir_parse_alloc).
 * max_entries: capacity of entries_out.
 * Returns number of entries parsed, or -1 on error (overflow, out of bounds). */
int asset_dir_parse(const uint8_t *rom_buf, size_t rom_size,
	uint32_t table_offset, uint32_t table_size,
	asset_entry_t *entries_out, unsigned max_entries);

/* Validate asset directory: each entry's rom_offset and compressed_size must be within ROM,
 * and segments must not overlap each other or header/code/data/table. Returns 0 on success. */
int asset_dir_validate(const uint8_t *rom_buf, size_t rom_size,
	uint32_t table_offset, uint32_t table_size,
	uint32_t code_offset, uint32_t code_size,
	uint32_t data_offset, uint32_t data_size,
	const asset_entry_t *entries, unsigned num_entries,
	const char **errmsg);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS32_ASSET_DIR_H */

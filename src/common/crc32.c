#include "crc32.h"
#include <stddef.h>

static uint32_t crc32_table[256];
static int crc32_table_initialized;

static void crc32_init_table(void)
{
	for (uint32_t i = 0; i < 256; i++) {
		uint32_t c = i;
		for (int k = 0; k < 8; k++)
			c = (c >> 1) ^ (0xEDB88320u & -(uint32_t)(c & 1));
		crc32_table[i] = c;
	}
	crc32_table_initialized = 1;
}

uint32_t crc32_update(uint32_t crc, const void *data, unsigned long len)
{
	const uint8_t *p = (const uint8_t *)data;
	if (!crc32_table_initialized)
		crc32_init_table();
	crc ^= 0xFFFFFFFFu;
	while (len--) {
		crc = crc32_table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
	}
	return crc ^ 0xFFFFFFFFu;
}

uint32_t crc32(const void *data, unsigned long len)
{
	return crc32_update(0, data, len);
}
